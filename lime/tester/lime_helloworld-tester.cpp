/*
	lime_helloworld-tester.cpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2017  Belledonne Communications SARL

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

#include "lime_log.hpp"
#include "lime/lime.hpp"
#include "lime-tester.hpp"
#include "lime-tester-utils.hpp"

#include <bctoolbox/tester.h>
#include <bctoolbox/exception.hh>
#include <belle-sip/belle-sip.h>

using namespace::std;
using namespace::lime;

static belle_sip_stack_t *bc_stack=NULL;
static belle_http_provider_t *prov=NULL;


static int http_before_all(void) {
	bc_stack=belle_sip_stack_new(NULL);

	prov=belle_sip_stack_create_http_provider(bc_stack,"0.0.0.0");

	belle_tls_crypto_config_t *crypto_config=belle_tls_crypto_config_new();

	belle_tls_crypto_config_set_root_ca(crypto_config,std::string(bc_tester_get_resource_dir_prefix()).append("/data/").data());
	belle_http_provider_set_tls_crypto_config(prov,crypto_config);
	belle_sip_object_unref(crypto_config);
	return 0;
}

static int http_after_all(void) {
	belle_sip_object_unref(prov);
	belle_sip_object_unref(bc_stack);
	return 0;
}

// this emulate a network transmission: bob got a mailbox (2 buffers actually) where we can post/retrieve data to/from
static std::vector<uint8_t> bobDRmessageMailbox{};
static std::vector<uint8_t> bobCipherMessageMailbox{};
static void sendMessageTo(std::string recipient, std::vector<uint8_t> &DRmessage, std::vector<uint8_t> &cipherMessage) {
	if (recipient == "bob") {
		bobDRmessageMailbox = DRmessage;
		bobCipherMessageMailbox = cipherMessage;
		return;
	}

	LIME_LOGE<<"sending a message to unknown user "<<recipient;
	BC_FAIL("");
}

static void getMessageFor(std::string recipient, std::vector<uint8_t> &DRmessage, std::vector<uint8_t> &cipherMessage) {
	if (recipient == "bob") {
		DRmessage = bobDRmessageMailbox;
		cipherMessage = bobCipherMessageMailbox;
		return;
	}

	LIME_LOGE<<"getting a message to unknown user "<<recipient;
	BC_FAIL("");
}

struct C_Callback_userData {
	const limeX3DHServerResponseProcess responseProcess; // a callback to forward the response to lib lime
	const std::string username; // the username to provide corresponding credentials, not really in use in this test as the test server let us access any record with the same credentials
	C_Callback_userData(const limeX3DHServerResponseProcess &response, const std::string &username) : responseProcess(response), username{username} {};
};

static void process_auth_requested (void *data, belle_sip_auth_event_t *event){
	// Useless code but just for the example: we shall get the username from our callback user data
	C_Callback_userData *userData = static_cast<C_Callback_userData *>(data);
	// and set it as username to retrieve the correct credentials and send them back
	LIME_LOGI<<"Accessing credentials for user "<<std::string(userData->username.data());

	// for test purpose we use a server which accept commands in name of any user without credentials
	// just do nothing here while we shall put password and username
	// belle_sip_auth_event_set_username(event, <place here the username>);
	// belle_sip_auth_event_set_passwd(event, <place here the user password>);
}

static void process_io_error(void *data, const belle_sip_io_error_event_t *event) noexcept{
	C_Callback_userData *userData = static_cast<C_Callback_userData *>(data);
	(userData->responseProcess)(0, std::vector<uint8_t>{});
	delete(userData);
}

static void process_response(void *data, const belle_http_response_event_t *event) noexcept {
	C_Callback_userData *userData = static_cast<C_Callback_userData *>(data);
	if (event->response){
		auto code=belle_http_response_get_status_code(event->response);
		belle_sip_message_t *message = BELLE_SIP_MESSAGE(event->response);
		// all raw data access functions in lime use uint8_t *, so safely cast the body pointer to it, it's just a data stream pointer anyway
		auto body = reinterpret_cast<const uint8_t *>(belle_sip_message_get_body(message));
		auto bodySize = belle_sip_message_get_body_size(message);
		(userData->responseProcess)(code, std::vector<uint8_t>{body, body+bodySize});
	} else {
		(userData->responseProcess)(0, std::vector<uint8_t>{});
	}
	delete(userData);
}

/** @brief Post data to X3DH server.
 * Communication with X3DH server is entirely managed out of the lib lime, in this example code it is performed over HTTPS provided by belle-sip
 * Here the HTTPS stack provider prov is a static variable in global context so there is no need to capture it, it may be the case in real usage
 * This lambda prototype is defined in lime.hpp
 *
 * @param[in] url		The URL of X3DH server
 * @param[in] from		The local device id, used to identify user on the X3DH server, user identification and credential verification is out of lib lime scope.
 * 				Here identification is performed on test server via belle-sip authentication mechanism and providing the test user credentials
 * @param[in] message		The data to be sent to the X3DH server
 * @param[in] responseProcess	The function to be called when response from server arrives. Function prototype is defined in lime.hpp: (void)(int responseCode, std::vector<uint8_t>response)
 */
