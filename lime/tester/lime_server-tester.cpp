/*
	lime_server-tester.cpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2020  Belledonne Communications SARL

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

struct C_Callback_userData {
	const limeX3DHServerResponseProcess responseProcess;
	const std::string username; // the username sending message, used for logs
	C_Callback_userData(const limeX3DHServerResponseProcess &response, const std::string &username) : responseProcess(response), username{username} {};
};

static void process_io_error(void *data, const belle_sip_io_error_event_t *event) noexcept{
	C_Callback_userData *userData = static_cast<C_Callback_userData *>(data);
	LIME_LOGI<<"IO Error on X3DH server request from user "<<userData->username;
	(userData->responseProcess)(0, std::vector<uint8_t>{});
	delete(userData);
}

static void process_response(void *data, const belle_http_response_event_t *event) noexcept {
	C_Callback_userData *userData = static_cast<C_Callback_userData *>(data);
	LIME_LOGI<<"Response from X3DH server for user "<<userData->username;
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
	// store a reference to the responseProcess function in a wrapper as belle-sip request C-style callbacks with a void * user data parameter, C++ implementation shall
	// use lambda and capture the function.
	C_Callback_userData *userData = new C_Callback_userData(responseProcess, from); // create on the heap a copy of the responseProcess closure so it's available when we're called back by belle-sip
	l=belle_http_request_listener_create_from_callbacks(&cbs, userData);
	belle_sip_object_data_set(BELLE_SIP_OBJECT(req), "http_request_listener", l, belle_sip_object_unref); // Ensure the listener object is destroyed when the request is destroyed
	LIME_LOGI<<"user "<<from<<"post a request to X3DH server";
	belle_http_provider_send_request(prov,req,l);
});

/**
 * Scenario: Server is set to accept 200 OPks per device
 * - Create user alice with 500 OPks -> it shall fail
 * - Create user alice with 100 OPks -> it shall pass
 * - Add 100 OPks to alice -> it shall pass
 * - Add 100 OPks to alice -> is shall fail
 */
static void lime_server_resource_limit_reached_test(const lime::CurveId curve) {
	const std::string dbBaseFilename{"lime_server_resource_limit_reached"};
	// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append(CurveId2String(curve)).append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;
	int expected_failure=0;

	limeCallback callback = [&counters](lime::CallbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::CallbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						LIME_LOGE<<"Lime operation failed : "<<anythingToSay;
					}
				};
	try {
		std::vector<lime::CurveId> algos{curve};
		// create Manager and device for alice
		auto aliceManager = std::make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.");
		aliceManager->create_user(*aliceDeviceId, algos, lime_tester::test_x3dh_default_server, 500, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_failed, ++expected_failure,lime_tester::wait_for_timeout));

		// Try again to create the device, with 100 OPks, it shall pass
		aliceManager->create_user(*aliceDeviceId, algos, lime_tester::test_x3dh_default_server, 100, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// call the update, set the serverLimit 200 and upload an other 100
		aliceManager=nullptr; // destroy manager before modifying DB
		lime_tester::forwardTime(dbFilenameAlice, 2);
		aliceManager = std::make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		aliceManager->update(*aliceDeviceId, algos, callback, 200, 100);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
		BC_ASSERT_EQUAL((int)lime_tester::get_OPks(dbFilenameAlice, *aliceDeviceId, curve), 200, int, "%d");

		// call the update, set the serverLimit 300 and upload an other 100 -> it shall fail but we have 300 OPks in DB
		aliceManager=nullptr; // destroy manager before modifying DB
		lime_tester::forwardTime(dbFilenameAlice, 2);
		aliceManager = std::make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		aliceManager->update(*aliceDeviceId, algos, callback, 300, 100);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_failed, ++expected_failure,lime_tester::wait_for_timeout));
		BC_ASSERT_EQUAL((int)lime_tester::get_OPks(dbFilenameAlice, *aliceDeviceId, curve), 300, int, "%d");

		// update again, with correct values, server already holds 200 keys, so the only effect would be to set the failed 100 OPks status to dispatched as they are not on server
		aliceManager=nullptr; // destroy manager before modifying DB
		lime_tester::forwardTime(dbFilenameAlice, 2);
		aliceManager = std::make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		aliceManager->update(*aliceDeviceId, algos, callback, 125, 25);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
		BC_ASSERT_EQUAL((int)lime_tester::get_OPks(dbFilenameAlice, *aliceDeviceId, curve), 300, int, "%d");

		// forward time by OPK_limboTime_days
		aliceManager=nullptr; // destroy manager before modifying DB
		lime_tester::forwardTime(dbFilenameAlice, lime::settings::OPk_limboTime_days+1);
		aliceManager = std::make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);

		// update one last time, with correct values, server already holds 200 keys
		// so the only effect would be to remove the OPk keys status was set to dispatch before we forward the time
		// We now hold 200 keys
		aliceManager->update(*aliceDeviceId, algos, callback, 125, 25);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
		BC_ASSERT_EQUAL((int)lime_tester::get_OPks(dbFilenameAlice, *aliceDeviceId, curve), 200, int, "%d");

		if (cleanDatabase) {
			aliceManager->delete_user(DeviceId(*aliceDeviceId, curve), callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data());
		}
	} catch (BctbxException &e) {
		LIME_LOGE << e;
		BC_FAIL("");
	}
}

