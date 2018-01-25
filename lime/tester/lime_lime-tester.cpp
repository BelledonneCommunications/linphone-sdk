/*
	lime_lime-tester.cpp
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define BCTBX_LOG_DOMAIN "lime-tester"
#include <bctoolbox/logging.h>

#include "lime/lime.hpp"
#include "lime_lime.hpp"
#include "lime-tester.hpp"
#include "lime_keys.hpp"
#include "lime-tester-utils.hpp"

#include <bctoolbox/tester.h>
#include <bctoolbox/exception.hh>
#include <belle-sip/belle-sip.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>

using namespace::std;
using namespace::lime;

std::string test_x3dh_server_url{"localhost"};
std::string test_x3dh_c25519_server_port{"25519"};
std::string test_x3dh_c448_server_port{"25520"};

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

/* This is the callback used for authentication on the test server.
 * Test server holds only one user which is used for all connections(which MUST not work on a real server)
 */
static void process_auth_requested (void *data, belle_sip_auth_event_t *event){
	// the deviceId is set in the event username(accessible via belle_sip_auth_event_get_username(event);
	const char *username = belle_sip_auth_event_get_username(event);
	if (username == NULL) {
		BCTBX_SLOGI<<"Unable to retrieve username from server's authentication request";
	} else {
		BCTBX_SLOGI<<"Accessing credentials for user "<<std::string(username);
	}

	// for test purpose we use a server which accept commands in name of any user using credential of the only one user active on it
	// so we will crash the username with the one test server accepts
	belle_sip_auth_event_set_username(event, lime_tester::test_server_user_name.data());

	// In real world we shall provide the password for the requested user as below
	belle_sip_auth_event_set_passwd(event, lime_tester::test_server_user_password.data());
}

struct C_Callback_userData {
	const limeX3DHServerResponseProcess responseProcess;
	C_Callback_userData(const limeX3DHServerResponseProcess &response) : responseProcess(response) {};
};

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
	belle_http_request_listener_callbacks_t cbs={};
	belle_http_request_listener_t *l;
	belle_generic_uri_t *uri;
	belle_http_request_t *req;
	belle_sip_memory_body_handler_t *bh;

	bh = belle_sip_memory_body_handler_new_copy_from_buffer(message.data(), message.size(), NULL, NULL);

	uri=belle_generic_uri_parse(url.data());

	req=belle_http_request_create("POST",
			uri,
			belle_http_header_create("User-Agent", "lime"),
			belle_http_header_create("Content-type", "x3dh/octet-stream"),
			belle_http_header_create("From", from.data()),
			NULL);

	belle_sip_message_set_body_handler(BELLE_SIP_MESSAGE(req),BELLE_SIP_BODY_HANDLER(bh));
	cbs.process_response=process_response;
	cbs.process_io_error=process_io_error;
	cbs.process_auth_requested=process_auth_requested;
	// store a reference to the responseProcess function in a wrapper as belle-sip request C-style callbacks with a void * user data parameter, C++ implementation shall
	// use lambda and capture the function.
	C_Callback_userData *userData = new C_Callback_userData(responseProcess); // create on the heap a copy of the responseProcess closure so it's available when we're called back by belle-sip
	l=belle_http_request_listener_create_from_callbacks(&cbs, userData);
	belle_sip_object_data_set(BELLE_SIP_OBJECT(req), "http_request_listener", l, belle_sip_object_unref); // Ensure the listener object is destroyed when the request is destroyed
	belle_http_provider_send_request(prov,req,l);
});


/* This function will destroy and recreate managers given in parameter, force deleting all internal cache and start back from what is in local Storage */
static void managersClean(std::unique_ptr<LimeManager> &alice, std::unique_ptr<LimeManager> &bob, std::string aliceDb, std::string bobDb) {
	alice = nullptr;
	bob = nullptr;
	alice = unique_ptr<lime::LimeManager>(new lime::LimeManager(aliceDb, X3DHServerPost));
	bob = std::unique_ptr<lime::LimeManager>(new lime::LimeManager(bobDb, X3DHServerPost));
	BCTBX_SLOGI<<"Trash and reload alice and bob LimeManagers";
}

/**
 * Helper, alice and bob exchange messages:
 * - bob sends batch_size message to alice, alice decrypts
 * - alice responds with batch_size messages to bob, bob decrypts
 * - repeat batch_number times
 *
 */
static void lime_exchange_messages(std::shared_ptr<std::string> &aliceDeviceId, std::unique_ptr<LimeManager> &aliceManager,
				   std::shared_ptr<std::string> &bobDeviceId, std::unique_ptr<LimeManager> &bobManager,
				   int batch_number, int batch_size) {


	size_t messageCount = 0;
	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});

	try {
		auto bobCount=0;
		for (auto batches=0; batches<batch_number; batches++) {
			for (auto i=0; i<batch_size; i++) {
				auto patternIndex = messageCount % lime_tester::messages_pattern.size();
				// bob encrypt a message to Alice
				auto bobRecipients = make_shared<std::vector<recipientData>>();
				bobRecipients->emplace_back(*aliceDeviceId);
				auto bobMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[patternIndex].begin(), lime_tester::messages_pattern[patternIndex].end());
				auto bobCipherMessage = make_shared<std::vector<uint8_t>>();

				bobManager->encrypt(*bobDeviceId, make_shared<const std::string>("alice"), bobRecipients, bobMessage, bobCipherMessage, callback);
				BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

				// alice decrypt
				std::vector<uint8_t> receivedMessage{};
				BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "alice", *bobDeviceId, (*bobRecipients)[0].cipherHeader, *bobCipherMessage, receivedMessage));
				auto receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
				BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[patternIndex]);
				messageCount++;
			}

			for (auto i=0; i<batch_size; i++) {
				auto patternIndex = messageCount % lime_tester::messages_pattern.size();
				// alice respond to bob
				auto aliceRecipients = make_shared<std::vector<recipientData>>();
				aliceRecipients->emplace_back(*bobDeviceId);
				auto aliceMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[patternIndex].begin(), lime_tester::messages_pattern[patternIndex].end());
				auto aliceCipherMessage = make_shared<std::vector<uint8_t>>();

				aliceManager->encrypt(*aliceDeviceId, make_shared<const std::string>("bob"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
				BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

				// bob decrypt
				bobCount++;
				std::vector<uint8_t> receivedMessage{};
				BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, (*aliceRecipients)[0].cipherHeader, *aliceCipherMessage, receivedMessage));
				auto receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
				BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[patternIndex]);
				messageCount++;
			}
		}
	} catch (BctbxException &e) {
		BC_FAIL("Message Exchange failed");
		throw;
	}

}


/**
 * Helper, create DB, alice and bob devices, and exchange a message
 */
