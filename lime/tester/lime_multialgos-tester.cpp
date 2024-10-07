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

#include "lime_log.hpp"
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
#include <chrono>
#include <thread>
#include <deque>
#include <mutex>
#include <list>

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

/* This function will destroy and recreate managers given in parameter, force deleting all internal cache and start back from what is in local Storage */
static void managersClean(std::unique_ptr<LimeManager> &alice, std::unique_ptr<LimeManager> &bob, std::string aliceDb, std::string bobDb) {
	alice = nullptr;
	bob = nullptr;
	alice = make_unique<LimeManager>(aliceDb, X3DHServerPost);
	bob = make_unique<LimeManager>(bobDb, X3DHServerPost);
	LIME_LOGI<<"Trash and reload alice and bob LimeManagers";
}


static bool multialgos_basic_test(const std::vector<lime::CurveId> aliceAlgos, const std::vector<lime::CurveId>bobAlgos, lime::EncryptionPolicy policy, bool continuousSession=true) {
	std::string dbBaseFilename("multialgos_basic");
	bool ret = true;
	lime_tester::events_counters_t counters={};
	int expected_success=0;

	auto callback = make_shared<limeCallback>([&counters](lime::CallbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::CallbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						LIME_LOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});

	try {
		// create DB
		auto dbFilenameAlice = dbBaseFilename;
		dbFilenameAlice.append(".alice.").append(CurveId2String(aliceAlgos, "-")).append(".sqlite3");
		auto dbFilenameBob = dbBaseFilename;
		dbFilenameBob.append(".bob.").append(CurveId2String(bobAlgos, "-")).append(".sqlite3");

		remove(dbFilenameAlice.data()); // delete the database file if already exists
		remove(dbFilenameBob.data()); // delete the database file if already exists

		// create Manager and device for alice
		auto aliceManager = make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.d.");
		aliceManager->create_user(*aliceDeviceId, aliceAlgos, lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
		ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for bob
		auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);
		auto bobDeviceId = lime_tester::makeRandomDeviceName("bob.d");
		bobManager->create_user(*bobDeviceId, bobAlgos, lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
		ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		/* destroy and reload the Managers(tests everything is correctly saved/load from local Storage) */
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// alice send a message to bob
		auto recipients = make_shared<std::vector<RecipientData>>();
		recipients->emplace_back(*bobDeviceId);
		auto message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto cipherMessage = make_shared<std::vector<uint8_t>>();

		aliceManager->encrypt(*aliceDeviceId, aliceAlgos, make_shared<const std::string>("bob"), recipients, message, cipherMessage, callback, policy);
		ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// decrypt with bob Manager
		std::vector<uint8_t> receivedMessage{};
		ret &= BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit((*recipients)[0].DRmessage)); // new sessions created, they must convey X3DH init message
		ret &= BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, (*recipients)[0].DRmessage, *cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
		ret &= BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// bob replies to alice
		recipients->clear();
		recipients->emplace_back(*aliceDeviceId);
		message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[1].begin(), lime_tester::messages_pattern[1].end());
		bobManager->encrypt(*bobDeviceId, bobAlgos, make_shared<const std::string>("alice"), recipients, message, cipherMessage, callback, policy);
		ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// decrypt it
		receivedMessage.clear();
		ret &= BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "alice", *bobDeviceId, (*recipients)[0].DRmessage, *cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		std::string receivedMessageStringAlice{receivedMessage.begin(), receivedMessage.end()};
		ret &= BC_ASSERT_TRUE(receivedMessageStringAlice == lime_tester::messages_pattern[1]);

		// delete the users
		if (cleanDatabase) {
			for (const auto &algo:aliceAlgos) {
				aliceManager->delete_user(DeviceId(*aliceDeviceId, algo), callback);
				ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			}
			for (const auto &algo:bobAlgos) {
				bobManager->delete_user(DeviceId(*bobDeviceId, algo), callback);
				ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			}
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		LIME_LOGE << e;
		BC_FAIL("");
		return false;
	}
	return ret;
}

static void multialgos_basic(void) {
#if defined(EC25519_ENABLED) && defined(HAVE_BCTBXPQ)
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c25519k512}, lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c25519k512}, lime::EncryptionPolicy::DRMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c25519k512}, lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c25519k512}, lime::EncryptionPolicy::cipherMessage,false));
#endif
}