static void lime_server_resource_limit_reached() {
#ifdef EC25519_ENABLED
	lime_server_resource_limit_reached_test(lime::CurveId::c25519);
#endif
#ifdef EC448_ENABLED
	lime_server_resource_limit_reached_test(lime::CurveId::c448);
#endif
#ifdef HAVE_BCTBXPQ
#ifdef EC25519_ENABLED
	lime_server_resource_limit_reached_test(lime::CurveId::c25519k512);

	lime_server_resource_limit_reached_test(lime::CurveId::c25519mlk512);
#endif
#ifdef EC448_ENABLED
	lime_server_resource_limit_reached_test(lime::CurveId::c448mlk1024);
#endif
#endif
}

/**
 * Scenario: Server is set to deliver 5 OPks per device per week
 * - Create user alice with 10 OPks
 * - Create user bob with 10 OPks
 * - repeat bundle_request_limit (shall be 5) times:
 *   - alice sends a message to bob
 *   - bob sends a message to alice
 *   - alice deletes bobs from her local storage
 * - repeat again, alice shall not obtain an OPk or even bob's info depending on server's configuration
 *   - blocking server is configured with very short block time span. Wait and try again, it shall now work
 */
static void lime_server_bundle_request_limit_reached_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url, bool always_served=true) {
	// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	std::string dbFilenameBob{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append(CurveId2String(curve)).append(".sqlite3");
	dbFilenameBob.append(".bob.").append(CurveId2String(curve)).append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;
	int expected_failure=0;

	limeCallback callback = [&counters](lime::CallbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::CallbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						LIME_LOGE<<"Lime operation failed : "<<anythingToSay;
					}
				};
	try {
		std::vector<lime::CurveId> algos{curve};
		// create Manager and device for alice and bob
		auto aliceManager = make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.");
		auto bobDeviceId = lime_tester::makeRandomDeviceName("bob.");
		aliceManager->create_user(*aliceDeviceId, algos, x3dh_server_url, 10, callback);
		bobManager->create_user(*bobDeviceId, algos, x3dh_server_url, 10, callback);
		expected_success += 2;
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, expected_success,lime_tester::wait_for_timeout));
		if (counters.operation_success<2) return; // skip the end of the test if we can't do this

		for (auto i=0; i<lime_tester::bundle_request_limit; i++) {

			// alice encrypts a message for bob
			auto enc = make_shared<lime::EncryptionContext>("bob", lime_tester::messages_pattern[2*i]);
			enc->addRecipient(*bobDeviceId);
			aliceManager->encrypt(*aliceDeviceId, algos, enc, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

			// Bob decrypts the message, it shall hold a X3DH init with an OPk
			std::vector<uint8_t> receivedMessage{};
			bool with_OPk = false;
			BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(enc->m_recipients[0].DRmessage, with_OPk)); // new sessions created, they must convey X3DH init message
			BC_ASSERT_TRUE(with_OPk==true); // it must hold an OPk
			// on decrypt as Bob may already received message from alice, status could be untrusted or unknown, just check it is not fail
			BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i]);

			// bob encrypts a message for alice (so next time Alice writes to Bob it should not embed an X3DHInit)
			enc = make_shared<lime::EncryptionContext>("alice", lime_tester::messages_pattern[2*i+1]);
			enc->addRecipient(*aliceDeviceId);
			bobManager->encrypt(*bobDeviceId, algos, enc, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

			// Alice decrypts the message, it shall not hold a X3DH init as it is a reply from Bob
			receivedMessage.clear();
			BC_ASSERT_FALSE(lime_tester::DR_message_holdsX3DHInit(enc->m_recipients[0].DRmessage));
			BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "alice", *bobDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) == lime::PeerDeviceStatus::untrusted);
			BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i+1]);

			// Alice deletes bob's device from local DB to force fetching of a new key bundle
			aliceManager->delete_peerDevice(*bobDeviceId);
		}

		//  Now alice tries again to encrypt but server shall not give her any OPk
		bool expected_hasOPk = false; // we expect to produce a message without OPk
		auto enc = make_shared<lime::EncryptionContext>("bob", lime_tester::messages_pattern[0]);
		enc->addRecipient(*bobDeviceId);
		aliceManager->encrypt(*aliceDeviceId, algos, enc, callback);
		if (always_served == false) {
			// server is expected to not serve the key bundle at all, the encryption will fail as all our recipients(it's just bob) have no bundle
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_failed,++expected_failure,lime_tester::wait_for_timeout));
			// But blocking server is configured to have a very short time windows on its requests list: 20s. Wait and try again
			int dummy=0;
			BC_ASSERT_FALSE(lime_tester::wait_for(bc_stack,&dummy,1,lime_tester::bundle_request_limit_timespan));
			enc = make_shared<lime::EncryptionContext>("bob", lime_tester::messages_pattern[0]);
			enc->addRecipient(*bobDeviceId);
			aliceManager->encrypt(*aliceDeviceId, algos, enc, callback);
			expected_hasOPk = true; // Waiting remove the ban, this request should be served with an OPk
		}

		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// Bob decrypts the message, it shall hold a X3DH init with an OPk
		std::vector<uint8_t> receivedMessage{};
		bool with_OPk = !expected_hasOPk;
		BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(enc->m_recipients[0].DRmessage, with_OPk)); // new sessions created, they must convey X3DH init message
		BC_ASSERT_TRUE(with_OPk==expected_hasOPk);
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) == lime::PeerDeviceStatus::untrusted);
		BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[0]);

		if (cleanDatabase) {
			aliceManager->delete_user(DeviceId(*aliceDeviceId, curve), callback);
			bobManager->delete_user(DeviceId(*bobDeviceId, curve), callback);
			expected_success += 2;
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		LIME_LOGE << e;
		BC_FAIL("");
	}

}