static void lime_session_establishment(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url,
					std::string &dbFilenameAlice, std::shared_ptr<std::string> &aliceDeviceId, std::unique_ptr<LimeManager> &aliceManager,
					std::string &dbFilenameBob, std::shared_ptr<std::string> &bobDeviceId, std::unique_ptr<LimeManager> &bobManager) {
	// create DB
	dbFilenameAlice = dbBaseFilename;
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");
	dbFilenameBob = dbBaseFilename;
	dbFilenameBob.append(".bob.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});
	try {
		// create Manager and device for alice
		aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
		aliceDeviceId = lime_tester::makeRandomDeviceName("alice.d1.");
		aliceManager->create_user(*aliceDeviceId, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for bob
		bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost));
		bobDeviceId = lime_tester::makeRandomDeviceName("bob.d");
		bobManager->create_user(*bobDeviceId, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		lime_exchange_messages(aliceDeviceId, aliceManager, bobDeviceId, bobManager, 1, 1);

	} catch (BctbxException &e) {
		BC_FAIL("Session establishment failed");
		throw;
	}
}
/**
 * Scenario:
 * - create Bob and Alice devices
 * - retrieve their respective Identity keys
 * - check if they are verified -> they shall not be
 * - set alice key as verified in bob's context
 * - check it is now verified
 * - set it as non verified and check
 * - try to set a different alice identity key in bob's context, we shall have an exception
 * - bob encrypts a message to alice -> check return status give NOT all recipients trusted
 * - set alice key as verified in bob's context
 * - bob encrypts a message to alice -> check return status give all recipients trusted
 * - set a fake bob key in alice context
 * - try to decrypt bob's message, it shall fail
 * - alice try to encrypt a message to bob, it shall fail
 */
static void lime_identityVerifiedStatus_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url) {
		// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");
	dbFilenameBob.append(".bob.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});
	// declare variable outside the try block as we will generate exceptions during the test
	std::unique_ptr<LimeManager> aliceManager = nullptr;
	std::unique_ptr<LimeManager> bobManager = nullptr;
	std::shared_ptr<std::string> aliceDeviceId = nullptr;
	std::shared_ptr<std::string> bobDeviceId = nullptr;
	std::vector<uint8_t> aliceIk{};
	std::vector<uint8_t> bobIk{};

	try {
		// create Manager and device for alice and bob
		aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
		aliceDeviceId = lime_tester::makeRandomDeviceName("alice.d1.");
		aliceManager->create_user(*aliceDeviceId, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost));
		bobDeviceId = lime_tester::makeRandomDeviceName("bob.d1.");
		bobManager->create_user(*bobDeviceId, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		expected_success += 2;
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, expected_success,lime_tester::wait_for_timeout));

		// retrieve their respective Ik
		aliceManager->get_selfIdentityKey(*aliceDeviceId, aliceIk);
		bobManager->get_selfIdentityKey(*bobDeviceId, bobIk);

		// check their status
		BC_ASSERT_FALSE(aliceManager->get_peerIdentityVerifiedStatus(*bobDeviceId));
		BC_ASSERT_FALSE(bobManager->get_peerIdentityVerifiedStatus(*aliceDeviceId));

		// set alice Id key as vertified in Bob's Manager and check it worked
		bobManager->set_peerIdentityVerifiedStatus(*aliceDeviceId, aliceIk, true);
		BC_ASSERT_TRUE(bobManager->get_peerIdentityVerifiedStatus(*aliceDeviceId));
		// reset it and check it worked
		bobManager->set_peerIdentityVerifiedStatus(*aliceDeviceId, aliceIk, false);
		BC_ASSERT_FALSE(bobManager->get_peerIdentityVerifiedStatus(*aliceDeviceId));

	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}

	// try to set another key for alice in bob's context, it shall generate an exception
	auto gotException = false;
	try {
		// copy alice Ik but modify it
		std::vector<uint8_t> fakeIk = aliceIk;
		fakeIk[0] ^= 0xFF;

		bobManager->set_peerIdentityVerifiedStatus(*aliceDeviceId, fakeIk, true);
	} catch (BctbxException &e) {
		BC_PASS();
		gotException = true;
	}

	BC_ASSERT_TRUE(gotException);


	try {
		// Bob encrypts a message for Alice, alice identity verified status shall be : not verified
		auto bobRecipients = make_shared<std::vector<recipientData>>();
		bobRecipients->emplace_back(*aliceDeviceId);
		auto bobMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto bobCipherMessage = make_shared<std::vector<uint8_t>>();
		bobManager->encrypt(*bobDeviceId, make_shared<const std::string>("alice"), bobRecipients, bobMessage, bobCipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		BC_ASSERT_FALSE((*bobRecipients)[0].identityVerified);

		// set again the key as verified in bob's context
		bobManager->set_peerIdentityVerifiedStatus(*aliceDeviceId, aliceIk, true);
		BC_ASSERT_TRUE(bobManager->get_peerIdentityVerifiedStatus(*aliceDeviceId));

		// Bob encrypts a message for Alice, alice identity verified status shall be : verified
		bobRecipients = make_shared<std::vector<recipientData>>();
		bobRecipients->emplace_back(*aliceDeviceId);
		bobMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[1].begin(), lime_tester::messages_pattern[1].end());
		bobCipherMessage = make_shared<std::vector<uint8_t>>();
		bobManager->encrypt(*bobDeviceId, make_shared<const std::string>("alice"), bobRecipients, bobMessage, bobCipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		BC_ASSERT_TRUE((*bobRecipients)[0].identityVerified);

		// set a fake bob key in alice context(set is as verified otherwise the request is just ignored)
		std::vector<uint8_t> fakeIk = bobIk;
		fakeIk[0] ^= 0xFF;
		aliceManager->set_peerIdentityVerifiedStatus(*bobDeviceId, fakeIk, true);

		// alice decrypt but it will fail as the identity key in X3DH init packet is not matching the one we assert as verified
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_FALSE (aliceManager->decrypt(*aliceDeviceId, "alice", *bobDeviceId, (*bobRecipients)[0].cipherHeader, *bobCipherMessage, receivedMessage));

		// alice now try to encrypt to Bob but it will fail as key fetched from X3DH server won't match the one we assert as verified
		auto aliceRecipients = make_shared<std::vector<recipientData>>();
		aliceRecipients->emplace_back(*bobDeviceId);
		auto aliceMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[2].begin(), lime_tester::messages_pattern[2].end());
		auto aliceCipherMessage = make_shared<std::vector<uint8_t>>();

		aliceManager->encrypt(*aliceDeviceId, make_shared<const std::string>("bob"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_failed,1,lime_tester::wait_for_timeout));

		if (cleanDatabase) {
			aliceManager->delete_user(*aliceDeviceId, callback);
			bobManager->delete_user(*bobDeviceId, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success+2,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}

	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}
}

static void lime_identityVerifiedStatus() {
#ifdef EC25519_ENABLED
	lime_identityVerifiedStatus_test(lime::CurveId::c25519, "lime_identityVerifiedStatus", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
#endif
#ifdef EC448_ENABLED
	lime_identityVerifiedStatus_test(lime::CurveId::c448, "lime_identityVerifiedStatus", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
#endif
}

/**
 * scenario :
 * - check the Ik in pattern_db is retrieved as expected
 * - try asking for an unknown user, we shall get an exception
 */
static void lime_getSelfIk_test(const lime::CurveId curve, const std::string &dbFilename, const std::vector<uint8_t> &pattern) {
	// retrieve the Ik and check it matches given pattern
	std::unique_ptr<LimeManager> aliceManager = nullptr;
	std::vector<uint8_t> Ik{};
	try  {
		// create Manager for alice
		aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilename, X3DHServerPost));
		// retrieve alice identity key
		aliceManager->get_selfIdentityKey("alice", Ik);

		BC_ASSERT_TRUE((Ik==pattern));
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
		return;
	}

	// try to get the Ik of a user not in there, we shall get an exception
	try {
		aliceManager->get_selfIdentityKey("bob", Ik);
	} catch (BctbxException &e) {
		// just swallow it
		BC_PASS();
		return;
	}
	BC_FAIL("Get the Ik of a user not in local Storage didn't throw an exception");
}

static void lime_getSelfIk() {
#ifdef EC25519_ENABLED
	std::vector<uint8_t> pattern_selfIk_C25519 {{0x55, 0x6b, 0x4a, 0xc2, 0x24, 0xc1, 0xd4, 0xff, 0xb7, 0x44, 0x82, 0xe2, 0x3c, 0x75, 0x1c, 0x2b, 0x1c, 0xcb, 0xf6, 0xe2, 0x96, 0xcb, 0x18, 0x01, 0xc6, 0x76, 0x2d, 0x30, 0xa0, 0xa2, 0xbb, 0x27}};
	lime_getSelfIk_test(lime::CurveId::c25519, std::string(bc_tester_get_resource_dir_prefix()).append("/data/pattern_getSelfIk.C25519.sqlite3"), pattern_selfIk_C25519);
#endif
#ifdef EC448_ENABLED
	std::vector<uint8_t> pattern_selfIk_C448 {{0xe7, 0x96, 0x9e, 0x53, 0xd3, 0xbf, 0xfb, 0x4c, 0x6d, 0xdb, 0x79, 0xd2, 0xd7, 0x24, 0x91, 0x7b, 0xa8, 0x99, 0x87, 0x20, 0x23, 0xe1, 0xec, 0xd4, 0xb5, 0x76, 0x0f, 0xc2, 0x83, 0xae, 0x5a, 0xf9, 0x1d, 0x25, 0x47, 0xda, 0x0e, 0x71, 0x50, 0xd5, 0xaf, 0x79, 0x92, 0x48, 0xb0, 0xb6, 0x0f, 0xdc, 0x6f, 0x73, 0x3f, 0xd9, 0x9c, 0x2c, 0x95, 0xe3, 0x00}};
	lime_getSelfIk_test(lime::CurveId::c448, std::string(bc_tester_get_resource_dir_prefix()).append("/data/pattern_getSelfIk.C448.sqlite3"), pattern_selfIk_C448);
#endif
}

/**
 * Scenario:
 * - Create a user alice
 * - Create Bob with two devices and encrypt to alice
 * - update so localStorage can notice two keys are not on server anymore and upload more keys(check it worked)
 * - simulate a move forward in time so the OPks missing from server shall be deleted from base
 * - decrypt the first message, it shall work
 * - update
 * - decrypt the second message, it shall fail
 */
