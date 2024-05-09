/*
	lime_multidomains-tester.cpp
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

// Scenario:
// Alice has a device Id sip:alice@domainA;gr=<...>;
// Bob has a device Id sip:bob@domainB;gr=<...>;
//
// Create users on their respective servers
// Alice encrypts a message to bob
static void lime_multidomains_simple() {
// To avoid the configuration of too many servers, the multidomain is tested on Curve25519 only
#ifdef EC25519_ENABLED
	auto curve = lime::CurveId::c25519;
	// create DBs
	std::string dbBaseFilename("multidomain");
	std::string dbFilenameAlice = dbBaseFilename;
	dbFilenameAlice.append(".alice.").append(".sqlite3");
	std::string dbFilenameBob = dbBaseFilename;
	dbFilenameBob.append(".bob.").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::CallbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::CallbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						LIME_LOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});
	try {
		// create Manager and device for alice
		auto aliceManager = make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("sip:alice@domainA;gr=");
		aliceManager->create_user(*aliceDeviceId, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainA_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for bob
		auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);
		auto bobDeviceId = lime_tester::makeRandomDeviceName("sip:bob@domainB;gr=");
		bobManager->create_user(*bobDeviceId, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainB_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Alice encrypts a message to Bob  - keys are fetch from alice server forwarding the bundle request to friends domainB and domainC
		auto aliceRecipients = make_shared<std::vector<RecipientData>>();
		aliceRecipients->emplace_back(*bobDeviceId);
		auto aliceMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto aliceCipherMessage = make_shared<std::vector<uint8_t>>();

		aliceManager->encrypt(*aliceDeviceId, make_shared<const std::string>("foreign friends"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// bob decrypts
		std::vector<uint8_t> receivedMessage{};
		// in that context we cannot know the expected decrypt return value, just check it is not fail
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "foreign friends", *aliceDeviceId, (*aliceRecipients)[0].DRmessage, *aliceCipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		auto receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);
	} catch (BctbxException &) {
		BC_FAIL("Multidomain test got an exception");
		throw;
	}

#else
	LIME_LOGE<<"Warning: Multidomain is not tested when curve25519 is not supported";
#endif
}


// Scenario:
// Alice has a device Id sip:alice@domainA;gr=<...>;
// Bob has a device Id sip:bob@domainB;gr=<...>;
// Claire has a device Id sip:claire@domainC;gr=<...>;
// Dave has a device Id sip:dave@domainA;gr=<...>;
//
// Create users on their respective servers
// Alice send message to all of them
static void lime_multidomains_several_foreign() {
// To avoid the configuration of too many servers, the multidomain is tested on Curve25519 only
#ifdef EC25519_ENABLED
	auto curve = lime::CurveId::c25519;
	// create DBs
	std::string dbBaseFilename("multidomain");
	std::string dbFilenameAlice = dbBaseFilename;
	dbFilenameAlice.append(".alice.").append(".sqlite3");
	std::string dbFilenameBob = dbBaseFilename;
	dbFilenameBob.append(".bob.").append(".sqlite3");
	std::string dbFilenameClaire = dbBaseFilename;
	dbFilenameClaire.append(".claire.").append(".sqlite3");
	std::string dbFilenameDave = dbBaseFilename;
	dbFilenameDave.append(".dave.").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists
	remove(dbFilenameClaire.data()); // delete the database file if already exists
	remove(dbFilenameDave.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::CallbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::CallbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						LIME_LOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});
	try {
		// create Manager and device for alice
		auto aliceManager = make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("sip:alice@domainA;gr=");
		aliceManager->create_user(*aliceDeviceId, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainA_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for bob
		auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);
		auto bobDeviceId = lime_tester::makeRandomDeviceName("sip:bob@domainB;gr=");
		bobManager->create_user(*bobDeviceId, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainB_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for claire
		auto claireManager = make_unique<LimeManager>(dbFilenameClaire, X3DHServerPost);
		auto claireDeviceId = lime_tester::makeRandomDeviceName("sip:claire@domainC;gr=");
		claireManager->create_user(*claireDeviceId, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainC_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for dave
		auto daveManager = make_unique<LimeManager>(dbFilenameDave, X3DHServerPost);
		auto daveDeviceId = lime_tester::makeRandomDeviceName("sip:dave@domainA;gr=");
		daveManager->create_user(*daveDeviceId, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainA_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Alice encrypts a message to Dave, Bob and Claire - keys are fetch from alice server forwarding the bundle request to friends domainB and domainC
		auto aliceRecipients = make_shared<std::vector<RecipientData>>();
		aliceRecipients->emplace_back(*bobDeviceId);
		aliceRecipients->emplace_back(*claireDeviceId);
		aliceRecipients->emplace_back(*daveDeviceId);
		auto aliceMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto aliceCipherMessage = make_shared<std::vector<uint8_t>>();

		aliceManager->encrypt(*aliceDeviceId, make_shared<const std::string>("foreign friends"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// bob decrypts
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "foreign friends", *aliceDeviceId, (*aliceRecipients)[0].DRmessage, *aliceCipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		auto receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// claire decrypts
		receivedMessage.clear();
		BC_ASSERT_TRUE(claireManager->decrypt(*claireDeviceId, "foreign friends", *aliceDeviceId, (*aliceRecipients)[1].DRmessage, *aliceCipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// dave decrypts
		receivedMessage.clear();
		BC_ASSERT_TRUE(daveManager->decrypt(*daveDeviceId, "foreign friends", *aliceDeviceId, (*aliceRecipients)[2].DRmessage, *aliceCipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

	} catch (BctbxException &) {
		BC_FAIL("Multidomain test got an exception");
		throw;
	}

#else
	LIME_LOGE<<"Warning: Multidomain is not tested when curve25519 is not supported";
#endif
}

// Scenario:
// Alice has 2 devices Id sip:alice@domainA;gr=<...>;
// Bob has 2 devices Id sip:bob@domainB;gr=<...>;
// Claire has 2 devices Id sip:claire@domainC;gr=<...>;
// Dave has 2 devices Id sip:dave@domainA;gr=<...>;
//
// Create users on their respective servers
// Alice send message to all of them
static void lime_multidomains_several_users_foreign() {
// To avoid the configuration of too many servers, the multidomain is tested on Curve25519 only
#ifdef EC25519_ENABLED
	auto curve = lime::CurveId::c25519;
	// create DBs
	std::string dbBaseFilename("multidomain");
	std::string dbFilenameAlice = dbBaseFilename;
	dbFilenameAlice.append(".alice.").append(".sqlite3");
	std::string dbFilenameBob = dbBaseFilename;
	dbFilenameBob.append(".bob.").append(".sqlite3");
	std::string dbFilenameClaire = dbBaseFilename;
	dbFilenameClaire.append(".claire.").append(".sqlite3");
	std::string dbFilenameDave = dbBaseFilename;
	dbFilenameDave.append(".dave.").append(".sqlite3");

	remove(dbFilenameAlice.data()); // delete the database file if already exists
	remove(dbFilenameBob.data()); // delete the database file if already exists
	remove(dbFilenameClaire.data()); // delete the database file if already exists
	remove(dbFilenameDave.data()); // delete the database file if already exists

	lime_tester::events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::CallbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::CallbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						LIME_LOGE<<"Lime operation failed : "<<anythingToSay;
					}
				});
	try {
		// create Manager and device for alice
		auto aliceManager = make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("sip:alice@domainA;gr=");
		aliceManager->create_user(*aliceDeviceId, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainA_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		auto aliceDeviceId2 = lime_tester::makeRandomDeviceName("sip:alice@domainA;gr=");
		aliceManager->create_user(*aliceDeviceId2, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainA_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for bob
		auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);
		auto bobDeviceId = lime_tester::makeRandomDeviceName("sip:bob@domainB;gr=");
		bobManager->create_user(*bobDeviceId, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainB_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
		auto bobDeviceId2 = lime_tester::makeRandomDeviceName("sip:bob@domainB;gr=");
		bobManager->create_user(*bobDeviceId2, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainB_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for claire
		auto claireManager = make_unique<LimeManager>(dbFilenameClaire, X3DHServerPost);
		auto claireDeviceId = lime_tester::makeRandomDeviceName("sip:claire@domainC;gr=");
		claireManager->create_user(*claireDeviceId, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainC_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
		auto claireDeviceId2 = lime_tester::makeRandomDeviceName("sip:claire@domainC;gr=");
		claireManager->create_user(*claireDeviceId2, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainC_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for dave
		auto daveManager = make_unique<LimeManager>(dbFilenameDave, X3DHServerPost);
		auto daveDeviceId = lime_tester::makeRandomDeviceName("sip:dave@domainA;gr=");
		daveManager->create_user(*daveDeviceId, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainA_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
		auto daveDeviceId2 = lime_tester::makeRandomDeviceName("sip:dave@domainA;gr=");
		daveManager->create_user(*daveDeviceId2, std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_domainA_server_port).data(), curve, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Alice encrypts a message to Dave, Bob and Claire - keys are fetch from alice server forwarding the bundle request to friends domainB and domainC
		auto aliceRecipients = make_shared<std::vector<RecipientData>>();
		aliceRecipients->emplace_back(*bobDeviceId);
		aliceRecipients->emplace_back(*claireDeviceId);
		aliceRecipients->emplace_back(*daveDeviceId);
		aliceRecipients->emplace_back(*aliceDeviceId2);
		aliceRecipients->emplace_back(*bobDeviceId2);
		aliceRecipients->emplace_back(*claireDeviceId2);
		aliceRecipients->emplace_back(*daveDeviceId2);
		auto aliceMessage = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end());
		auto aliceCipherMessage = make_shared<std::vector<uint8_t>>();

		aliceManager->encrypt(*aliceDeviceId, make_shared<const std::string>("foreign friends"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// bob decrypts
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "foreign friends", *aliceDeviceId, (*aliceRecipients)[0].DRmessage, *aliceCipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		auto receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// claire decrypts
		receivedMessage.clear();
		BC_ASSERT_TRUE(claireManager->decrypt(*claireDeviceId, "foreign friends", *aliceDeviceId, (*aliceRecipients)[1].DRmessage, *aliceCipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// dave decrypts
		receivedMessage.clear();
		BC_ASSERT_TRUE(daveManager->decrypt(*daveDeviceId, "foreign friends", *aliceDeviceId, (*aliceRecipients)[2].DRmessage, *aliceCipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// alice2 decrypts
		receivedMessage.clear();
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId2, "foreign friends", *aliceDeviceId, (*aliceRecipients)[3].DRmessage, *aliceCipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// bob2 decrypts
		receivedMessage.clear();
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId2, "foreign friends", *aliceDeviceId, (*aliceRecipients)[4].DRmessage, *aliceCipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// claire2 decrypts
		receivedMessage.clear();
		BC_ASSERT_TRUE(claireManager->decrypt(*claireDeviceId2, "foreign friends", *aliceDeviceId, (*aliceRecipients)[5].DRmessage, *aliceCipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);

		// dave2 decrypts
		receivedMessage.clear();
		BC_ASSERT_TRUE(daveManager->decrypt(*daveDeviceId2, "foreign friends", *aliceDeviceId, (*aliceRecipients)[6].DRmessage, *aliceCipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[0]);
	} catch (BctbxException &) {
		BC_FAIL("Multidomain test got an exception");
		throw;
	}

#else
	LIME_LOGE<<"Warning: Multidomain is not tested when curve25519 is not supported";
#endif
}

static test_t tests[] = {
	TEST_NO_TAG("One foreign user", lime_multidomains_simple),
	TEST_NO_TAG("Several Foreign Domains", lime_multidomains_several_foreign),
	TEST_NO_TAG("Several Users on several foreign Domains", lime_multidomains_several_users_foreign)
};

test_suite_t lime_multidomains_test_suite = {
	"Multidomains",
	http_before_all,
	http_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests,
	0,
	0
};