static void lime_server_bundle_request_limit_reached() {
#ifdef EC25519_ENABLED
	lime_server_bundle_request_limit_reached_test(lime::CurveId::c25519, "lime_server_bundle_request_limit_reached_keep", lime_tester::test_x3dh_default_server);
#endif
#ifdef EC448_ENABLED
	lime_server_bundle_request_limit_reached_test(lime::CurveId::c448, "lime_server_bundle_request_limit_reached_keep", lime_tester::test_x3dh_default_server);
#endif
#ifdef HAVE_BCTBXPQ
#ifdef EC25519_ENABLED
	lime_server_bundle_request_limit_reached_test(lime::CurveId::c25519k512, "lime_server_bundle_request_limit_reached_keep", lime_tester::test_x3dh_default_server);

	lime_server_bundle_request_limit_reached_test(lime::CurveId::c25519mlk512, "lime_server_bundle_request_limit_reached_keep", lime_tester::test_x3dh_default_server);
#endif
#ifdef EC448_ENABLED
	lime_server_bundle_request_limit_reached_test(lime::CurveId::c448mlk1024, "lime_server_bundle_request_limit_reached_keep", lime_tester::test_x3dh_default_server);
#endif
#endif
}

static void lime_server_bundle_request_limit_reached_stop_serving() {
#ifdef EC25519_ENABLED
	lime_server_bundle_request_limit_reached_test(lime::CurveId::c25519, "lime_server_bundle_request_limit_reached_stop", lime_tester::test_x3dh_stop_on_request_limit_server, false);
#endif
#ifdef EC448_ENABLED
	lime_server_bundle_request_limit_reached_test(lime::CurveId::c448, "lime_server_bundle_request_limit_reached_stop", lime_tester::test_x3dh_stop_on_request_limit_server, false);
#endif
#ifdef HAVE_BCTBXPQ
#ifdef EC25519_ENABLED
	lime_server_bundle_request_limit_reached_test(lime::CurveId::c25519k512, "lime_server_bundle_request_limit_reached_stop", lime_tester::test_x3dh_stop_on_request_limit_server, false);

	lime_server_bundle_request_limit_reached_test(lime::CurveId::c25519mlk512, "lime_server_bundle_request_limit_reached_stop", lime_tester::test_x3dh_stop_on_request_limit_server, false);
#endif
#ifdef EC448_ENABLED
	lime_server_bundle_request_limit_reached_test(lime::CurveId::c448mlk1024, "lime_server_bundle_request_limit_reached_stop", lime_tester::test_x3dh_stop_on_request_limit_server, false);
#endif
#endif
}