static bool multialgos_three_users_test(const std::vector<lime::CurveId> aliceAlgos, const std::vector<lime::CurveId> bobAlgos, const std::vector<lime::CurveId> claireAlgos, lime::EncryptionPolicy policy) {
	std::string dbBaseFilename("multialgos_three_users");
	bool ret = true;
	lime_tester::events_counters_t counters={};
	int expected_success=0;

	auto callback = make_shared<limeCallback>([&counters](lime::CallbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::CallbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						LIME_LOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});

	try {
		// create DB
		auto dbFilenameAlice = dbBaseFilename;
		dbFilenameAlice.append(".alice.").append(CurveId2String(aliceAlgos, "-")).append(".sqlite3");
		auto dbFilenameBob = dbBaseFilename;
		dbFilenameBob.append(".bob.").append(CurveId2String(bobAlgos, "-")).append(".sqlite3");
		auto dbFilenameClaire = dbBaseFilename;
		dbFilenameClaire.append(".claire.").append(CurveId2String(claireAlgos, "-")).append(".sqlite3");

		remove(dbFilenameAlice.data()); // delete the database file if already exists
		remove(dbFilenameBob.data()); // delete the database file if already exists
		remove(dbFilenameClaire.data()); // delete the database file if already exists

		// create Manager and device for alice
		auto aliceManager = make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.d.");
		aliceManager->create_user(*aliceDeviceId, aliceAlgos, lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
		ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for bob
		auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);
		auto bobDeviceId = lime_tester::makeRandomDeviceName("bob.d");
		bobManager->create_user(*bobDeviceId, bobAlgos, lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
		ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for claire
		auto claireManager = make_unique<LimeManager>(dbFilenameClaire, X3DHServerPost);
		auto claireDeviceId = lime_tester::makeRandomDeviceName("claire.d");
		claireManager->create_user(*claireDeviceId, claireAlgos, lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
		ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// alice send a message to bob and claire
		auto recipients = make_shared<std::vector<RecipientData>>();
		recipients->emplace_back(*bobDeviceId);
		recipients->emplace_back(*claireDeviceId);
		auto message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto cipherMessage = make_shared<std::vector<uint8_t>>();

		aliceManager->encrypt(*aliceDeviceId, aliceAlgos, make_shared<const std::string>("friends"), recipients, message, cipherMessage, callback, policy);
		ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// decrypt with bob Manager
		std::vector<uint8_t> receivedMessage{};
		ret &= BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit((*recipients)[0].DRmessage)); // new sessions created, they must convey X3DH init message
		ret &= BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "friends", *aliceDeviceId, (*recipients)[0].DRmessage, *cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
		ret &= BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// decrypt with claire Manager
		receivedMessage.clear();
		ret &= BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit((*recipients)[1].DRmessage)); // new sessions created, they must convey X3DH init message
		ret &= BC_ASSERT_TRUE(claireManager->decrypt(*claireDeviceId, "friends", *aliceDeviceId, (*recipients)[1].DRmessage, *cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		ret &= BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// bob replies to alice and claire
		recipients->clear();
		recipients->emplace_back(*aliceDeviceId);
		recipients->emplace_back(*claireDeviceId);
		message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[1].begin(), lime_tester::messages_pattern[1].end());
		bobManager->encrypt(*bobDeviceId, bobAlgos, make_shared<const std::string>("friends"), recipients, message, cipherMessage, callback, policy);
		ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// decrypt it on alice
		receivedMessage.clear();
		ret &= BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "friends", *bobDeviceId, (*recipients)[0].DRmessage, *cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		ret &= BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[1]);
		// decrypt it on claire
		receivedMessage.clear();
		ret &= BC_ASSERT_TRUE(claireManager->decrypt(*claireDeviceId, "friends", *bobDeviceId, (*recipients)[1].DRmessage, *cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		ret &= BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[1]);

		// delete the users
		if (cleanDatabase) {
			for (const auto &algo:aliceAlgos) {
				aliceManager->delete_user(DeviceId(*aliceDeviceId, algo), callback);
				ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			}
			for (const auto &algo:bobAlgos) {
				bobManager->delete_user(DeviceId(*bobDeviceId, algo), callback);
				ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			}
			for (const auto &algo:claireAlgos) {
				claireManager->delete_user(DeviceId(*claireDeviceId, algo), callback);
				ret &= BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			}
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
			remove(dbFilenameClaire.data());
		}
	} catch (BctbxException &e) {
		LIME_LOGE << e;
		BC_FAIL("");
		return false;
	}
	return ret;
}

static void multialgos_three_users(void) {
#if defined(EC25519_ENABLED) && defined(HAVE_BCTBXPQ)
	BC_ASSERT_TRUE(multialgos_three_users_test(
			std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519},
			std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519},
			std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519},
			lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_three_users_test(
			std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519},
			std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519},
			std::vector<lime::CurveId>{lime::CurveId::c25519},
			lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_three_users_test(
			std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519},
			std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519},
			std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519},
			lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_three_users_test(
			std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519},
			std::vector<lime::CurveId>{lime::CurveId::c25519k512, lime::CurveId::c25519},
			std::vector<lime::CurveId>{lime::CurveId::c25519},
			lime::EncryptionPolicy::cipherMessage));
#endif
}

static test_t tests[] = {
	TEST_NO_TAG("Basic", multialgos_basic),
	TEST_NO_TAG("three users", multialgos_three_users)
};

test_suite_t lime_multialgos_test_suite = {
	"Multialgos",
	http_before_all,
	http_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests,
	0,
	0
};
