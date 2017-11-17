/*
	lime_helloworld-tester.cpp
	Copyright (C) 2017  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define BCTBX_LOG_DOMAIN "lime-tester"
#include <bctoolbox/logging.h>

#include "lime/lime.hpp"
#include "lime-tester.hpp"
#include "lime-tester-utils.hpp"

#include <bctoolbox/tester.h>
#include <bctoolbox/exception.hh>
#include <belle-sip/belle-sip.h>

using namespace::std;
using namespace::lime;

extern int wait_for_timeout;
extern std::string test_x3dh_server_url;
extern std::string test_x3dh_c25519_server_port;
extern std::string test_x3dh_c448_server_port;

static belle_sip_stack_t *stack=NULL;
static belle_http_provider_t *prov=NULL;


static int http_before_all(void) {
	stack=belle_sip_stack_new(NULL);

	prov=belle_sip_stack_create_http_provider(stack,"0.0.0.0");

	belle_tls_crypto_config_t *crypto_config=belle_tls_crypto_config_new();

	belle_tls_crypto_config_set_root_ca(crypto_config,std::string(bc_tester_get_resource_dir_prefix()).append("/data/").data());
	belle_http_provider_set_tls_crypto_config(prov,crypto_config);
	belle_sip_object_unref(crypto_config);
	return 0;
}

static int http_after_all(void) {
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);
	return 0;
}

// this emulate a network transmission: bob got a mailbox(2 buffers actually) where we can post/retrieve data to/from
static std::vector<uint8_t> bobCipherHeaderMailbox{};
static std::vector<uint8_t> bobCipherMessageMailbox{};
static void sendMessageTo(std::string recipient, std::vector<uint8_t> &cipherHeader, std::vector<uint8_t> &cipherMessage) {
	if (recipient == "bob") {
		bobCipherHeaderMailbox = cipherHeader;
		bobCipherMessageMailbox = cipherMessage;
		return;
	}

	BCTBX_SLOGE<<"sending a message to unknown user "<<recipient;
	BC_FAIL();
}

static void getMessageFor(std::string recipient, std::vector<uint8_t> &cipherHeader, std::vector<uint8_t> &cipherMessage) {
	if (recipient == "bob") {
		cipherHeader = bobCipherHeaderMailbox;
		cipherMessage = bobCipherMessageMailbox;
		return;
	}

	BCTBX_SLOGE<<"getting a message to unknown user "<<recipient;
	BC_FAIL();
}


 /* Basic usage scenario
 * - Alice and Bob register themselves on X3DH server(use randomized device Ids to allow test server to run several test in parallel)
 * - Alice encrypt a message for Bob (this will fetch Bob's key from server)
 * - Bob decrypt alice message
 *
 *   @param[in] curve		Lime can run with cryptographic operations based on curve25519 or curve448, set by this parameter in this test.
 *   				One X3DH server runs on one type of key and all clients must use the same
 *   @param[in]	dbBaseFilename	The local filename for users will be this base.<alice/bob>.<curve type>.sqlite3
 *   @param[x3dh_server_url]	The URL (including port) of the X3DH server
 */