static limeX3DHServerPostData X3DHServerPost([](const std::string &url, const std::string &from, const std::vector<uint8_t> &message, const limeX3DHServerResponseProcess &responseProcess){
	belle_http_request_listener_callbacks_t cbs;
	belle_http_request_listener_t *l;
	belle_generic_uri_t *uri;
	belle_http_request_t *req;
	belle_sip_memory_body_handler_t *bh;

	memset(&cbs,0,sizeof(belle_http_request_listener_callbacks_t));

	bh = belle_sip_memory_body_handler_new_copy_from_buffer(message.data(), message.size(), NULL, NULL);

	uri=belle_generic_uri_parse(url.data());

	req=belle_http_request_create("POST",
			uri,
			belle_http_header_create("User-Agent", "lime"),
			belle_http_header_create("Content-type", "x3dh/octet-stream"),
			belle_http_header_create("X-Lime-user-identity", from.data()),
			NULL);

	belle_sip_message_set_body_handler(BELLE_SIP_MESSAGE(req),BELLE_SIP_BODY_HANDLER(bh));
	cbs.process_response=process_response;
	cbs.process_io_error=process_io_error;
	cbs.process_auth_requested=process_auth_requested;
	// store a reference to the responseProcess function in a wrapper as belle-sip request C-style callbacks with a void * user data parameter, C++ implementation shall
	// use lambda and capture the function.
	// this new creates on the heap a copy of the responseProcess closure, so we have access to it when called back by belle-sip
	// We also provide the username to be used to retrieve credentials when server ask for it
	C_Callback_userData *userData = new C_Callback_userData(responseProcess, from);
	l=belle_http_request_listener_create_from_callbacks(&cbs, userData);
	belle_sip_object_data_set(BELLE_SIP_OBJECT(req), "http_request_listener", l, belle_sip_object_unref); // Ensure the listener object is destroyed when the request is destroyed
	belle_http_provider_send_request(prov,req,l);
});


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
	dbFilenameAlice.append(".alice.").append(lime_tester::curveId(curve)).append(".sqlite3");
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameBob.append(".bob.").append(lime_tester::curveId(curve)).append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	// This function is used as a lambda for creation/deletion of user, not of lot of interest
	// The counters part is for test synchronisation purpose
	// The returnCode gives the status of command execution.
	// Encryption make use of a lambda too but it's written directly in the call, see below
	limeCallback callback([&counters](lime::CallbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::CallbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						LIME_LOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});

	try {
		LIME_LOGI<<"Create alice and bob LimeManagers"<<endl;
		// create Random devices names (in case we use a shared test server, devices id shall be the GRUU, X3DH/Lime does not connect user (sip:uri) and device (gruu)
		// From Lime perspective, only devices exists and they must be uniquely identifies on the X3DH server.
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.");
		auto bobDeviceId = lime_tester::makeRandomDeviceName("bob.");

		// create Managers : they will open/create the database given in first parameter, and use the function given in second one to communicate with server.
		// Any application using Lime shall instantiate one LimeManager only, even in case of multiple users managed by the application.
		auto aliceManager = make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);

		// Here we have simulate two distinct devices so we have two managers
		auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);

		LIME_LOGI<<"Create "<<*aliceDeviceId<<" and "<<*bobDeviceId<<" users"<<endl;
		// create users, this operation is asynchronous (as the user is also created on X3DH server)
		// The OPkInitialBatchSize parameter is optionnal and is used to set how many One-Time pre-keys will be
		// uploaded to the X3DH server at creation. Default value is set in lime::settings.
		// Last parameter is a callback accepting as parameters a return code and a string
		//      - In case of successful operation the return code is lime::CallbackReturn::success, and string is empty
		//      - In case of failure, the return code is lime::CallbackReturn::fail and the string shall give details on the failure cause
		auto tmp_aliceDeviceId = *aliceDeviceId; // use a temporary variable as it may be a local variable which get out of scope right after call to create_user
		aliceManager->create_user(tmp_aliceDeviceId, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		tmp_aliceDeviceId.clear(); // deviceId may go out of scope as soon as we come back from call
		// wait for the operation to complete
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		auto tmp_bobDeviceId = *bobDeviceId; // use a temporary variable as it may be a local variable which get out of scope right after call to create_user
		bobManager->create_user(tmp_bobDeviceId, x3dh_server_url, curve, callback);
		tmp_bobDeviceId.clear(); // deviceId may go out of scope as soon as we come back from call
		// wait for the operation to complete
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));


		/*** alice encrypt a message to bob, all parameters given to encrypt function are shared_ptr. ***/
		// The encryption generates:
		//      - one common cipher message which must be sent to all recipient devices(depends on encryption policy, message length and recipient number, it may be actually empty)
		//      - a cipher header per recipient device, each recipient device shall receive its specific one

		// Create an empty RecipientData vector, in this basic case we will encrypt to one device only but we can do it to any number of recipient devices.
		// RecipientData holds:
		//      - recipient device id (identify the recipient)
		//      - peer Device status :
		//          - input : if explicitely set to lime::PeerDeviceStatus::fail, this entry is ignored
		//          - output : the current status of this device in local database. See lime::PeerDeviceStatus definition(in lime.hpp) for details
		//      - Double Ratchet message : output of encryption process targeted to this recipient device only
		auto recipients = make_shared<std::vector<RecipientData>>();
		recipients->emplace_back(*bobDeviceId); // we have only one recipient identified by its device id.
		// Shall we have more recipients (bob can have several devices or be a conference sip:uri, alice other devices must get a copy of the message), we just need to emplace_back some more recipients Device Id (GRUU)

		// the plain message, type is std::vector<uint8_t> as it can be text as in this test but also any kind of data.
		auto message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto cipherMessage = make_shared<std::vector<uint8_t>>(); // an empty buffer to get the encrypted message

		LIME_LOGI<<"Alice encrypt the message"<<endl;
		/************** SENDER SIDE CODE *****************************/
		// encrypt, parameters are:
		//      - localDeviceId to select which of the users managed by the LimeManager we shall use to perform the encryption (in our example we have only one local device). This one doesn't need to be a shared pointer.
		//      - recipientUser: an id of the recipient user (which can hold several devices), typically its sip:uri
		//      - RecipientData vector (see above), list all recipient devices, will hold their DR message
		//      - plain message
		//      - cipher message (this one must then be distributed to all recipients devices)
		//      - a callback (prototype: void(lime::CallbackReturn, std::string))
		{
		aliceManager->encrypt(*aliceDeviceId, make_shared<const std::string>("bob"), recipients, message, cipherMessage,
					// lambda to get the results, it captures :
					// - counter : relative to the test, real application won't need this, it's local and used to wait for completion and can't be destroyed before the call to this closure
					// - recipients :  It will hold the same list of deviceIds we set as input with their corresponding DRmessage.
					// - cipherMessage : It will hold the cipher message to be sent to all recipients devices.
					// IMPORTANT : recipients and cipherMessage are captured by copy not reference. They are shared_ptr, their original scope is likely to be the function where the encrypt is called.
					//             they shall then be destroyed when getting out of this function and thus won't be valid anymore when this closure is called. By getting a copy we just increase their
					//             use count and are sure to still have them valid when we are called.
					//             When the closure itself is destroyed (when last reference to it is destroyed), it will trigger destruction of the captured values (-1 in use count for the shared_ptr)
					//             After this closure is called it is destroyed(internal reference is dropped) decreasing the count and allowing the release of the buffer.
					//
					//             It may be wise to use weak_ptr instead of shared ones so if any problem occurs resulting in callback never being called/destroyed, it won't held this buffer from being destroyed
					//             In normal operation, the shared_ptrs to recipients and cipherMessage given to encrypt function are internally owned at least until the callback is called.
					[&counters,
					recipients, cipherMessage](lime::CallbackReturn returnCode, std::string errorMessage){
						// counters is related to this test environment only, not to be considered for real usage
						if (returnCode == lime::CallbackReturn::success) {
							counters.operation_success++;
							// here is the code processing the output when all went well.
							// Send the message to recipient
							// that function must, before returning, send or copy the data to send them later
							// recipients and cipherMessage are likely to be be destroyed as soon as we get out of this closure
							// In this example we know that bodDevice is in recipients[0], real code shall loop on recipients vector
							sendMessageTo("bob", (*recipients)[0].DRmessage, *cipherMessage);
						} else {
							counters.operation_failed++;
							// The encryption failed.
							LIME_LOGE<<"Lime operation failed : "<<errorMessage;
						}
					});
		}
		LIME_LOGI<<"Alice encrypt the message, out of encrypt call, wait for callback"<<endl;
		// in real sending situation, the local instance of the shared pointer are destroyed by exiting the function where they've been declared
		// and where we called the encrypt function. (The LimeManager shall instead never be destroyed until the application terminates)
		recipients = nullptr;
		cipherMessage = nullptr;
		/****** end of SENDER SIDE CODE ******************************/

		/************** SYNCHRO **************************************/
		// this is just waiting for the callback to increase the operation_success field in counters
		// sending ticks to the belle-sip stack in order to process messages
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		LIME_LOGI<<"Alice encrypt the message, callback Ok or timeout reached"<<endl;
		/****** end of  SYNCHRO **************************************/


		/************** RECIPIENT SIDE CODE **************************/
		LIME_LOGI<<"Bob decrypt the message"<<endl;
		// retrieve message, in real situation the server shall fan-out only the part we need or client shall parse in the DRmessages to retrieve the one addressed to him.
		// Note: here we just use the aliceDeviceId variable, in real situation, recipient shall extract from incoming message the sender's GRUU
		std::vector<uint8_t> bobReceivedDRmessage{};
		std::vector<uint8_t> bobReceivedCipherMessage{};
		getMessageFor("bob", bobReceivedDRmessage, bobReceivedCipherMessage);

		if (bobReceivedDRmessage.size()>0 && bobReceivedCipherMessage.size()>0) {

			std::vector<uint8_t> plainTextMessage{}; // a data vector to store the decrypted message
			// it is the first time bob's Device is in communication with Alice's one, so the decrypt will return PeerDeviceStatus::unknown
			// successive messages from Alice shall get a PeerDeviceStatus::untrusted as we did not take care of peer identity validation
			BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, bobReceivedDRmessage, bobReceivedCipherMessage, plainTextMessage) == lime::PeerDeviceStatus::unknown);

			// it's a text message, so turn it into a string and compare with the one alice sent
			std::string plainTextMessageString{plainTextMessage.begin(), plainTextMessage.end()};
			BC_ASSERT_TRUE(plainTextMessageString == lime_tester::messages_pattern[0]);
			LIME_LOGI<<"Bob decrypt the message completed"<<endl;
		} else {
			LIME_LOGI<<"Bob decrypt the message : no message found"<<endl;
		}
		/******* end of RECIPIENT SIDE CODE **************************/



		/************** Users maintenance ****************************/
		// Around once a day the update function shall be called on LimeManagers
		// it will perform localStorage cleanings
		// update of cryptographic material (Signed Pre-key and One-time Pre-keys)
		// The update take as optionnal parameters :
		//  - lower bound for One-time Pre-key available on server
		//  - One-time Pre-key batch size to be generated and uploaded if lower limit on server is reached
		//  If called more often than once a day, it just does nothing
		aliceManager->update(*aliceDeviceId, callback, 10, 3); // if less than 10 keys are availables on server, upload a batch of 3, typical values shall be higher.
		bobManager->update(*bobDeviceId, callback); // use default values for the limit and batch size
		expected_success+=2;
		/******* end of Users maintenance ****************************/
		// wait for updates to complete
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));

		// delete the users
		if (cleanDatabase) {
			aliceManager->delete_user(*aliceDeviceId, callback);
			bobManager->delete_user(*bobDeviceId, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,expected_success+2,lime_tester::wait_for_timeout)); // we must get a callback saying all went well
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		LIME_LOGE << e;
		BC_FAIL("");
	}
}

 /* Verify identity :
 * this test code is a copy of previous one with the addition of peer identity verification.
 * Code could be obviouly factorised with previous test but it's kept complete for clarity
 * Difference from previous test are introduced by a [verify] marker in the comments
 *
 * - Alice and Bob register themselves on X3DH server(use randomized device Ids to allow test server to run several test in parallel)
 * - [verify] retrieve Alice and Bob identity keys, and set as verified Alice's key in Bob's context and Bob's key in alice context
 *   Note: this step shall be performed in a secure way, using ZRTP auxiliary secret and SAS verification mechanism
 * - Alice encrypt a message for Bob (this will fetch Bob's key from server)
 * - [verify] Check in encrypt output that the recipient device is a trusted one
 * - [verify] Before decryption, Bob check that sender is a trusted device
 * - Bob decrypt alice message
 *
 *   @param[in] curve		Lime can run with cryptographic operations based on curve25519 or curve448, set by this parameter in this test.
 *   				One X3DH server runs on one type of key and all clients must use the same
 *   @param[in]	dbBaseFilename	The local filename for users will be this base.<alice/bob>.<curve type>.sqlite3
 *   @param[x3dh_server_url]	The URL (including port) of the X3DH server
 */