static void lime_update_OPk_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url) {
	// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");
	dbFilenameBob.append(".bob.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});
	try {
		// create Manager and device for alice
		auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.d1.");
		aliceManager->create_user(*aliceDeviceId, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// check we have the expected count of OPk in base : the initial batch
		BC_ASSERT_EQUAL(lime_tester::get_OPks(dbFilenameAlice, *aliceDeviceId), lime_tester::OPkInitialBatchSize, size_t, "%ld");

		// call the update, set the serverLimit to initialBatch size and upload an other initial batch if needed
		// As all the keys are still on server, it shall have no effect, check it then
		aliceManager->update(callback, lime_tester::OPkInitialBatchSize, lime_tester::OPkInitialBatchSize);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
		BC_ASSERT_EQUAL(lime_tester::get_OPks(dbFilenameAlice, *aliceDeviceId), lime_tester::OPkInitialBatchSize, size_t, "%ld");

		// We will create a bob device and encrypt for each new epoch
		std::vector<std::unique_ptr<LimeManager>> bobManagers{};
		std::vector<std::shared_ptr<std::string>> bobDeviceIds{};
		std::vector<std::shared_ptr<std::vector<recipientData>>> bobRecipients{};
		std::vector<std::shared_ptr<std::vector<uint8_t>>> bobCipherMessages{};

		size_t patternIndex = 0;
		// create tow devices for bob and encrypt to alice
		for (auto i=0; i<2; i++) {
			bobManagers.push_back(std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost)));
			bobDeviceIds.push_back(lime_tester::makeRandomDeviceName("bob.d"));
			bobManagers.back()->create_user(*(bobDeviceIds.back()), x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

			bobRecipients.push_back(make_shared<std::vector<recipientData>>());
			bobRecipients.back()->emplace_back(*aliceDeviceId);
			auto plainMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[patternIndex].begin(), lime_tester::messages_pattern[patternIndex].end());
			bobCipherMessages.push_back(make_shared<std::vector<uint8_t>>());

			bobManagers.back()->encrypt(*(bobDeviceIds.back()), make_shared<const std::string>("alice"), bobRecipients.back(), plainMessage, bobCipherMessages.back(), callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

			patternIndex++;
			patternIndex %= lime_tester::messages_pattern.size();
		}

		// check we have the expected count of OPk in base : we shall still have the initial batch
		BC_ASSERT_EQUAL(lime_tester::get_OPks(dbFilenameAlice, *aliceDeviceId), lime_tester::OPkInitialBatchSize, size_t, "%ld");

		// call the update, set the serverLimit to initialBatch size and upload an other initial batch if needed
		// As some keys were removed from server this time we shall generate and upload a new batch
		aliceManager->update(callback, lime_tester::OPkInitialBatchSize, lime_tester::OPkInitialBatchSize);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
		// we uploaded a new batch but no key were removed from localStorage so we now have 2*batch size keys
		BC_ASSERT_EQUAL(lime_tester::get_OPks(dbFilenameAlice, *aliceDeviceId), 2*lime_tester::OPkInitialBatchSize, size_t, "%ld");

		// forward time by OPK_limboTime_days
		aliceManager=nullptr; // destroy manager before modifying DB
		lime_tester::forwardTime(dbFilenameAlice, lime::settings::OPk_limboTime_days+1);
		aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));

		// check nothing has changed on our local OPk count
		BC_ASSERT_EQUAL(lime_tester::get_OPks(dbFilenameAlice, *aliceDeviceId), 2*lime_tester::OPkInitialBatchSize, size_t, "%ld");

		// decrypt Bob first message
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "alice", *(bobDeviceIds[0]), (*bobRecipients[0])[0].cipherHeader, *(bobCipherMessages[0]), receivedMessage));
		auto receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// check we have one less key
		BC_ASSERT_EQUAL(lime_tester::get_OPks(dbFilenameAlice, *aliceDeviceId), 2*lime_tester::OPkInitialBatchSize - 1, size_t, "%ld");

		// call the update, set the serverLimit to 0, we don't want to upload more keys, but too old unused local OPk dispatched by server long ago shall be deleted
		aliceManager->update(callback, 0, 0);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// check the local OPk missing on server for a long time has been deleted
		BC_ASSERT_EQUAL(lime_tester::get_OPks(dbFilenameAlice, *aliceDeviceId), 2*lime_tester::OPkInitialBatchSize - 2, size_t, "%ld");

		// try to decrypt Bob's second message, it shall fail as we got rid of the OPk
		receivedMessage.clear();
		BC_ASSERT_FALSE(aliceManager->decrypt(*aliceDeviceId, "alice", *(bobDeviceIds[1]), (*bobRecipients[1])[0].cipherHeader, *(bobCipherMessages[1]), receivedMessage));

		if (cleanDatabase) {
			auto i=0;
			for (auto &bobManager : bobManagers) {
				bobManager->delete_user(*(bobDeviceIds[i]), callback);
				i++;
			}
			aliceManager->delete_user(*aliceDeviceId, callback);
			expected_success += 1+bobManagers.size();
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}
}

static void lime_update_OPk() {
#ifdef EC25519_ENABLED
	lime_update_OPk_test(lime::CurveId::c25519, "lime_update_OPk", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
#endif
#ifdef EC448_ENABLED
	lime_update_OPk_test(lime::CurveId::c448, "lime_update_OPk", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
#endif
}

/**
 * Scenario:
 * - Create a user alice
 * - simulate a move forward in time and update
 * - check we changed SPk and still got the old one
 * - create a device for Bob and encrypt a message for alice fetching a new key bundle(check Bob uses the current SPk set as active)
 * - repeat until we reach the epoch in which SPk shall be deleted, check it is the case
 * - decrypt all Bob message, it shall be Ok for all except the one encrypted with a bundle based on the deleted SPk where decrypt shall fail
 */
static void lime_update_SPk_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url) {
	// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");
	dbFilenameBob.append(".bob.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});
	try {
		// starting epoch nad number of SPk keys in localStorage
		unsigned int epoch=0;
		auto SPkExpectedCount=1;
		size_t patternIndex = 0;

		// create Manager and device for alice
		auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.d1.");
		aliceManager->create_user(*aliceDeviceId, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
		size_t SPkCount=0;
		uint32_t activeSPkId=0;
		BC_ASSERT_TRUE(lime_tester::get_SPks(dbFilenameAlice, *aliceDeviceId, SPkCount, activeSPkId));
		BC_ASSERT_EQUAL(SPkCount, SPkExpectedCount, int, "%d");

		// We will create a bob device and encrypt for each new epoch
		std::vector<std::unique_ptr<LimeManager>> bobManagers{};
		std::vector<std::shared_ptr<std::string>> bobDeviceIds{};
		std::vector<std::shared_ptr<std::vector<recipientData>>> bobRecipients{};
		std::vector<std::shared_ptr<std::vector<uint8_t>>> bobCipherMessages{};

		// create a device for bob and encrypt to alice
		bobManagers.push_back(std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost)));
		bobDeviceIds.push_back(lime_tester::makeRandomDeviceName("bob.d"));
		bobManagers.back()->create_user(*(bobDeviceIds.back()), x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		bobRecipients.push_back(make_shared<std::vector<recipientData>>());
		bobRecipients.back()->emplace_back(*aliceDeviceId);
		auto plainMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[patternIndex].begin(), lime_tester::messages_pattern[patternIndex].end());
		bobCipherMessages.push_back(make_shared<std::vector<uint8_t>>());

		bobManagers.back()->encrypt(*(bobDeviceIds.back()), make_shared<const std::string>("alice"), bobRecipients.back(), plainMessage, bobCipherMessages.back(), callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
		uint32_t SPkIdMessage=0;
		// extract SPKid from bob's message, and check it matches the current one from Alice DB
		BC_ASSERT_TRUE(lime_tester::DR_message_extractX3DHInit_SPkId((*(bobRecipients.back()))[0].cipherHeader, SPkIdMessage));
		BC_ASSERT_EQUAL(SPkIdMessage, activeSPkId, uint32_t, "%x");

		patternIndex++;
		patternIndex %= lime_tester::messages_pattern.size();

		// steping by SPK_lifeTime_days go ahead in time and check the update is performed correctly
		while (epoch<=lime::settings::SPK_limboTime_days) {
			// forward time by SPK_lifeTime_days
			aliceManager=nullptr; // destroy manager before modifying DB
			lime_tester::forwardTime(dbFilenameAlice, lime::settings::SPK_lifeTime_days);
			epoch+=lime::settings::SPK_lifeTime_days;
			aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));

			// call the update, it shall create and upload a new SPk but keep the old ones
			aliceManager->update(callback, 0, lime_tester::OPkInitialBatchSize);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
			SPkExpectedCount++;

			// check we have the correct count of keys in local
			SPkCount=0;
			activeSPkId=0;
			BC_ASSERT_TRUE(lime_tester::get_SPks(dbFilenameAlice, *aliceDeviceId, SPkCount, activeSPkId));
			BC_ASSERT_EQUAL(SPkCount, SPkExpectedCount, int, "%d");

			// create a device for bob and use it to encrypt
			bobManagers.push_back(std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost)));
			bobDeviceIds.push_back(lime_tester::makeRandomDeviceName("bob.d"));
			bobManagers.back()->create_user(*(bobDeviceIds.back()), x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

			bobRecipients.push_back(make_shared<std::vector<recipientData>>());
			bobRecipients.back()->emplace_back(*aliceDeviceId);
			auto plainMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[patternIndex].begin(), lime_tester::messages_pattern[patternIndex].end());
			bobCipherMessages.push_back(make_shared<std::vector<uint8_t>>());

			bobManagers.back()->encrypt(*(bobDeviceIds.back()), make_shared<const std::string>("alice"), bobRecipients.back(), plainMessage, bobCipherMessages.back(), callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

			// extract SPKid from bob's message, and check it matches the current one from Alice DB
			SPkIdMessage=0;
			BC_ASSERT_TRUE(lime_tester::DR_message_extractX3DHInit_SPkId((*(bobRecipients.back()))[0].cipherHeader, SPkIdMessage));
			BC_ASSERT_EQUAL(SPkIdMessage, activeSPkId, uint32_t, "%x");

			patternIndex++;
			patternIndex %= lime_tester::messages_pattern.size();
		}

		// forward time once more by SPK_lifeTime_days, our first SPk shall now be out of limbo and ready to be deleted
		aliceManager=nullptr; // destroy manager before modifying DB
		lime_tester::forwardTime(dbFilenameAlice, lime::settings::SPK_lifeTime_days);
		epoch+=lime::settings::SPK_lifeTime_days;
		aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));

		// call the update, it shall create and upload a new SPk but keep the old ones and delete one
		aliceManager->update(callback, 0, lime_tester::OPkInitialBatchSize);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
		// there shall not be any rise in the number of SPk keys found in DB, check that
		SPkCount=0;
		activeSPkId=0;
		BC_ASSERT_TRUE(lime_tester::get_SPks(dbFilenameAlice, *aliceDeviceId, SPkCount, activeSPkId));
		BC_ASSERT_EQUAL(SPkCount, SPkExpectedCount, int, "%d");

		// Try to decrypt all message: the first message must fail to decrypt as we just deleted the SPk needed to create the session
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_FALSE(aliceManager->decrypt(*aliceDeviceId, "alice", *(bobDeviceIds[0]), (*bobRecipients[0])[0].cipherHeader, *(bobCipherMessages[0]), receivedMessage));
		// other shall be Ok.
		for (size_t i=1; i<bobManagers.size(); i++) {
			receivedMessage.clear();
			BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "alice", *(bobDeviceIds[i]), (*bobRecipients[i])[0].cipherHeader, *(bobCipherMessages[i]), receivedMessage));
			auto receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
			BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[i%lime_tester::messages_pattern.size()]);
		}

		if (cleanDatabase) {
			auto i=0;
			for (auto &bobManager : bobManagers) {
				bobManager->delete_user(*(bobDeviceIds[i]), callback);
				i++;
			}
			aliceManager->delete_user(*aliceDeviceId, callback);
			expected_success += 1+bobManagers.size();
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}
}