static void helloworld_basic_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url) {
	// users databases names: baseFilename.<alice/bob>.<curve id>.sqlite3
	std::string dbFilenameAlice{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameBob.append(".bob.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	events_counters_t counters={};
	int expected_success=0;

	// This function is used as a lambda for creation/deletion of user, not of lot of interest
	// The counters part is for test synchronisation purpose
	// The returnCode gives the status of command execution.
	// Encryption make use of a lambda too but it's written directly in the call, see below
	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});

	try {
		// create Managers : they will open/create the database given in first parameter, and use the http provider given in second one to communicate with server.
		// Any application using Lime shall instantiate one LimeManager only, even in case of multiple users managed by the application.
		auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, prov));
		auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, prov));

		// create Random devices names (in case we use a shared test server, devices id shall be the GRUU, X3DH/Lime does not connect user(sip:uri) and device(gruu)
		// From Lime perspective, only devices exists and they must be uniquely identifies on the X3DH server.
		auto aliceDeviceId = makeRandomDeviceName("alice.");
		auto bobDeviceId = makeRandomDeviceName("bob.");

		// create users, this operation is asynchronous(as the user is also created on X3DH server)
		// Last parameter is a callback acceptiong as parameters a return code and a string
		//      - In case of successfull operation the return code is lime::callbackReturn::success, and string is empty
		//      - In case of failure, the return code is lime::callbackReturn::fail and the string shall give details on the failure cause
		aliceManager->create_user(*aliceDeviceId, x3dh_server_url, curve, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,++expected_success,wait_for_timeout)); // we must get a callback saying all went well
		bobManager->create_user(*bobDeviceId, x3dh_server_url, curve, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,++expected_success,wait_for_timeout)); // we must get a callback saying all went well


		/*** alice encrypt a message to bob, all parameters given to encrypt function are shared_ptr. ***/
		// The encryption generates:
		//      - one common cipher message which must be sent to all recipient devices
		//      - a cipher header per recipient device, each recipient device shall receive its specific one

		// Create an empty recipientData vector, in this basic case we will encrypt to one device only but we can do it to any number of recipient devices.
		// recipientData holds:
		//      - recipient device id (identify the recipient)
		//      - cipherHeader : output of encryption process targeted to this recipient device only
		auto recipients = make_shared<std::vector<recipientData>>();
		recipients->emplace_back(*bobDeviceId); // we have only one recipient identified by its device id.
		//Shall we have more recipients(bob can have several devices or be a conference sip:uri, alice other devices must get a copy of the message), we just need to emplace_back some more recipients Device Id(GRUU)

		// the plain message, type is std::vector<uint8_t> as it can be text as in this test but also any kind of data.
		auto message = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[0].begin(), lime_messages_pattern[0].end());
		auto cipherMessage = make_shared<std::vector<uint8_t>>(); // an empty buffer to get the encrypted message

		/************** SENDER SIDE CODE *****************************/
		// encrypt, parameters are:
		//      - localDeviceId to select which of the users managed by the LimeManager we shall use to perform the encryption(in our example we have only one local device). This one doesn't need to be a shared pointer.
		//      - recipientUser: an id of the recipient user(which can hold several devices), typically its sip:uri
		//      - recipientData vector(see above), list all recipient devices, will hold their cipher header
		//      - plain message
		//      - cipher message (this one must then be distributed to all recipients devices)
		//      - a callback (prototype: void(lime::callbackReturn, std::string))
		aliceManager->encrypt(*aliceDeviceId, make_shared<const std::string>("bob"), recipients, message, cipherMessage,
					// lambda to get the results, it captures :
					// - counter : relative to the test, real application won't need this, it's local and used to wait for this macro so can't be destroyed before the call to this closure
					// - recipients :  It will hold the same list of deviceIds we set as input with their corresponding cipherHeader.
					// - cipherMessage : It will hold the cipher message to be sent to all recipients devices.
					// IMPORTANT : recipients and cipherMessage are captured by copy not reference. They are shared_ptr, their original scope is likely to be the function where the encrypt is called.
					//             they shall then be destroyed when getting out of this function and thus won't be valid anymore when this closure is called. By getting a copy we just increase their
					//             use count and are sure to still have them valid when we are called.
					//             It may be wise to use weak_ptr instead of shared ones so if any problem occurs resulting in callback never being called, it won't held this buffer from being destroyed
					//             In normal operation, the shared_ptr given to encrypt function is internally owned at least until the callback is called.
					[&counters,
					recipients, cipherMessage](lime::callbackReturn returnCode, std::string errorMessage){
						// counters is related to this test environment only, not to be considered for real usage
						if (returnCode == lime::callbackReturn::success) {
							counters.operation_success++;
							// here is the code processing the output when all went well.
							// Send the message to recipient
							// that function must, before returning, send or copy the data to send them later
							// recipients and cipherMessage are likely to be be destroyed as soon as we get out of this closure
							// In this exanple we know that bodDevice is in recipients[0], real code shall loop on recipients vector
							sendMessageTo("bob", (*recipients)[0].cipherHeader, *cipherMessage);
						} else {
							counters.operation_failed++;
							// The encryption failed.
							BCTBX_SLOGE<<"Lime operation failed : "<<errorMessage;
						}
					});

		// in real sending situation, the local instance of the shared pointer are destroyed by exiting the function where they've been declared
		// and where we called the encrypt function. (The LimeManager shall instead never be destroyed until the application terminates)
		recipients = nullptr;
		cipherMessage = nullptr;
		/****** end of SENDER SIDE CODE ******************************/

		/************** SYNCHRO **************************************/
		// this is just waiting for the callback to increase the operation_success field in counters
		// sending ticks to the belle-sip stack in order to process messages
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,++expected_success,wait_for_timeout));
		/****** end of  SYNCHRO **************************************/


		/************** RECIPIENT SIDE CODE **************************/
		// retrieve message, in real situation the server shall fan-out only the part we need or client shall parse in the cipherHeaders to retrieve the one addressed to him.
		std::vector<uint8_t> bobReceivedCipherHeader{};
		std::vector<uint8_t> bobReceivedCipherMessage{};
		getMessageFor("bob", bobReceivedCipherHeader, bobReceivedCipherMessage);

		std::vector<uint8_t> plainTextMessage{}; // a data vector to store the decrypted message
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, bobReceivedCipherHeader, bobReceivedCipherMessage, plainTextMessage));

		// it's a text message, so turn it into a string and compare with the one alice sent
		std::string plainTextMessageString{plainTextMessage.begin(), plainTextMessage.end()};
		BC_ASSERT_TRUE(plainTextMessageString == lime_messages_pattern[0]);
		/******* end of RECIPIENT SIDE CODE **************************/

		// delete the users
		if (cleanDatabase) {
			aliceManager->delete_user(*aliceDeviceId, callback);
			bobManager->delete_user(*bobDeviceId, callback);
			BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success+2,wait_for_timeout)); // we must get a callback saying all went well
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}
}

static void helloworld_basic(void) {
	// run the test on Curve25519 and Curve448 based encryption if available
#ifdef EC25519_ENABLED
	helloworld_basic_test(lime::CurveId::c25519, "helloworld_basic", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
#endif
#ifdef EC448_ENABLED
	helloworld_basic_test(lime::CurveId::c448, "helloworld_basic", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
#endif
}

static test_t tests[] = {
	TEST_NO_TAG("Basic", helloworld_basic),
};

test_suite_t lime_helloworld_test_suite = {
	"Hello World",
	http_before_all,
	http_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