static void helloworld_verifyIdentity_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url) {
	// users databases names: baseFilename.<alice/bob>.<curve id>.sqlite3
	std::string dbFilenameAlice{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append(lime_tester::curveId(curve)).append(".sqlite3");
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameBob.append(".bob.").append(lime_tester::curveId(curve)).append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	// This function is used as a lambda for creation/deletion of user, not of lot of interest
	// The counters part is for test synchronisation purpose
	// The returnCode gives the status of command execution.
	// Encryption make use of a lambda too but it's written directly in the call, see below
	limeCallback callback([&counters](lime::CallbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::CallbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						LIME_LOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});

	try {
		LIME_LOGI<<"Create alice and bob LimeManagers"<<endl;
		// create random devices names (in case we use a shared test server, devices id shall be the GRUU, X3DH/Lime does not connect user (sip:uri) and device (gruu)
		// From Lime perspective, only devices exists and they must be uniquely identifies on the X3DH server.
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.");
		auto bobDeviceId = lime_tester::makeRandomDeviceName("bob.");

		// create Managers : they will open/create the database given in first parameter, and use the function given in second one to communicate with server.
		// Any application using Lime shall instantiate one LimeManager only, even in case of multiple users managed by the application.
		auto aliceManager = make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);

		// Here we have simulate two distinct devices so we have two managers
		auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);

		LIME_LOGI<<"Create "<<*aliceDeviceId<<" and "<<*bobDeviceId<<" users"<<endl;
		// create users, this operation is asynchronous(as the user is also created on X3DH server)
		// The OPkInitialBatchSize parameter is optionnal and is used to set how many One-Time pre-keys will be
		// uploaded to the X3DH server at creation. Default value is set in lime::settings.
		// Last parameter is a callback acceptiong as parameters a return code and a string
		//      - In case of successfull operation the return code is lime::CallbackReturn::success, and string is empty
		//      - In case of failure, the return code is lime::CallbackReturn::fail and the string shall give details on the failure cause
		auto tmp_aliceDeviceId = *aliceDeviceId; // use a temporary variable as it may be a local variable which get out of scope right after call to create_user
		aliceManager->create_user(tmp_aliceDeviceId, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		tmp_aliceDeviceId.clear(); // deviceId may go out of scope as soon as we come back from call
		// wait for the operation to complete
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		auto tmp_bobDeviceId = *bobDeviceId; // use a temporary variable as it may be a local variable which get out of scope right after call to create_user
		bobManager->create_user(tmp_bobDeviceId, x3dh_server_url, curve, callback);
		tmp_bobDeviceId.clear(); // deviceId may go out of scope as soon as we come back from call
		// wait for the operation to complete
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// [verify] Retrieve from Managers Bob and Alice device Identity Key
		std::vector<uint8_t> aliceIk{};
		aliceManager->get_selfIdentityKey(*aliceDeviceId, aliceIk);
		std::vector<uint8_t> bobIk{};
		bobManager->get_selfIdentityKey(*bobDeviceId, bobIk);

		// [verify] Set each others key as verified, this step is usually done via a secure exchange mechanism
		// libsignal uses fingerprints, linphone inserts the key in SDP and then build a ZRTP auxiliary secret out of it
		// SAS validation with matching auxiliary secret confirms that keys have been exchanged correctly
		//
		// There is no need to provide local device reference when setting a key as all peer devices identity infornations are
		// shared between local devices.
		//
		// Last parameter is the value of trust flag, it can be reset(in case of SAS reset) by calling again this function and setting it to false.
		//
		// This call can be performed before or after the beginning of a Lime conversation, if something is really bad happen, it will generate an exception.
		// When calling it with true as trusted flag after a SAS validation confirms the peer identity key, if an exception is raised
		// it MUST be reported to user as it means that all previously established Lime session with that device were actually compromised(or someone broke ZRTP)
		aliceManager->set_peerDeviceStatus(*bobDeviceId, bobIk, lime::PeerDeviceStatus::trusted);
		bobManager->set_peerDeviceStatus(*aliceDeviceId, aliceIk, lime::PeerDeviceStatus::trusted);

		/*** alice encrypt a message to bob, all parameters given to encrypt function are shared_ptr. ***/
		// The encryption generates:
		//      - one common cipher message which must be sent to all recipient devices(depends on encryption policy, message length and recipient number, it may be actually empty)
		//      - a cipher header per recipient device, each recipient device shall receive its specific one

		// [verify] before encryption we can verify that recipient identity is a trusted peer(and eventually decide to not perform the encryption if it is not)
		// This information will be provided by the encrypt function anyway for each recipient device
		// Here Bob's device is trusted as we just set its identity as verified
		BC_ASSERT_TRUE(aliceManager->get_peerDeviceStatus(*bobDeviceId) == lime::PeerDeviceStatus::trusted);

		// Create an empty RecipientData vector, in this basic case we will encrypt to one device only but we can do it to any number of recipient devices.
		// RecipientData holds:
		//      - recipient device id (identify the recipient)
		//      - peer Device status :
		//          - input : if explicitely set to lime::PeerDeviceStatus::fail, this entry is ignored
		//          - output : the current status of this device in local database. See lime::PeerDeviceStatus definition(in lime.hpp) for details
		//      - Double Ratchet message : output of encryption process targeted to this recipient device only
		auto recipients = make_shared<std::vector<RecipientData>>();
		recipients->emplace_back(*bobDeviceId); // we have only one recipient identified by its device id.
		// Shall we have more recipients (bob can have several devices or be a conference sip:uri, alice other devices must get a copy of the message), we just need to emplace_back some more recipients Device Id (GRUU)

		// the plain message, type is std::vector<uint8_t> as it can be text as in this test but also any kind of data.
		auto message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto cipherMessage = make_shared<std::vector<uint8_t>>(); // an empty buffer to get the encrypted message

		LIME_LOGI<<"Alice encrypt the message"<<endl;
		/************** SENDER SIDE CODE *****************************/
		// encrypt, parameters are:
		//      - localDeviceId to select which of the users managed by the LimeManager we shall use to perform the encryption (in our example we have only one local device). This one doesn't need to be a shared pointer.
		//      - recipientUser: an id of the recipient user (which can hold several devices), typically its sip:uri
		//      - RecipientData vector (see above), list all recipient devices, will hold their DR message
		//      - plain message
		//      - cipher message (this one must then be distributed to all recipients devices)
		//      - a callback (prototype: void(lime::CallbackReturn, std::string))
		aliceManager->encrypt(*aliceDeviceId, make_shared<const std::string>("bob"), recipients, message, cipherMessage,
					// lambda to get the results, it captures :
					// - counter : relative to the test, real application won't need this, it's local and used to wait for completion and can't be destroyed before the call to this closure
					// - recipients :  It will hold the same list of deviceIds we set as input with their corresponding DRmessage.
					// - cipherMessage : It will hold the cipher message to be sent to all recipients devices.
					// IMPORTANT : recipients and cipherMessage are captured by copy not reference. They are shared_ptr, their original scope is likely to be the function where the encrypt is called.
					//             they shall then be destroyed when getting out of this function and thus won't be valid anymore when this closure is called. By getting a copy we just increase their
					//             use count and are sure to still have them valid when we are called.
					//             When the closure itself is destroyed (when last reference to it is destroyed), it will trigger destruction of the captured values(-1 in use count for the shared_ptr)
					//             After this closure is called it is destroyed(internal reference is dropped) decreasing the count and allowing the release of the buffer.
					//
					//             It may be wise to use weak_ptr instead of shared ones so if any problem occurs resulting in callback never being called/destroyed, it won't held this buffer from being destroyed
					//             In normal operation, the shared_ptrs to recipients and cipherMessage given to encrypt function are internally owned at least until the callback is called.
					[&counters,
					recipients, cipherMessage](lime::CallbackReturn returnCode, std::string errorMessage){
						// counters is related to this test environment only, not to be considered for real usage
						if (returnCode == lime::CallbackReturn::success) {
							counters.operation_success++;
							// here is the code processing the output when all went well.
							// Send the message to recipient
							// that function must, before returning, send or copy the data to send them later
							// recipients and cipherMessage are likely to be be destroyed as soon as we get out of this closure
							// In this example we know that bodDevice is in recipients[0], real code shall loop on recipients vector
							sendMessageTo("bob", (*recipients)[0].DRmessage, *cipherMessage);
							// [verify] now we can also check the trusted status of recipients, as we set as trusted Bob's key, it shall be trusted
							BC_ASSERT_TRUE((*recipients)[0].peerStatus == lime::PeerDeviceStatus::trusted);
						} else {
							counters.operation_failed++;
							// The encryption failed.
							LIME_LOGE<<"Lime operation failed : "<<errorMessage;
						}
					});

		LIME_LOGI<<"Alice encrypt the message, out of encrypt call, wait for callback"<<endl;
		// in real sending situation, the local instance of the shared pointer are destroyed by exiting the function where they've been declared
		// and where we called the encrypt function. (The LimeManager shall instead never be destroyed until the application terminates)
		recipients = nullptr;
		cipherMessage = nullptr;
		/****** end of SENDER SIDE CODE ******************************/

		/************** SYNCHRO **************************************/
		// this is just waiting for the callback to increase the operation_success field in counters
		// sending ticks to the belle-sip stack in order to process messages
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		LIME_LOGI<<"Alice encrypt the message, callback Ok or timeout reached"<<endl;
		/****** end of  SYNCHRO **************************************/


		/************** RECIPIENT SIDE CODE **************************/
		LIME_LOGI<<"Bob decrypt the message"<<endl;
		// retrieve message, in real situation the server shall fan-out only the part we need or client shall parse in the DRmessages to retrieve the one addressed to him.
		// Note: here we just use the aliceDeviceId variable, in real situation, recipient shall extract from incoming message the sender's GRUU
		std::vector<uint8_t> bobReceivedDRmessage{};
		std::vector<uint8_t> bobReceivedCipherMessage{};
		getMessageFor("bob", bobReceivedDRmessage, bobReceivedCipherMessage);

		if (bobReceivedDRmessage.size()>0 && bobReceivedCipherMessage.size()>0) {

			// [verify] before decryption we can verify that sender is a trusted peer,
			// it is not really needed as this information will be provided by the decrypt function anyway
			BC_ASSERT_TRUE(bobManager->get_peerDeviceStatus(*aliceDeviceId) == lime::PeerDeviceStatus::trusted);

			std::vector<uint8_t> plainTextMessage{}; // a data vector to store the decrypted message
			// [verify] it is the first time bob's Device is in communication with Alice's one via message
			// but they already exchanged their identity keys so they Bob's device trust Alice's one since the first incoming message
			BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, bobReceivedDRmessage, bobReceivedCipherMessage, plainTextMessage) == lime::PeerDeviceStatus::trusted);

			// it's a text message, so turn it into a string and compare with the one alice sent
			std::string plainTextMessageString{plainTextMessage.begin(), plainTextMessage.end()};
			BC_ASSERT_TRUE(plainTextMessageString == lime_tester::messages_pattern[0]);
			LIME_LOGI<<"Bob decrypt the message completed"<<endl;
		} else {
			LIME_LOGI<<"Bob decrypt the message : no message found"<<endl;
		}
		/******* end of RECIPIENT SIDE CODE **************************/



		/************** Users maintenance ****************************/
		// Around once a day the update function shall be called on LimeManagers
		// it will perform localStorage cleanings
		// update of cryptographic material (Signed Pre-key and One-time Pre-keys)
		// The update take as optionnal parameters :
		//  - lower bound for One-time Pre-key available on server
		//  - One-time Pre-key batch size to be generated and uploaded if lower limit on server is reached
		//
		// Important : Avoid calling this function when connection to network is impossible
		// try to first fetch any available message on server, process anything and then update
		aliceManager->update(*aliceDeviceId, callback, 10, 3); // if less than 10 keys are availables on server, upload a batch of 3, typical values shall be higher.
		bobManager->update(*bobDeviceId, callback); // use default values for the limit and batch size
		expected_success+=2;
		/******* end of Users maintenance ****************************/
		// wait for updates to complete
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));

		// delete the users
		if (cleanDatabase) {
			aliceManager->delete_user(*aliceDeviceId, callback);
			bobManager->delete_user(*bobDeviceId, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,expected_success+2,lime_tester::wait_for_timeout)); // we must get a callback saying all went well
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		LIME_LOGE << e;
		BC_FAIL("");
	}
}