static void lime_server_bundle_request_limit_reached_multiple_users_test(const lime::CurveId curve) {
	const std::string dbBaseFilename{"lime_server_bundle_request_limit_reached_multiple_users"};	
	// create DB
	std::string dbFilenameAlice{dbBaseFilename};
	std::string dbFilenameBob{dbBaseFilename};
	std::string dbFilenameClaire{dbBaseFilename};
	dbFilenameAlice.append(".alice.").append(CurveId2String(curve)).append(".sqlite3");
	dbFilenameBob.append(".bob.").append(CurveId2String(curve)).append(".sqlite3");
	dbFilenameClaire.append(".bob.").append(CurveId2String(curve)).append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists
	remove(dbFilenameClaire.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback = [&counters](lime::CallbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::CallbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						LIME_LOGE<<"Lime operation failed : "<<anythingToSay;
					}
				};
	try {
		std::vector<lime::CurveId> algos{curve};
		// create Manager and devices for alice, bob and claire
		auto aliceManager = make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);
		auto claireManager = make_unique<LimeManager>(dbFilenameClaire, X3DHServerPost);
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.1.");
		auto bobDeviceId = lime_tester::makeRandomDeviceName("bob.1.");
		auto claireDeviceId = lime_tester::makeRandomDeviceName("claire.1.");
		auto aliceDeviceId2 = lime_tester::makeRandomDeviceName("alice.2.");
		auto bobDeviceId2 = lime_tester::makeRandomDeviceName("bob.2.");
		auto claireDeviceId2 = lime_tester::makeRandomDeviceName("claire.2.");
		aliceManager->create_user(*aliceDeviceId, algos, lime_tester::test_x3dh_default_server, 10, callback);
		bobManager->create_user(*bobDeviceId, algos, lime_tester::test_x3dh_default_server, 10, callback);
		claireManager->create_user(*claireDeviceId, algos, lime_tester::test_x3dh_default_server, 10, callback);
		aliceManager->create_user(*aliceDeviceId2, algos, lime_tester::test_x3dh_default_server, 10, callback);
		bobManager->create_user(*bobDeviceId2, algos, lime_tester::test_x3dh_default_server, 10, callback);
		claireManager->create_user(*claireDeviceId2, algos, lime_tester::test_x3dh_default_server, 10, callback);
		expected_success += 6;
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, expected_success,lime_tester::wait_for_timeout));
		if (counters.operation_success<2) return; // skip the end of the test if we can't do this

		for (auto i=0; i<lime_tester::bundle_request_limit; i++) {

			// alice encrypts a message for everyone
			auto enc = make_shared<lime::EncryptionContext>("friends", lime_tester::messages_pattern[2*i]);
			enc->addRecipient(*aliceDeviceId2);
			enc->addRecipient(*bobDeviceId);
			enc->addRecipient(*bobDeviceId2);
			enc->addRecipient(*claireDeviceId);
			enc->addRecipient(*claireDeviceId2);
			aliceManager->encrypt(*aliceDeviceId, algos, enc, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			// all messages should hold an OPk
			for (auto &recipient : enc->m_recipients) {
				bool with_OPk = false;
				BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(recipient.DRmessage, with_OPk));
				BC_ASSERT_TRUE(with_OPk==true);
			}

			// Everyone decrypts the message
			std::vector<uint8_t> receivedMessage{};
			BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId2, "friends", *aliceDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i]);
			receivedMessage.clear();

			BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "friends", *aliceDeviceId, enc->m_recipients[1].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i]);
			receivedMessage.clear();

			BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId2, "friends", *aliceDeviceId, enc->m_recipients[2].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i]);
			receivedMessage.clear();

			BC_ASSERT_TRUE(claireManager->decrypt(*claireDeviceId, "friends", *aliceDeviceId, enc->m_recipients[3].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i]);
			receivedMessage.clear();

			BC_ASSERT_TRUE(claireManager->decrypt(*claireDeviceId2, "friends", *aliceDeviceId, enc->m_recipients[4].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i]);
			receivedMessage.clear();

			// everyone reply to alice and she decrypts(so her next message should not hold a X3DH init)
			enc = make_shared<lime::EncryptionContext>("friends", lime_tester::messages_pattern[2*i+1]);
			enc->addRecipient(*aliceDeviceId);
			aliceManager->encrypt(*aliceDeviceId2, algos, enc, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			receivedMessage.clear();
			// alice2 is a local device for alice, so it is trusted
			BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "friends", *aliceDeviceId2, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) == lime::PeerDeviceStatus::trusted);
			BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i+1]);

			enc = make_shared<lime::EncryptionContext>("friends", lime_tester::messages_pattern[2*i+1]);
			enc->addRecipient(*aliceDeviceId);
			bobManager->encrypt(*bobDeviceId, algos, enc, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			receivedMessage.clear();
			BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "friends", *bobDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) == lime::PeerDeviceStatus::untrusted);
			BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i+1]);

			enc = make_shared<lime::EncryptionContext>("friends", lime_tester::messages_pattern[2*i+1]);
			enc->addRecipient(*aliceDeviceId);
			bobManager->encrypt(*bobDeviceId2, algos, enc, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			receivedMessage.clear();
			BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "friends", *bobDeviceId2, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) == lime::PeerDeviceStatus::untrusted);
			BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i+1]);

			enc = make_shared<lime::EncryptionContext>("friends", lime_tester::messages_pattern[2*i+1]);
			enc->addRecipient(*aliceDeviceId);
			claireManager->encrypt(*claireDeviceId, algos, enc, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			receivedMessage.clear();
			BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "friends", *claireDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) == lime::PeerDeviceStatus::untrusted);
			BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i+1]);

			// On first round, claire2 does not reply and is not deleted
			// So alice message on successive round will still hold an X3DH Init message but won't fetch a new bundle
			if (i>0) {
				enc = make_shared<lime::EncryptionContext>("friends", lime_tester::messages_pattern[2*i+1]);
				enc->addRecipient(*aliceDeviceId);
				claireManager->encrypt(*claireDeviceId2, algos, enc, callback);
				BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
				receivedMessage.clear();
				BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "friends", *claireDeviceId2, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) == lime::PeerDeviceStatus::untrusted);
				BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2*i+1]);
			}

			// Alice deletes friends devices from local DB to force fetching of a new key bundle
			aliceManager->delete_peerDevice(*aliceDeviceId2);
			aliceManager->delete_peerDevice(*bobDeviceId);
			aliceManager->delete_peerDevice(*bobDeviceId2);
			aliceManager->delete_peerDevice(*claireDeviceId);
			if (i>0) { // skip this one on first round
				aliceManager->delete_peerDevice(*claireDeviceId2);
			}
		}

		//  Now alice tries again to encrypt but server shall not give her any OPk
		auto enc = make_shared<lime::EncryptionContext>("friends", lime_tester::messages_pattern[0]);
		enc->addRecipient(*aliceDeviceId2);
		enc->addRecipient(*bobDeviceId);
		enc->addRecipient(*bobDeviceId2);
		enc->addRecipient(*claireDeviceId);
		enc->addRecipient(*claireDeviceId2);
		aliceManager->encrypt(*aliceDeviceId, algos, enc, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		// claire2 message should still hold an OPk but not the others
		bool with_OPk = true;
		BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(enc->m_recipients[0].DRmessage, with_OPk));
		BC_ASSERT_TRUE(with_OPk==false);
		BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(enc->m_recipients[1].DRmessage, with_OPk));
		BC_ASSERT_TRUE(with_OPk==false);
		BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(enc->m_recipients[2].DRmessage, with_OPk));
		BC_ASSERT_TRUE(with_OPk==false);
		BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(enc->m_recipients[3].DRmessage, with_OPk));
		BC_ASSERT_TRUE(with_OPk==false);
		BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(enc->m_recipients[4].DRmessage, with_OPk));
		BC_ASSERT_TRUE(with_OPk==true);

		if (cleanDatabase) {
			aliceManager->delete_user(DeviceId(*aliceDeviceId, curve), callback);
			aliceManager->delete_user(DeviceId(*aliceDeviceId2, curve), callback);
			bobManager->delete_user(DeviceId(*bobDeviceId, curve), callback);
			bobManager->delete_user(DeviceId(*bobDeviceId2, curve), callback);
			claireManager->delete_user(DeviceId(*claireDeviceId, curve), callback);
			claireManager->delete_user(DeviceId(*claireDeviceId2, curve), callback);
			expected_success += 6;
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		LIME_LOGE << e;
		BC_FAIL("");
	}

}