static void lime_update_SPk() {
#ifdef EC25519_ENABLED
	lime_update_SPk_test(lime::CurveId::c25519, "lime_update_SPk", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
#endif
#ifdef EC448_ENABLED
	lime_update_SPk_test(lime::CurveId::c448, "lime_update_SPk", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
#endif
}

/**
 * Scenario:
 * - Establish a session between alice and bob
 * - Skip two messages and keep on exchanging post until maxMessagesReceivedAfterSkip
 * - Check we have two message key in localStorage
 * - decrypt a message
 * - Check we have one message key in localStorage
 * - call the update
 * - Check we have no more message key in storage
 * - try to decrypt the message, it shall fail
 */
static void lime_update_clean_MK_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url) {
	std::string dbFilenameAlice;
	std::shared_ptr<std::string> aliceDeviceId;
	std::unique_ptr<LimeManager> aliceManager;
	std::string dbFilenameBob;
	std::shared_ptr<std::string> bobDeviceId;
	std::unique_ptr<LimeManager> bobManager;

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});

	try {
		lime_session_establishment(curve, dbBaseFilename, x3dh_server_url,
					dbFilenameAlice, aliceDeviceId, aliceManager,
					dbFilenameBob, bobDeviceId, bobManager);

		/* Alice encrypt 2 messages that are kept */
		auto aliceRecipients1 = make_shared<std::vector<recipientData>>();
		aliceRecipients1->emplace_back(*bobDeviceId);
		auto aliceMessage1 = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto aliceCipherMessage1 = make_shared<std::vector<uint8_t>>();
		auto aliceRecipients2 = make_shared<std::vector<recipientData>>();
		aliceRecipients2->emplace_back(*bobDeviceId);
		auto aliceMessage2 = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[1].begin(), lime_tester::messages_pattern[1].end());
		auto aliceCipherMessage2 = make_shared<std::vector<uint8_t>>();

		aliceManager->encrypt(*aliceDeviceId, make_shared<const std::string>("bob"), aliceRecipients1, aliceMessage1, aliceCipherMessage1, callback);
		aliceManager->encrypt(*aliceDeviceId, make_shared<const std::string>("bob"), aliceRecipients2, aliceMessage2, aliceCipherMessage2, callback);
		expected_success+=2;
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, expected_success, lime_tester::wait_for_timeout));

		/* Alice and Bob exchange more than maxMessagesReceivedAfterSkip messages(do batches of 4 to be faster) */
		lime_exchange_messages(aliceDeviceId, aliceManager, bobDeviceId, bobManager, lime::settings::maxMessagesReceivedAfterSkip/4+1, 4);

		/* Check that bob got 2 message key in local Storage */
		BC_ASSERT_EQUAL(lime_tester::get_StoredMessageKeyCount(dbFilenameBob, *bobDeviceId, *aliceDeviceId), 2, unsigned int, "%d");

		/* decrypt held message 1 */
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, (*aliceRecipients1)[0].cipherHeader, *aliceCipherMessage1, receivedMessage));
		auto receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		/* Check that bob got 1 message key in local Storage */
		BC_ASSERT_EQUAL(lime_tester::get_StoredMessageKeyCount(dbFilenameBob, *bobDeviceId, *aliceDeviceId), 1, unsigned int, "%d");

		/* call the update function */
		bobManager->update(callback, 0, lime_tester::OPkInitialBatchSize);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success, lime_tester::wait_for_timeout));

		/* Check that bob got 0 message key in local Storage */
		BC_ASSERT_EQUAL(lime_tester::get_StoredMessageKeyCount(dbFilenameBob, *bobDeviceId, *aliceDeviceId), 0, unsigned int, "%d");

		/* try to decrypt message 2, it shall fail */
		receivedMessage.clear();
		BC_ASSERT_FALSE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, (*aliceRecipients2)[0].cipherHeader, *aliceCipherMessage2, receivedMessage));

		if (cleanDatabase) {
			aliceManager->delete_user(*aliceDeviceId, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}
}