static void helloworld_basic(void) {
	// run the test on Curve25519 and Curve448 based encryption if available
#ifdef EC25519_ENABLED
	helloworld_basic_test(lime::CurveId::c25519, "helloworld_basic", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519_server_port).data());
#endif
#ifdef EC448_ENABLED
	helloworld_basic_test(lime::CurveId::c448, "helloworld_basic", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c448_server_port).data());
#endif
#ifdef HAVE_BCTBXPQ
	helloworld_basic_test(lime::CurveId::c25519k512, "helloworld_basic", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519k512_server_port).data());
#endif
}

static void helloworld_verifyIdentity(void) {
	// run the test on Curve25519 and Curve448 based encryption if available
#ifdef EC25519_ENABLED
	helloworld_verifyIdentity_test(lime::CurveId::c25519, "helloworld_basic", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519_server_port).data());
#endif
#ifdef EC448_ENABLED
	helloworld_verifyIdentity_test(lime::CurveId::c448, "helloworld_basic", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c448_server_port).data());
#endif
#ifdef HAVE_BCTBXPQ
	helloworld_verifyIdentity_test(lime::CurveId::c25519k512, "helloworld_basic", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519k512_server_port).data());
#endif
}


static test_t tests[] = {
	TEST_NO_TAG("Basic", helloworld_basic),
	TEST_NO_TAG("Verify Identity", helloworld_verifyIdentity),
};

test_suite_t lime_helloworld_test_suite = {
	"Hello World",
	http_before_all,
	http_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests,
	0,
	0
};