static void lime_server_bundle_request_limit_reached_multiple_users() {
#ifdef EC25519_ENABLED
	lime_server_bundle_request_limit_reached_multiple_users_test(lime::CurveId::c25519);
#endif
#ifdef EC448_ENABLED
	lime_server_bundle_request_limit_reached_multiple_users_test(lime::CurveId::c448);
#endif
#ifdef HAVE_BCTBXPQ
#ifdef EC25519_ENABLED
	lime_server_bundle_request_limit_reached_multiple_users_test(lime::CurveId::c25519k512);

	lime_server_bundle_request_limit_reached_multiple_users_test(lime::CurveId::c25519mlk512);
#endif
#ifdef EC448_ENABLED
	lime_server_bundle_request_limit_reached_multiple_users_test(lime::CurveId::c448mlk1024);
#endif
#endif
}

static test_t tests[] = {
	TEST_NO_TAG("Server resource limit reached", lime_server_resource_limit_reached),
	TEST_NO_TAG("Server bundle request limit reached: server keep sending bundle without OPk", lime_server_bundle_request_limit_reached),
	TEST_NO_TAG("Server bundle request limit reached: server stop serving request", lime_server_bundle_request_limit_reached_stop_serving),
	TEST_NO_TAG("Server bundle request limit reached: multiple users", lime_server_bundle_request_limit_reached_multiple_users),
};

test_suite_t lime_server_test_suite = {
	"Lime server",
	http_before_all,
	http_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests,
	0,
	0
};