static void lime_update_clean_MK() {
#ifdef EC25519_ENABLED
	lime_update_clean_MK_test(lime::CurveId::c25519, "lime_update_clean_MK", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
#endif
#ifdef EC448_ENABLED
	lime_update_clean_MK_test(lime::CurveId::c448, "lime_update_clean_MK", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
#endif
}

/** Scenario
 * - Create one device for alice
 * - Create OPk_batch_size devices for bob, they will all fetch a key and encrypt a messaage to alice, server shall not have anymore OPks
 * - Create another device for alice and send a message to bob, it shall get a non OPk bundle, check it is the case in the message sent to Alice
 * Alice decrypt all messages to complete the check
 *
 */
static void x3dh_without_OPk_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url, bool continuousSession=true) {
	// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameBob.append(".bob.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});
	try {
		// create Manager and device for alice
		auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.d1.");
		auto aliceOPkInitialBatchSize = 3; // give it only 3 OPks as initial batch
		aliceManager->create_user(*aliceDeviceId, x3dh_server_url, curve, aliceOPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		for (auto i=0; i<aliceOPkInitialBatchSize+1; i++) {
			// Create manager and device for bob
			auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost));
			auto bobDeviceId = lime_tester::makeRandomDeviceName("bob.d");
			bobManager->create_user(*bobDeviceId, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

			// encrypt a message to Alice
			auto messagePatternIndex = i % lime_tester::messages_pattern.size();
			auto bobRecipients = make_shared<std::vector<recipientData>>();
			bobRecipients->emplace_back(*aliceDeviceId);
			auto bobMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[messagePatternIndex].begin(), lime_tester::messages_pattern[messagePatternIndex].end());
			auto bobCipherMessage = make_shared<std::vector<uint8_t>>();

			bobManager->encrypt(*bobDeviceId, make_shared<const std::string>("alice"), bobRecipients, bobMessage, bobCipherMessage, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

			// alice decrypt
			std::vector<uint8_t> receivedMessage{};
			bool haveOPk=false;
			BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit((*bobRecipients)[0].cipherHeader, haveOPk));
			if (i<aliceOPkInitialBatchSize) { // the first aliceOPkInitialBatchSize messages must hold an X3DH init message with an OPk
				BC_ASSERT_TRUE(haveOPk);
			} else { // then the last message shall not convey OPK_id as none were available
				BC_ASSERT_FALSE(haveOPk);
			}
			BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "alice", *bobDeviceId, (*bobRecipients)[0].cipherHeader, *bobCipherMessage, receivedMessage));
			auto receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
			BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[messagePatternIndex]);

			if (cleanDatabase) {
				bobManager->delete_user(*bobDeviceId, callback);
				BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			}

			// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
			if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}
		}

		// delete the users so the remote DB will be clean too
		if (cleanDatabase) {
			aliceManager->delete_user(*aliceDeviceId, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}
}
static void x3dh_without_OPk() {
#ifdef EC25519_ENABLED
	x3dh_without_OPk_test(lime::CurveId::c25519, "lime_x3dh_without_OPk", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
	x3dh_without_OPk_test(lime::CurveId::c25519, "lime_x3dh_without_OPk_clean", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data(), false);
#endif
#ifdef EC448_ENABLED
	x3dh_without_OPk_test(lime::CurveId::c448, "lime_x3dh_without_OPk", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
	x3dh_without_OPk_test(lime::CurveId::c448, "lime_x3dh_without_OPk_clean", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data(), false);
#endif
}


/* Alice encrypt to bob, bob replies so session is fully established, then alice encrypt more tjan maxSendingChain message so we must start a new session with bob
 * - alice.d1 and bob.d1 exchange messages
 * - alice encrypt maxSendingChain messages, bob never reply, no real need to decryp them, just check they are not holding X3DH init message
 * - alice encrypt one more message, it shall trigger new session creation so message will hold an X3DH init, decrypt it just to check bob can hold the session change
 */
static void x3dh_sending_chain_limit_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url, bool continuousSession=true) {
	// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameBob.append(".bob.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});
	try {
		// create Manager
		auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
		auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost));

		// create Random devices names
		auto aliceDevice1 = lime_tester::makeRandomDeviceName("alice.d1.");
		auto bobDevice1 = lime_tester::makeRandomDeviceName("bob.d1.");

		// create users alice.d1 and bob.d1
		aliceManager->create_user(*aliceDevice1, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		bobManager->create_user(*bobDevice1, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		expected_success +=2; // we have two asynchronous operation on going
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, expected_success,lime_tester::wait_for_timeout));
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// alice.d1 encrypts a message for bob.d1, bob replies
		auto aliceRecipients = make_shared<std::vector<recipientData>>();
		aliceRecipients->emplace_back(*bobDevice1);
		auto aliceMessage = make_shared<std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto aliceCipherMessage = make_shared<std::vector<uint8_t>>();

		auto bobRecipients = make_shared<std::vector<recipientData>>();
		bobRecipients->emplace_back(*aliceDevice1);
		auto bobMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[1].begin(), lime_tester::messages_pattern[1].end());
		auto bobCipherMessage = make_shared<std::vector<uint8_t>>();

		// alice encrypt
		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// bob decrypt
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit((*aliceRecipients)[0].cipherHeader)); // new sessions created, they must convey X3DH init message
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice1, "bob", *aliceDevice1, (*aliceRecipients)[0].cipherHeader, *aliceCipherMessage, receivedMessage));
		std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// bob encrypt
		bobManager->encrypt(*bobDevice1, make_shared<const std::string>("alice"), bobRecipients, bobMessage, bobCipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// alice decrypt
		BC_ASSERT_FALSE(lime_tester::DR_message_holdsX3DHInit((*bobRecipients)[0].cipherHeader)); // Bob didn't initiate a new session created, so no X3DH init message
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDevice1, "alice", *bobDevice1, (*bobRecipients)[0].cipherHeader, *bobCipherMessage, receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[1]);

		// Alice encrypt maxSendingChain messages to bob, none shall have the X3DH init
		for (auto i=0; i<lime::settings::maxSendingChain; i++) {
			// alice encrypt
			aliceMessage->assign(lime_tester::messages_pattern[i%lime_tester::messages_pattern.size()].begin(), lime_tester::messages_pattern[i%lime_tester::messages_pattern.size()].end());
			aliceCipherMessage->clear();
			aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

			// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
			if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

			BC_ASSERT_FALSE(lime_tester::DR_message_holdsX3DHInit((*aliceRecipients)[0].cipherHeader)); // it's an ongoing session, no X3DH init

			// bob decrypt, it's not really needed here but cannot really hurt, comment if the test is too slow
/*
			std::vector<uint8_t> receivedMessage{};
			BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice1, "bob", *aliceDevice1, (*aliceRecipients)[0].cipherHeader, *aliceCipherMessage, receivedMessage));
			std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
			BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[i%lime_tester::messages_pattern.size()]);
*/
		}

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// alice encrypt, we are over the maximum number, so Alice shall fetch a new key on server and start a new session
		aliceMessage->assign(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		aliceCipherMessage->clear();
		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// bob decrypt, it's not really needed here but...
		receivedMessage.clear();
		BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit((*aliceRecipients)[0].cipherHeader)); // we started a new session
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice1, "bob", *aliceDevice1, (*aliceRecipients)[0].cipherHeader, *aliceCipherMessage, receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// delete the users so the remote DB will be clean too
		if (cleanDatabase) {
			aliceManager->delete_user(*aliceDevice1, callback);
			bobManager->delete_user(*bobDevice1, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success+2,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}
}
static void x3dh_sending_chain_limit() {
#ifdef EC25519_ENABLED
	x3dh_sending_chain_limit_test(lime::CurveId::c25519, "lime_x3dh_sending_chain_limit", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
	x3dh_sending_chain_limit_test(lime::CurveId::c25519, "lime_x3dh_sending_chain_limit_clean", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data(), false);
#endif
#ifdef EC448_ENABLED
	x3dh_sending_chain_limit_test(lime::CurveId::c448, "lime_x3dh_sending_chain_limit", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
	x3dh_sending_chain_limit_test(lime::CurveId::c448, "lime_x3dh_sending_chain_limit_clean", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data(), false);
#endif
}

/*
 * Create multiple DR sessions between a pair of devices and check it will converge to one session being used but also that an old message to a stale session is decrypted correctly
 * - create users alice.d1 and bob.d1
 * - they both encrypt to peer (so each one fetch the peer key bundle from server)
 * - decrypt messages and check we have now 2 sessions in base and the session created by peer is the active one
 * - bob.d1 encrypt to Alice (it must use the alice created session, we have no easy way to check that at this point)
 * - alice.d decrypt, it will move its active session back to the one originally created, now devices are in sync on one session.
 */
static void x3dh_multiple_DRsessions_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url, bool continuousSession=true) {
	// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameBob.append(".bob.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});

	try {
		// create Manager
		auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
		auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost));

		// create Random devices names
		auto aliceDevice1 = lime_tester::makeRandomDeviceName("alice.d1.");
		auto bobDevice1 = lime_tester::makeRandomDeviceName("bob.d1.");

		// create users alice.d1 and bob.d1
		aliceManager->create_user(*aliceDevice1, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		bobManager->create_user(*bobDevice1, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		expected_success +=2; // we have two asynchronous operation on going
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, expected_success,lime_tester::wait_for_timeout));
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// alice.d1 encrypts a message for bob.d1 and bob.d1 encrypts a message for alice.d1
		auto aliceRecipients = make_shared<std::vector<recipientData>>();
		aliceRecipients->emplace_back(*bobDevice1);
		auto bobRecipients = make_shared<std::vector<recipientData>>();
		bobRecipients->emplace_back(*aliceDevice1);
		auto aliceMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto aliceCipherMessage = make_shared<std::vector<uint8_t>>();
		auto bobMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[1].begin(), lime_tester::messages_pattern[1].end());
		auto bobCipherMessage = make_shared<std::vector<uint8_t>>();

		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		bobManager->encrypt(*bobDevice1, make_shared<const std::string>("alice"), bobRecipients, bobMessage, bobCipherMessage, callback);
		expected_success += 1;
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));

		// DB are cleans, so we shall have only one session between pairs and it shall have Id 1, check it
		std::vector<long int> aliceSessionsId{};
		auto aliceActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameAlice, *aliceDevice1, *bobDevice1, aliceSessionsId);
		BC_ASSERT_EQUAL(aliceActiveSessionId, 1, long int, "%ld");
		BC_ASSERT_EQUAL(aliceSessionsId.size(), 1, size_t, "%ld");

		std::vector<long int> bobSessionsId{};
		auto bobActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameBob, *bobDevice1, *aliceDevice1, bobSessionsId);
		BC_ASSERT_EQUAL(bobActiveSessionId, 1, long int, "%ld");
		BC_ASSERT_EQUAL(bobSessionsId.size(), 1, size_t, "%ld");

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// both decrypt the messages, they have crossed on the network
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit((*aliceRecipients)[0].cipherHeader)); // new sessions created, they must convey X3DH init message
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice1, "bob", *aliceDevice1, (*aliceRecipients)[0].cipherHeader, *aliceCipherMessage, receivedMessage));
		std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit((*bobRecipients)[0].cipherHeader)); // new sessions created, they must convey X3DH init message
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDevice1, "alice", *bobDevice1, (*bobRecipients)[0].cipherHeader, *bobCipherMessage, receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[1]);

		// Now DB shall holds 2 sessions each, and both shall have their active as session 2 as the active one must be the last one used for decrypt
		aliceSessionsId.clear();
		aliceActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameAlice, *aliceDevice1, *bobDevice1, aliceSessionsId);
		BC_ASSERT_EQUAL(aliceActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(aliceSessionsId.size(), 2, size_t, "%ld");

		bobSessionsId.clear();
		bobActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameBob, *bobDevice1, *aliceDevice1, bobSessionsId);
		BC_ASSERT_EQUAL(bobActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(bobSessionsId.size(), 2, size_t, "%ld");

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// bob.d1 encrypts a message for alice.d1, it shall use the active session, which is his session 2(used to decrypt alice message) but matched session 1(used to encrypt the first message) in alice DB
		bobRecipients = make_shared<std::vector<recipientData>>();
		bobRecipients->emplace_back(*aliceDevice1);
		bobMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[2].begin(), lime_tester::messages_pattern[2].end());
		bobCipherMessage = make_shared<std::vector<uint8_t>>();

		bobManager->encrypt(*bobDevice1, make_shared<const std::string>("alice"), bobRecipients, bobMessage, bobCipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		aliceSessionsId.clear();
		aliceActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameAlice, *aliceDevice1, *bobDevice1, aliceSessionsId);
		BC_ASSERT_EQUAL(aliceActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(aliceSessionsId.size(), 2, size_t, "%ld");

		bobSessionsId.clear();
		bobActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameBob, *bobDevice1, *aliceDevice1, bobSessionsId);
		BC_ASSERT_EQUAL(bobActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(bobSessionsId.size(), 2, size_t, "%ld");

		// alice decrypt the messages it shall set back session 1 to be the active one as bob used this one to encrypt to alice, they have now converged on a session
		receivedMessage.clear();
		BC_ASSERT_FALSE(lime_tester::DR_message_holdsX3DHInit((*bobRecipients)[0].cipherHeader)); // it is not a new session, no more X3DH message
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDevice1, "alice", *bobDevice1, (*bobRecipients)[0].cipherHeader, *bobCipherMessage, receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[2]);

		// check we have the expected configuration: 2 sessions in each base, session 1 active for alice, session 2 for bob
		aliceSessionsId.clear();
		aliceActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameAlice, *aliceDevice1, *bobDevice1, aliceSessionsId);
		BC_ASSERT_EQUAL(aliceActiveSessionId, 1, long int, "%ld");
		BC_ASSERT_EQUAL(aliceSessionsId.size(), 2, size_t, "%ld");

		bobSessionsId.clear();
		bobActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameBob, *bobDevice1, *aliceDevice1, bobSessionsId);
		BC_ASSERT_EQUAL(bobActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(bobSessionsId.size(), 2, size_t, "%ld");

		// run the update function
		aliceManager->update(callback, 0, lime_tester::OPkInitialBatchSize);
		bobManager->update(callback, 0, lime_tester::OPkInitialBatchSize);
		expected_success+=2;
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));

		// check we have still the same configuration on DR sessions
		aliceSessionsId.clear();
		aliceActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameAlice, *aliceDevice1, *bobDevice1, aliceSessionsId);
		BC_ASSERT_EQUAL(aliceActiveSessionId, 1, long int, "%ld");
		BC_ASSERT_EQUAL(aliceSessionsId.size(), 2, size_t, "%ld");

		bobSessionsId.clear();
		bobActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameBob, *bobDevice1, *aliceDevice1, bobSessionsId);
		BC_ASSERT_EQUAL(bobActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(bobSessionsId.size(), 2, size_t, "%ld");

		// fast forward enought time to get the session old enough to be cleared by update
		// actually, move all timeStamps in local base back in time.
		// destroy the manager to be sure to get an updated version of the DB we are going to modify
		aliceManager = nullptr;
		bobManager = nullptr;
		lime_tester::forwardTime(dbFilenameAlice, lime::settings::DRSession_limboTime_days+1);
		lime_tester::forwardTime(dbFilenameBob, lime::settings::DRSession_limboTime_days+1);

		aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
		bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost));

		// run the update function
		aliceManager->update(callback, 0, lime_tester::OPkInitialBatchSize);
		bobManager->update(callback, 0, lime_tester::OPkInitialBatchSize);
		expected_success+=2;
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));

		// check the configuration, kept the active sessions but removed the old ones
		aliceSessionsId.clear();
		aliceActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameAlice, *aliceDevice1, *bobDevice1, aliceSessionsId);
		BC_ASSERT_EQUAL(aliceActiveSessionId, 1, long int, "%ld");
		BC_ASSERT_EQUAL(aliceSessionsId.size(), 1, size_t, "%ld");

		bobSessionsId.clear();
		bobActiveSessionId =  lime_tester::get_DRsessionsId(dbFilenameBob, *bobDevice1, *aliceDevice1, bobSessionsId);
		BC_ASSERT_EQUAL(bobActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(bobSessionsId.size(), 1, size_t, "%ld");

		// delete the users so the remote DB will be clean too
		if (cleanDatabase) {
			aliceManager->delete_user(*aliceDevice1, callback);
			bobManager->delete_user(*bobDevice1, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success+2,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data()); // delete the database file if already exists
			remove(dbFilenameBob.data()); // delete the database file if already exists
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}
}

static void x3dh_multiple_DRsessions(void) {
#ifdef EC25519_ENABLED
	x3dh_multiple_DRsessions_test(lime::CurveId::c25519, "lime_x3dh_multiple_DRsessions", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
	x3dh_multiple_DRsessions_test(lime::CurveId::c25519, "lime_x3dh_multiple_DRsessions", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data(), false);
#endif
#ifdef EC448_ENABLED
	x3dh_multiple_DRsessions_test(lime::CurveId::c448, "lime_x3dh_multiple_DRsessions", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
	x3dh_multiple_DRsessions_test(lime::CurveId::c448, "lime_x3dh_multiple_DRsessions", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data(), false);
#endif
}


/*
 * alice.d1 will encrypt to bob.d1, bob.d2, bob.d3, bob.d4
 * - message burst from alice.d1 -> bob.d1
 * - wait for callbacks. alice.d1 hold session toward d1 only
 * then burst encrypt to:
 * - bob.d1, bob.d2 : test enqueing if a part of recipients are not available
 * - bob.d1 : test going through if we can process it without calling X3DH server
 * - bob.d2 : test enqueue and have session ready when processed
 * - bob.d3 : test enqueue and must start an asynchronous X3DH request when back
 * - bob.d4 : test enqueue and must start an asynchronous X3DH request when back
 *
 */
static void x3dh_multidev_operation_queue_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url, bool continuousSession=true) {
	// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameBob.append(".bob.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});

	try {
		// create Manager
		auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
		auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost));

		// create Random devices names
		auto aliceDevice1 = lime_tester::makeRandomDeviceName("alice.d1.");
		auto bobDevice1 = lime_tester::makeRandomDeviceName("bob.d1.");
		auto bobDevice2 = lime_tester::makeRandomDeviceName("bob.d2.");
		auto bobDevice3 = lime_tester::makeRandomDeviceName("bob.d3.");
		auto bobDevice4 = lime_tester::makeRandomDeviceName("bob.d4.");

		// create users alice.d1 and bob.d1,d2,d3,d4
		aliceManager->create_user(*aliceDevice1, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		bobManager->create_user(*bobDevice1, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		bobManager->create_user(*bobDevice2, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		bobManager->create_user(*bobDevice3, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		bobManager->create_user(*bobDevice4, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		expected_success +=5; // we have two asynchronous operation on going
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, expected_success,lime_tester::wait_for_timeout));
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// alice first send a message to bob.d1 without waiting for callback before asking encryption of a new one
		constexpr size_t messageBurstSize = 5; // we will reuse the same buffers for further messages storage, so just encrypt 5 messages to d1
		std::array<std::shared_ptr<const std::vector<uint8_t>>, messageBurstSize> messages;
		std::array<std::shared_ptr<std::vector<recipientData>>, messageBurstSize> recipients;
		std::array<std::shared_ptr<std::vector<uint8_t>>, messageBurstSize> cipherMessage;

		for (size_t i=0; i<messages.size(); i++) {
			cipherMessage[i] = make_shared<std::vector<uint8_t>>();
			recipients[i] = make_shared<std::vector<recipientData>>();
			recipients[i]->emplace_back(*bobDevice1);
			messages[i] = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[i].begin(), lime_tester::messages_pattern[i].end());
		}

		for (size_t i=0; i<messages.size(); i++) {
			aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients[i], messages[i], cipherMessage[i], callback);
			expected_success++;
		}
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		std::vector<uint8_t> X3DH_initMessageBuffer{}; // store the X3DH init message extracted from first message of the burst to be able to compare it to the following one, they must be the same.
		// loop on cipher message and decrypt them bob Manager
		for (size_t i=0; i<messages.size(); i++) {
			for (auto &recipient : *(recipients[i])) {
				std::vector<uint8_t> receivedMessage{};
				BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(recipient.cipherHeader)); // new sessions created, they must convey X3DH init message
				if (i==0) { // first message of the burst, extract and store the X3DH init message
					lime_tester::DR_message_extractX3DHInit(recipient.cipherHeader, X3DH_initMessageBuffer);
				} else { // following message of the burst, extract X3DH init message and compare it to the first one, they must be the same, we shall not create new sessions
					std::vector<uint8_t> X3DH_initMessageBuffer_next{};
					lime_tester::DR_message_extractX3DHInit(recipient.cipherHeader, X3DH_initMessageBuffer_next);
					BC_ASSERT_TRUE(X3DH_initMessageBuffer == X3DH_initMessageBuffer_next);
				}
				BC_ASSERT_TRUE(bobManager->decrypt(recipient.deviceId, "bob", *aliceDevice1, recipient.cipherHeader, *(cipherMessage[i]), receivedMessage));
				std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
				BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[i]);
			}
		}

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// now alice will request encryption of message without waiting for callback to:
		// clean previously used buffer
		for (size_t i=0; i<messages.size(); i++) {
			cipherMessage[i] = make_shared<std::vector<uint8_t>>();
			recipients[i] = make_shared<std::vector<recipientData>>();
			// use pattern messages not used yet
			messages[i] = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[messages.size()+i].begin(), lime_tester::messages_pattern[messages.size()+i].end());
		}

		//  bob.d1,bob.d2 -> this one shall trigger a X3DH request to acquire bob.d2 key bundle
		recipients[0]->emplace_back(*bobDevice1);
		recipients[0]->emplace_back(*bobDevice2);
		//  bob.d1 -> this one shall be just be processed so callback will be called before even returning from encrypt call
		recipients[1]->emplace_back(*bobDevice1);
		//  bob.d2 -> this one shall be queued and processed when d1,d2 is done but it won't trigger an X3DH request
		recipients[2]->emplace_back(*bobDevice2);
		//  bob.d3 -> this one shall be queued and processed when previous one is done, it will trigger an X3DH request to get d3 key bundle
		recipients[3]->emplace_back(*bobDevice3);
		//  bob.d4 -> this one shall be queued and processed when previous one is done, it will trigger an X3DH request to get d4 key bundle
		recipients[4]->emplace_back(*bobDevice4);

		for (size_t i=0; i<messages.size(); i++) {
			aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients[i], messages[i], cipherMessage[i], callback);
			expected_success++;
		}
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// decrypt all sent messages and check if they match
		// recipients holds:
		// recipients[0] -> bob.d1, bob.d2
		// recipents[1] -> bob.d1
		// recipients[2] -> bob.d2
		// Check on these that the X3DH init message are matching (we didn't create a second session an encryption was queued correctly)
		std::vector<uint8_t> X3DH_initMessageBuffer1{};
		std::vector<uint8_t> X3DH_initMessageBuffer2{};
		// recipients[0][0] and recipients[1][0]
		lime_tester::DR_message_extractX3DHInit((*recipients[0])[0].cipherHeader, X3DH_initMessageBuffer1);
		lime_tester::DR_message_extractX3DHInit((*recipients[1])[0].cipherHeader, X3DH_initMessageBuffer2);
		BC_ASSERT_TRUE(X3DH_initMessageBuffer1 == X3DH_initMessageBuffer2);
		// recipients[0][1] and recipients[2][0]
		lime_tester::DR_message_extractX3DHInit((*recipients[0])[1].cipherHeader, X3DH_initMessageBuffer1);
		lime_tester::DR_message_extractX3DHInit((*recipients[2])[0].cipherHeader, X3DH_initMessageBuffer2);
		BC_ASSERT_TRUE(X3DH_initMessageBuffer1 == X3DH_initMessageBuffer2);

		// in recipient[0] we have a message encrypted for bob.d1 and bob.d2
		std::vector<uint8_t> receivedMessage{};
		std::string receivedMessageString{};
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[0])[0].deviceId, "bob", *aliceDevice1, (*recipients[0])[0].cipherHeader, *(cipherMessage[0]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[messages.size()+0]);
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[0])[1].deviceId, "bob", *aliceDevice1, (*recipients[0])[1].cipherHeader, *(cipherMessage[0]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[messages.size()+0]);

		// in recipient[1] we have a message encrypted to bob.d1
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[1])[0].deviceId, "bob", *aliceDevice1, (*recipients[1])[0].cipherHeader, *(cipherMessage[1]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[messages.size()+1]);

		// in recipient[2] we have a message encrypted to bob.d2
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[2])[0].deviceId, "bob", *aliceDevice1, (*recipients[2])[0].cipherHeader, *(cipherMessage[2]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[messages.size()+2]);

		// in recipient[2] we have a message encrypted to bob.d3
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[3])[0].deviceId, "bob", *aliceDevice1, (*recipients[3])[0].cipherHeader, *(cipherMessage[3]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[messages.size()+3]);

		// in recipient[2] we have a message encrypted to bob.d4
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[4])[0].deviceId, "bob", *aliceDevice1, (*recipients[4])[0].cipherHeader, *(cipherMessage[4]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[messages.size()+4]);

		// delete the users and db
		if (cleanDatabase) {
			aliceManager->delete_user(*aliceDevice1, callback);
			bobManager->delete_user(*bobDevice1, callback);
			bobManager->delete_user(*bobDevice2, callback);
			bobManager->delete_user(*bobDevice3, callback);
			bobManager->delete_user(*bobDevice4, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success+5,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}
}

static void x3dh_multidev_operation_queue(void) {
#ifdef EC25519_ENABLED
	x3dh_multidev_operation_queue_test(lime::CurveId::c25519, "lime_x3dh_multidev_operation_queue", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
	x3dh_multidev_operation_queue_test(lime::CurveId::c25519, "lime_x3dh_multidev_operation_queue", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data(), false);
#endif
#ifdef EC448_ENABLED
	x3dh_multidev_operation_queue_test(lime::CurveId::c448, "lime_x3dh_multidev_operation_queue", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
	x3dh_multidev_operation_queue_test(lime::CurveId::c448, "lime_x3dh_multidev_operation_queue", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data(), false);
#endif
}


/* test scenario:
 * - create alice.d1 and bob.d1
 * - alice ask for a burst encryption of several message to bob.d1, first one shall trigger a X3DH init, the other ones shall be queued, if not we will initiate several X3DH init
 * - bob.d1 decrypt messages, check decryption is Ok and that they encapsulate the same X3DH init message
 * - Delete Alice and Bob devices to leave distant server base clean
 *
 * if continuousSession is set to false, delete and recreate LimeManager before each new operation to force relying on local Storage
 */
static void x3dh_operation_queue_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url, bool continuousSession=true) {
	// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameBob.append(".bob.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});

	try {
		// create Manager
		auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
		auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost));

		// create Random devices names
		auto aliceDevice1 = lime_tester::makeRandomDeviceName("alice.d1.");
		auto bobDevice1 = lime_tester::makeRandomDeviceName("bob.d1.");

		// create users alice.d1 and bob.d1
		aliceManager->create_user(*aliceDevice1, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		bobManager->create_user(*bobDevice1, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		expected_success +=2; // we have two asynchronous operation on going
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success, expected_success,lime_tester::wait_for_timeout)); // we must get callback saying all went well
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this

		/* destroy and reload the Managers(tests everything is correctly saved/load from local Storage) */
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// alice send a burst of messages to bob without waiting for callback before asking encryption of a new one
		constexpr size_t messageBurstSize = 8;
		std::array<std::shared_ptr<const std::vector<uint8_t>>, messageBurstSize> messages;
		std::array<std::shared_ptr<std::vector<recipientData>>, messageBurstSize> recipients;
		std::array<std::shared_ptr<std::vector<uint8_t>>, messageBurstSize> cipherMessage;

		for (size_t i=0; i<messages.size(); i++) {
			cipherMessage[i] = make_shared<std::vector<uint8_t>>();
			recipients[i] = make_shared<std::vector<recipientData>>();
			recipients[i]->emplace_back(*bobDevice1);
			messages[i] = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[i].begin(), lime_tester::messages_pattern[i].end());
		}

		for (size_t i=0; i<messages.size(); i++) {
			aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients[i], messages[i], cipherMessage[i], callback);
			expected_success++;
		}
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));

		/* destroy and reload the Managers(tests everything is correctly saved/load from local Storage) */
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// bob.d1 decrypt the messages
		std::vector<uint8_t> X3DH_initMessageBuffer{}; // store the X3DH init message extracted from first message of the burst to be able to compare it to the following one, they must be the same.
		// loop on cipher message and decrypt them bob Manager
		for (size_t i=0; i<messages.size(); i++) {
			for (auto &recipient : *(recipients[i])) {
				std::vector<uint8_t> receivedMessage{};
				BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(recipient.cipherHeader)); // new sessions created, they must convey X3DH init message
				if (i==0) { // first message of the burst, extract and store the X3DH init message
					lime_tester::DR_message_extractX3DHInit(recipient.cipherHeader, X3DH_initMessageBuffer);
				} else { // following message of the burst, extract X3DH init message and compare it to the first one, they must be the same, we shall not create new sessions
					std::vector<uint8_t> X3DH_initMessageBuffer_next{};
					lime_tester::DR_message_extractX3DHInit(recipient.cipherHeader, X3DH_initMessageBuffer_next);
					BC_ASSERT_TRUE(X3DH_initMessageBuffer == X3DH_initMessageBuffer_next);
				}
				BC_ASSERT_TRUE(bobManager->decrypt(recipient.deviceId, "bob", *aliceDevice1, recipient.cipherHeader, *(cipherMessage[i]), receivedMessage));
				std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
				BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[i]);
			}
		}

		if (cleanDatabase) {

			// delete the users
			aliceManager->delete_user(*aliceDevice1, callback);
			bobManager->delete_user(*bobDevice1, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success+2,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data()); // delete the database file if already exists
			remove(dbFilenameBob.data()); // delete the database file if already exists
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE << e;
		BC_FAIL();
	}
}

static void x3dh_operation_queue(void) {
#ifdef EC25519_ENABLED
	x3dh_operation_queue_test(lime::CurveId::c25519, "lime_x3dh_operation_queue", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
	x3dh_operation_queue_test(lime::CurveId::c25519, "lime_x3dh_operation_queue", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data(), false);
#endif
#ifdef EC448_ENABLED
	x3dh_operation_queue_test(lime::CurveId::c448, "lime_x3dh_operation_queue", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
	x3dh_operation_queue_test(lime::CurveId::c448, "lime_x3dh_operation_queue", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data(), false);
#endif
}


 /* A simple test with alice having 1 device and bob 2
 * - Alice and Bob(d1 and d2) register themselves on X3DH server
 * - Alice send message to Bob (d1 and d2)
 * - Alice send another message to Bob (d1 and d2)
 * - Bob d1 respond to Alice(with bob d2 in copy)
 * - Bob d2 respond to Alice(with bob d1 in copy)
 * - Alice send another message to Bob(d1 and d2)
 * - Alice try to send a message to an inexistant device (bob.d3) -> X3DH server send an error back
 * - Delete Alice and Bob devices to leave distant server base clean
 *
 * At each message check that the X3DH init is present or not in the DR header
 * if continuousSession is set to false, delete and recreate LimeManager before each new operation to force relying on local Storage
 * Note: no asynchronous operation will start before the previous is over(callback returns)
 */
static void x3dh_basic_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url, bool continuousSession=true) {
	// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameBob.append(".bob.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});

	try {
		// create Manager
		auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
		auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, X3DHServerPost));

		// create Random devices names
		auto aliceDevice1 = lime_tester::makeRandomDeviceName("alice.d1.");
		auto bobDevice1 = lime_tester::makeRandomDeviceName("bob.d1.");
		auto bobDevice2 = lime_tester::makeRandomDeviceName("bob.d2.");

		// create users
		aliceManager->create_user(*aliceDevice1, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		bobManager->create_user(*bobDevice1, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		bobManager->create_user(*bobDevice2, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this

		/* destroy and reload the Managers(tests everything is correctly saved/load from local Storage) */
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// alice send a message to bob
		auto recipients = make_shared<std::vector<recipientData>>();
		recipients->emplace_back(*bobDevice1);
		recipients->emplace_back(*bobDevice2);
		auto message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto cipherMessage = make_shared<std::vector<uint8_t>>();

		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// loop on cipher message and decrypt with bob Manager
		for (auto &recipient : *recipients) {
			std::vector<uint8_t> receivedMessage{};
			BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(recipient.cipherHeader)); // new sessions created, they must convey X3DH init message
			BC_ASSERT_TRUE(bobManager->decrypt(recipient.deviceId, "bob", *aliceDevice1, recipient.cipherHeader, *cipherMessage, receivedMessage));
			std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
			BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);
		}

		// cleaning
		recipients->clear();
		message = nullptr;
		cipherMessage->clear();
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// encrypt another one, same recipients(we shall have no new X3DH session but still the X3DH init message)
		recipients->emplace_back(*bobDevice1);
		recipients->emplace_back(*bobDevice2);
		message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[1].begin(), lime_tester::messages_pattern[1].end());

		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// loop on cipher message and decrypt with bob Manager
		for (auto &recipient : *recipients) {
			std::vector<uint8_t> receivedMessage{};
			BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(recipient.cipherHeader)); // again X3DH message as no one ever responded to alice.d1
			BC_ASSERT_TRUE(bobManager->decrypt(recipient.deviceId, "bob", *aliceDevice1, recipient.cipherHeader, *cipherMessage, receivedMessage));
			std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
			BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[1]);
		}

		// cleaning
		recipients->clear();
		message = nullptr;
		cipherMessage->clear();
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}


		// bob.d1 reply to alice and copy bob.d2
		recipients->emplace_back(*aliceDevice1);
		recipients->emplace_back(*bobDevice2);
		message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[2].begin(), lime_tester::messages_pattern[2].end());

		bobManager->encrypt(*bobDevice1, make_shared<const std::string>("alice"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// decrypt it
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_FALSE(lime_tester::DR_message_holdsX3DHInit((*recipients)[0].cipherHeader)); // alice.d1 to bob.d1 already set up the DR Session, we shall not have any  X3DH message here
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDevice1, "alice", *bobDevice1, (*recipients)[0].cipherHeader, *cipherMessage, receivedMessage));
		std::string receivedMessageStringAlice{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageStringAlice == lime_tester::messages_pattern[2]);

		receivedMessage.clear();
		BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit((*recipients)[1].cipherHeader)); // bob.d1 to bob.d1 is a new session, we must have a X3DH message here
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice2, "alice", *bobDevice1, (*recipients)[1].cipherHeader, *cipherMessage, receivedMessage));
		std::string receivedMessageStringBob{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageStringBob == lime_tester::messages_pattern[2]);

		// cleaning
		recipients->clear();
		message = nullptr;
		cipherMessage->clear();
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// Now do bob.d2 to alice and bob.d1 every one has an open session towards everyo
		recipients->emplace_back(*aliceDevice1);
		recipients->emplace_back(*bobDevice1);
		message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[3].begin(), lime_tester::messages_pattern[3].end());

		bobManager->encrypt(*bobDevice2, make_shared<const std::string>("alice"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// decrypt it
		receivedMessage.clear();
		BC_ASSERT_FALSE(lime_tester::DR_message_holdsX3DHInit((*recipients)[0].cipherHeader)); // alice.d1 to bob.d2 already set up the DR Session, we shall not have any  X3DH message here
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDevice1, "alice", *bobDevice2, (*recipients)[0].cipherHeader, *cipherMessage, receivedMessage));
		receivedMessageStringAlice = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageStringAlice == lime_tester::messages_pattern[3]);
		receivedMessage.clear();
		BC_ASSERT_FALSE(lime_tester::DR_message_holdsX3DHInit((*recipients)[1].cipherHeader)); // bob.d1 to bob.d2 already set up the DR Session, we shall not have any  X3DH message here
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice1, "alice", *bobDevice2, (*recipients)[1].cipherHeader, *cipherMessage, receivedMessage));
		receivedMessageStringBob = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageStringBob == lime_tester::messages_pattern[3]);

		// cleaning
		recipients->clear();
		message = nullptr;
		cipherMessage->clear();
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// encrypt another one from alice to bob.d1 and .d2, it must not send X3DH init anymore
		recipients->emplace_back(*bobDevice1);
		recipients->emplace_back(*bobDevice2);
		message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[4].begin(), lime_tester::messages_pattern[4].end());

		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// loop on cipher message and decrypt with bob Manager
		for (auto &recipient : *recipients) {
			std::vector<uint8_t> receivedMessage{};
			BC_ASSERT_FALSE(lime_tester::DR_message_holdsX3DHInit(recipient.cipherHeader)); // bob.d1 and d2 both responded, so no more X3DH message shall be sent
			BC_ASSERT_TRUE(bobManager->decrypt(recipient.deviceId, "bob", *aliceDevice1, recipient.cipherHeader, *cipherMessage, receivedMessage));
			std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
			BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[4]);
		}

		// cleaning
		recipients->clear();
		message = nullptr;
		cipherMessage->clear();
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// try to encrypt to an unknown user, it must fail in callback
		recipients->emplace_back("bob.d3");
		message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[5].begin(), lime_tester::messages_pattern[5].end());

		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_failed,1,lime_tester::wait_for_timeout));

		// cleaning
		recipients->clear();
		message = nullptr;
		cipherMessage->clear();
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// delete the users
		if (cleanDatabase) {
			aliceManager->delete_user(*aliceDevice1, callback);
			bobManager->delete_user(*bobDevice1, callback);
			bobManager->delete_user(*bobDevice2, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,expected_success+3,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}
}

static void x3dh_basic(void) {
#ifdef EC25519_ENABLED
	x3dh_basic_test(lime::CurveId::c25519, "lime_x3dh_basic", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
	x3dh_basic_test(lime::CurveId::c25519, "lime_x3dh_basic", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data(), false);
#endif
#ifdef EC448_ENABLED
	x3dh_basic_test(lime::CurveId::c448, "lime_x3dh_basic", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
	x3dh_basic_test(lime::CurveId::c448, "lime_x3dh_basic", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data(), false);
#endif
}

/**
 * NOTE: this test uses low level user management functions which are not available to lib user which have access to
 * LimeManager functions only. Do not consider this test code as an example as it is not the way users shall
 * be managed by library end users.
 *
 * - Create alice using LimeManager
 * - Load alice directly using load_limeUser (it's for test only, this is not really available to lib user)
 * - Fail at creating alice again in the same DB
 * - Fail at loading an unknown user
 * - Delete Alice through LimeManager
 * - Fail at deleting an unknown user
 * - Create Alice again
 * - using an other DB, create alice, success on local Storage but shall get a callback error
 * - clean DB by deleting alice
 *
 */
static void user_management_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url ) {

	std::string dbFilenameAlice{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append((curve==CurveId::c25519)?"C25519":"C448").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGI <<"Insert Lime user failed : "<< anythingToSay.data() ;
					}
				});
	// we need a LimeManager
	auto Manager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, X3DHServerPost));
	auto aliceDeviceName = lime_tester::makeRandomDeviceName("alice.");

	try {
		/* create a user in a fresh database */
		Manager->create_user(*aliceDeviceName, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this

		/* load alice from from DB */
		auto alice = load_LimeUser(dbFilenameAlice, *aliceDeviceName, X3DHServerPost);
		/* no need to wait here, it shall load alice immediately */
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL();
	}

	bool gotExpectedException = false;
	/* Try to create the same user in the same data base, it must fail with exception raised */
	try {
		auto alice = insert_LimeUser(dbFilenameAlice, *aliceDeviceName, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, X3DHServerPost, callback);
		/* no need to wait here, it must fail immediately */
	} catch (BctbxException &e) {
		gotExpectedException = true;
	}
	if (!gotExpectedException) {
		// we didn't got any exception on trying to insert alice again in DB
		BC_FAIL("Insert twice the same userID in a DB without getting any exception");
		return;
	}

	/* Try to load a user which is not in DB, it must fail with exception raised */
	gotExpectedException = false;
	try {
		auto alice = load_LimeUser(dbFilenameAlice, "bob", X3DHServerPost);
		/* no need to wait here, it must fail immediately */
	} catch (BctbxException &e) {
		gotExpectedException = true;
	}
	if (!gotExpectedException) {
		// we didn't got any exception on trying to load bob from DB
		BC_FAIL("No exception arised when loading inexistent user from DB");
		return;
	}

	/* Delete users from base */
	try {
		// delete Alice, wait for callback confirmation from x3dh server
		Manager->delete_user(*aliceDeviceName, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;
		BC_FAIL("Delete Lime user raised exception");
		return;
	}
	gotExpectedException = false;
	auto savedCounters = counters;
	try {
		// delete bob which is not there, it shall raise an exception and never get to the callback
		Manager->delete_user("bob", callback);
	} catch (BctbxException &e) {
		gotExpectedException = true;
	}
	if (!gotExpectedException) {
		// we didn't got any exception on trying to delete bob from DB while he isn't theee
		BC_FAIL("No exception arised when deleting inexistent user from DB");
		return;
	}
	BC_ASSERT_FALSE(lime_tester::wait_for(stack,&counters.operation_failed,savedCounters.operation_failed+1,lime_tester::wait_for_timeout/2)); // give we few seconds to possible call to the callback(it shall not occurs
	// check we didn't land into callback as any call to it will modify the counters
	BC_ASSERT_TRUE(counters == savedCounters);


	/* Create Alice again */
	try {
		std::shared_ptr<LimeGeneric> alice = insert_LimeUser(dbFilenameAlice, *aliceDeviceName, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, X3DHServerPost, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// create another manager with a fresh DB
		std::string dbFilenameAliceTmp{dbFilenameAlice};
		dbFilenameAliceTmp.append(".tmp.sqlite3");

		// create a manager and try to create alice again, it shall pass local creation(db is empty) but server shall reject it
		auto ManagerTmp = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAliceTmp, X3DHServerPost));

		ManagerTmp->create_user(*aliceDeviceName, x3dh_server_url, curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_failed,counters.operation_failed+1,lime_tester::wait_for_timeout)); // wait on this one but we shall get a fail from server

		/* Clean DB */
		if (cleanDatabase) {
			// delete Alice, wait for callback confirmation from server
			Manager->delete_user(*aliceDeviceName, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data()); // delete the database file
			remove(dbFilenameAliceTmp.data()); // delete the database file
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;
		BC_FAIL();
	}
}

static void user_management(void) {
#ifdef EC25519_ENABLED
	user_management_test(lime::CurveId::c25519, "lime_user_management", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
#endif
#ifdef EC448_ENABLED
	user_management_test(lime::CurveId::c448, "lime_user_management", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
#endif
}

static test_t tests[] = {
	TEST_NO_TAG("User Management", user_management),
	TEST_NO_TAG("Basic", x3dh_basic),
	TEST_NO_TAG("Queued encryption", x3dh_operation_queue),
	TEST_NO_TAG("Multi devices queued encryption", x3dh_multidev_operation_queue),
	TEST_NO_TAG("Multiple sessions", x3dh_multiple_DRsessions),
	TEST_NO_TAG("Sending chain limit", x3dh_sending_chain_limit),
	TEST_NO_TAG("Without OPk", x3dh_without_OPk),
	TEST_NO_TAG("Update - clean MK", lime_update_clean_MK),
	TEST_NO_TAG("Update - SPk", lime_update_SPk),
	TEST_NO_TAG("Update - OPk", lime_update_OPk),
	TEST_NO_TAG("get self Identity Key", lime_getSelfIk),
	TEST_NO_TAG("Verified Status", lime_identityVerifiedStatus)
};

test_suite_t lime_lime_test_suite = {
	"Lime",
	http_before_all,
	http_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
