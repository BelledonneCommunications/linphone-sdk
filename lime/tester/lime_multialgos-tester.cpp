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
static limeX3DHServerPostData X3DHServerPost([](const std::string &url, const std::string &from, std::vector<uint8_t> &&message, const limeX3DHServerResponseProcess &responseProcess){
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

#if defined(HAVE_BCTBXPQ)
namespace {
/* This function will destroy and recreate managers given in parameter, force deleting all internal cache and start back from what is in local Storage */
void managersClean(std::unique_ptr<LimeManager> &alice, std::unique_ptr<LimeManager> &bob, std::string aliceDb, std::string bobDb) {
	alice = nullptr;
	bob = nullptr;
	alice = make_unique<LimeManager>(aliceDb, X3DHServerPost);
	bob = make_unique<LimeManager>(bobDb, X3DHServerPost);
	LIME_LOGI<<"Trash and reload alice and bob LimeManagers";
}
}//namespace


static bool multialgos_basic_test(const std::vector<lime::CurveId> aliceAlgos, const std::vector<lime::CurveId>bobAlgos, lime::EncryptionPolicy policy, bool continuousSession=true) {
	std::string dbBaseFilename("multialgos_basic");
	bool ret = true;
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
		ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for bob
		auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);
		auto bobDeviceId = lime_tester::makeRandomDeviceName("bob.d");
		bobManager->create_user(*bobDeviceId, bobAlgos, lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
		ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		/* destroy and reload the Managers(tests everything is correctly saved/load from local Storage) */
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// alice send a message to bob
		auto enc = make_shared<lime::EncryptionContext>("bob", lime_tester::messages_pattern[0], policy);
		enc->addRecipient(*bobDeviceId);
		aliceManager->encrypt(*aliceDeviceId, aliceAlgos, enc, callback);
		ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// decrypt with bob Manager
		std::vector<uint8_t> receivedMessage{};
		ret &= !!BC_ASSERT_TRUE(lime_tester::DR_message_holdsX3DHInit(enc->m_recipients[0].DRmessage)); // new sessions created, they must convey X3DH init message
		ret &= !!BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		ret &= !!BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[0]);

		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// bob replies to alice
		enc = make_shared<lime::EncryptionContext>("alice", lime_tester::messages_pattern[1], policy);
		enc->addRecipient(*aliceDeviceId);
		bobManager->encrypt(*bobDeviceId, bobAlgos, enc, callback);
		ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

		// decrypt it
		receivedMessage.clear();
		ret &= !!BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "alice", *bobDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		ret &= !!BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[1]);

		// delete the users
		if (cleanDatabase) {
			for (const auto &algo:aliceAlgos) {
				aliceManager->delete_user(DeviceId(*aliceDeviceId, algo), callback);
				ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			}
			for (const auto &algo:bobAlgos) {
				bobManager->delete_user(DeviceId(*bobDeviceId, algo), callback);
				ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
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
#endif // defined(HAVE_BCTBXPQ)

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

	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c448mlk1024}, lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c448mlk1024}, lime::EncryptionPolicy::DRMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c448mlk1024}, lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c448mlk1024, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c448mlk1024}, lime::EncryptionPolicy::cipherMessage,false));
#endif
#if defined(EC448_ENABLED) && defined(HAVE_BCTBXPQ)
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c25519mlk512}, lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::DRMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c25519mlk512}, lime::EncryptionPolicy::DRMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c25519mlk512}, lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519}, lime::EncryptionPolicy::cipherMessage,false));
	BC_ASSERT_TRUE(multialgos_basic_test(std::vector<lime::CurveId>{lime::CurveId::c25519mlk512, lime::CurveId::c25519}, std::vector<lime::CurveId>{lime::CurveId::c25519, lime::CurveId::c25519mlk512}, lime::EncryptionPolicy::cipherMessage,false));
#endif
}


#if defined(HAVE_BCTBXPQ)
namespace {
const std::vector<lime::CurveId> allAlgos{lime::CurveId::c25519, lime::CurveId::c448, lime::CurveId::c25519k512, lime::CurveId::c25519mlk512, lime::CurveId::c448mlk1024};
bool delete_any_user(std::shared_ptr<LimeManager> Manager, std::string &username, const lime::limeCallback &callback, int &counter ) {
	bool ret = true;
	// loop on all possible algo and delete the user if it exists
	for (const auto algo:allAlgos) {
		if (Manager->is_user(DeviceId(username,algo))) {
			Manager->delete_user(DeviceId(username, algo), callback);
			ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counter,counter+1,lime_tester::wait_for_timeout));
		}
	}
	return ret;
}
} //namespace

// scenario
// loop over 'epoch' using the parameters to set supported algorithm for each users.
// Base algo supported differs at each epoch, but db is not deleted in order to simulate users migration
//     create alice, bob, claire and dave users
//     Alice encrypts to bob, claire and dave
//     bob replies to alice, claire and dave
static bool multialgos_four_users_test(const std::vector<std::vector<lime::CurveId>> aliceAlgos,
	       const std::vector<std::vector<lime::CurveId>> bobAlgos,
	       const std::vector<std::vector<lime::CurveId>> claireAlgos,
	       const std::vector<std::vector<lime::CurveId>> daveAlgos,
	       lime::EncryptionPolicy policy) {
	std::string dbBaseFilename("multialgos_three_users");
	bool ret = true;
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
		// create DB
		auto dbFilenameAlice = dbBaseFilename;
		dbFilenameAlice.append(".alice.").append(CurveId2String(aliceAlgos[0], "-")).append(".sqlite3");
		auto dbFilenameBob = dbBaseFilename;
		dbFilenameBob.append(".bob.").append(CurveId2String(bobAlgos[0], "-")).append(".sqlite3");
		auto dbFilenameClaire = dbBaseFilename;
		dbFilenameClaire.append(".claire.").append(CurveId2String(claireAlgos[0], "-")).append(".sqlite3");
		auto dbFilenameDave = dbBaseFilename;
		dbFilenameDave.append(".dave.").append(CurveId2String(daveAlgos[0], "-")).append(".sqlite3");

		remove(dbFilenameAlice.data()); // delete the database file if already exists
		remove(dbFilenameBob.data()); // delete the database file if already exists
		remove(dbFilenameClaire.data()); // delete the database file if already exists
		remove(dbFilenameDave.data()); // delete the database file if already exists

		// create device Ids
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.d.");
		auto bobDeviceId = lime_tester::makeRandomDeviceName("bob.d");
		auto claireDeviceId = lime_tester::makeRandomDeviceName("claire.d");
		auto daveDeviceId = lime_tester::makeRandomDeviceName("dave.d");

		// loop on all three algo support setting (they must all have the same size)
		for (size_t i=0; i<aliceAlgos.size(); i++) {
			LIME_LOGI<<"Test epoch :"<<i;
			// create Manager and device for alice
			auto aliceManager = make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
			if (!aliceManager->is_user(*aliceDeviceId, aliceAlgos[i])) {
				aliceManager->create_user(*aliceDeviceId, aliceAlgos[i], lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
				ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
			}

			// Create manager and device for bob
			auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);
			if (!bobManager->is_user(*bobDeviceId, bobAlgos[i])) {
				bobManager->create_user(*bobDeviceId, bobAlgos[i], lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
				ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
			}

			// Create manager and device for claire
			auto claireManager = make_unique<LimeManager>(dbFilenameClaire, X3DHServerPost);
			if (!claireManager->is_user(*claireDeviceId, claireAlgos[i])) {
				claireManager->create_user(*claireDeviceId, claireAlgos[i], lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
				ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
			}
			// Create manager and device for dave
			auto daveManager = make_unique<LimeManager>(dbFilenameClaire, X3DHServerPost);
			if (!daveManager->is_user(*daveDeviceId, daveAlgos[i])) {
				daveManager->create_user(*daveDeviceId, daveAlgos[i], lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
				ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));
			}

			// alice send a message to bob, claire and dave
			auto enc = make_shared<lime::EncryptionContext>("friends", lime_tester::messages_pattern[0], policy);
			enc->addRecipient(*bobDeviceId);
			enc->addRecipient(*claireDeviceId);
			enc->addRecipient(*daveDeviceId);
			aliceManager->encrypt(*aliceDeviceId, aliceAlgos[i], enc, callback);
			ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			if (policy == lime::EncryptionPolicy::cipherMessage) {
				ret &= !!BC_ASSERT_TRUE(!enc->m_cipherMessage.empty());
			}

			// decrypt with bob Manager
			std::vector<uint8_t> receivedMessage{};
			ret &= !!BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "friends", *aliceDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			ret &= !!BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[0]);

			// decrypt with claire Manager
			receivedMessage.clear();
			ret &= !!BC_ASSERT_TRUE(claireManager->decrypt(*claireDeviceId, "friends", *aliceDeviceId, enc->m_recipients[1].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			ret &= !!BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[0]);

			// decrypt with dave Manager
			receivedMessage.clear();
			ret &= !!BC_ASSERT_TRUE(daveManager->decrypt(*daveDeviceId, "friends", *aliceDeviceId, enc->m_recipients[2].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			ret &= !!BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[0]);

			// bob replies to alice, claire and dave
			enc = make_shared<lime::EncryptionContext>("friends", lime_tester::messages_pattern[1], policy);
			enc->addRecipient(*aliceDeviceId);
			enc->addRecipient(*claireDeviceId);
			enc->addRecipient(*daveDeviceId);
			bobManager->encrypt(*bobDeviceId, bobAlgos[i], enc, callback);
			ret &= !!BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			if (policy == lime::EncryptionPolicy::cipherMessage) {
				ret &= !!BC_ASSERT_TRUE(!enc->m_cipherMessage.empty());
			}

			// decrypt it on alice
			receivedMessage.clear();
			ret &= !!BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "friends", *bobDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			ret &= !!BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[1]);

			// decrypt it on claire
			receivedMessage.clear();
			ret &= !!BC_ASSERT_TRUE(claireManager->decrypt(*claireDeviceId, "friends", *bobDeviceId, enc->m_recipients[1].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			ret &= !!BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[1]);

			// decrypt it on dave
			receivedMessage.clear();
			ret &= !!BC_ASSERT_TRUE(daveManager->decrypt(*daveDeviceId, "friends", *bobDeviceId, enc->m_recipients[2].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
			ret &= !!BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[1]);

			LIME_LOGI<<"Test epoch :"<<i<<" done";
		}

		// delete the users
		if (cleanDatabase) {
			auto aliceManager = make_shared<LimeManager>(dbFilenameAlice, X3DHServerPost);
			ret &= !!BC_ASSERT_TRUE(delete_any_user(aliceManager, *aliceDeviceId, callback, counters.operation_success));
			auto bobManager = make_shared<LimeManager>(dbFilenameBob, X3DHServerPost);
			ret &= !!BC_ASSERT_TRUE(delete_any_user(bobManager, *bobDeviceId, callback, counters.operation_success));
			auto claireManager = make_shared<LimeManager>(dbFilenameClaire, X3DHServerPost);
			ret &= !!BC_ASSERT_TRUE(delete_any_user(claireManager, *claireDeviceId, callback, counters.operation_success));
			auto daveManager = make_shared<LimeManager>(dbFilenameDave, X3DHServerPost);
			ret &= !!BC_ASSERT_TRUE(delete_any_user(daveManager, *daveDeviceId, callback, counters.operation_success));
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
			remove(dbFilenameClaire.data());
			remove(dbFilenameDave.data());
		}
	} catch (BctbxException &e) {
		LIME_LOGE << e;
		BC_FAIL("");
		return false;
	}
	return ret;
}
#endif // defined(HAVE_BCTBXPQ)

static void multialgos_four_users_basic(void) {
#if defined(EC25519_ENABLED) && defined(HAVE_BCTBXPQ)
	// Simple tests with all the same algorithm or only claire with the second one so Alice encrypts in two passes
	BC_ASSERT_TRUE(multialgos_four_users_test(
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			lime::EncryptionPolicy::DRMessage));
	BC_ASSERT_TRUE(multialgos_four_users_test(
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519}},
			lime::EncryptionPolicy::DRMessage));
	// same but force the use of cipherMessage to test the randomSeed management
	BC_ASSERT_TRUE(multialgos_four_users_test(
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_four_users_test(
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519}},
			lime::EncryptionPolicy::cipherMessage));
	BC_ASSERT_TRUE(multialgos_four_users_test(
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c448, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519k512, lime::CurveId::c25519, lime::CurveId::c448}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c448}},
			lime::EncryptionPolicy::cipherMessage));
#endif
}
static void multialgos_four_users_migration(void) {
#if defined(EC25519_ENABLED) && defined(HAVE_BCTBXPQ)
	// Dynamic algo support
	BC_ASSERT_TRUE(multialgos_four_users_test(
			// alice  uses: c25519 | c25519k512,c25519 | c25519k512,c25519 | c25519k512,c25519
			// bob    uses: c25519 |    c25519         | c25519k512,c25519 | c25519k512,c25519
			// claire uses: c25519 |    c25519         |      c25519       | c25519k512,c25519
			// dave   uses: c25519 |    c25519         |      c25519       |      c25519
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519}, {lime::CurveId::c25519k512, lime::CurveId::c25519}, {lime::CurveId::c25519k512, lime::CurveId::c25519}, {lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519}, {lime::CurveId::c25519}, {lime::CurveId::c25519k512, lime::CurveId::c25519}, {lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519}, {lime::CurveId::c25519}, {lime::CurveId::c25519}, {lime::CurveId::c25519k512, lime::CurveId::c25519}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c25519}, {lime::CurveId::c25519}, {lime::CurveId::c25519}, {lime::CurveId::c25519}},
			lime::EncryptionPolicy::cipherMessage));
#endif
#if defined(EC448_ENABLED) && defined(HAVE_BCTBXPQ)
	// Dynamic algo support
	BC_ASSERT_TRUE(multialgos_four_users_test(
			// alice  uses: c448 | c448mlk1024,c448 | c448mlk1024,c448 | c448mlk1024,c448
			// bob    uses: c448 |     c448         | c448mlk1024,c448 | c448mlk1024,c448
			// claire uses: c448 |     c448         |       c448       | c448mlk1024,c448
			// dave   uses: c448 |     c448         |       c448       |      c448
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c448}, {lime::CurveId::c448mlk1024, lime::CurveId::c448}, {lime::CurveId::c448mlk1024, lime::CurveId::c448}, {lime::CurveId::c448mlk1024, lime::CurveId::c448}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c448}, {lime::CurveId::c448}, {lime::CurveId::c448mlk1024, lime::CurveId::c448}, {lime::CurveId::c448mlk1024, lime::CurveId::c448}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c448}, {lime::CurveId::c448}, {lime::CurveId::c448}, {lime::CurveId::c448mlk1024, lime::CurveId::c448}},
			std::vector<std::vector<lime::CurveId>>{{lime::CurveId::c448}, {lime::CurveId::c448}, {lime::CurveId::c448}, {lime::CurveId::c448}},
			lime::EncryptionPolicy::cipherMessage));
#endif

}

/*
 * Scenario:
 * - create Bob and Alice using on curve 25519
 * - check both alice and bob see each other as unknown
 * - alice encrypts to Bob
 * - check both alice and bob see each other as untrusted
 * - set bob as trusted in alice db
 * - check bob is trusted
 * - now Alice migrates to c25519k512/c25519  
 * - alice encrypts to Bob
 * - check bob is still trusted
 * - now Bob migrates to c25519k512/c25519  
 * - alice encrypts to Bob (she will thus start a new session with a new bob peer device)
 * - check bob is untrusted now (as we never trusted his new key)
 * - alice change her mind and encrypt to bob using c25519 only session -> this force the active peer device for bob to be the c255519 one
 * - check bob is now trusted
 * - Bob encrypts to alice using the c25519k512 device -> alice decrypts, it forces back the active peerdevice to be the c25519k512 one
 * - check bob is back to untrusted
 */
static void multialgos_peerStatus() {
#if defined(EC25519_ENABLED) && defined(HAVE_BCTBXPQ)
	std::string dbBaseFilename("multialgos_peerStatus");
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
	const lime::EncryptionPolicy policy(lime::EncryptionPolicy::cipherMessage);

	try {
		// create DB
		auto dbFilenameAlice = dbBaseFilename;
		dbFilenameAlice.append(".alice").append(".sqlite3");
		auto dbFilenameBob = dbBaseFilename;
		dbFilenameBob.append(".bob").append(".sqlite3");

		remove(dbFilenameAlice.data()); // delete the database file if already exists
		remove(dbFilenameBob.data()); // delete the database file if already exists

		std::vector<lime::CurveId> c25519only{lime::CurveId::c25519};
		std::vector<lime::CurveId> c25519k512andc25519{lime::CurveId::c25519k512, lime::CurveId::c25519};

		// create Manager and device for alice, use c25519 only
		auto aliceManager = make_unique<LimeManager>(dbFilenameAlice, X3DHServerPost);
		auto aliceDeviceId = lime_tester::makeRandomDeviceName("alice.d.");
		aliceManager->create_user(*aliceDeviceId, c25519only, lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Create manager and device for bob, use c25519 only
		auto bobManager = make_unique<LimeManager>(dbFilenameBob, X3DHServerPost);
		auto bobDeviceId = lime_tester::makeRandomDeviceName("bob.d");
		bobManager->create_user(*bobDeviceId, c25519only, lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Check peerStatus: both are unknown
		BC_ASSERT_TRUE(aliceManager->get_peerDeviceStatus(*bobDeviceId) == lime::PeerDeviceStatus::unknown);
		BC_ASSERT_TRUE(bobManager->get_peerDeviceStatus(*aliceDeviceId) == lime::PeerDeviceStatus::unknown);

		// alice sends a message to bob
		auto enc = make_shared<lime::EncryptionContext>("bob", lime_tester::messages_pattern[0], policy);
		enc->addRecipient(*bobDeviceId);
		aliceManager->encrypt(*aliceDeviceId, c25519only, enc, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		// decrypt with bob Manager
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[0]);

		// Check peerStatus: both are untrusted
		BC_ASSERT_TRUE(aliceManager->get_peerDeviceStatus(*bobDeviceId) == lime::PeerDeviceStatus::untrusted);
		BC_ASSERT_TRUE(bobManager->get_peerDeviceStatus(*aliceDeviceId) == lime::PeerDeviceStatus::untrusted);

		// Get Bob Ik and set is a trusted for Alice
		std::map<lime::CurveId, std::vector<uint8_t>> Iks{};
		bobManager->get_selfIdentityKey(*bobDeviceId, c25519only, Iks);
		aliceManager->set_peerDeviceStatus(*bobDeviceId, lime::CurveId::c25519, Iks[lime::CurveId::c25519], lime::PeerDeviceStatus::trusted);

		// Check peerStatus
		BC_ASSERT_TRUE(aliceManager->get_peerDeviceStatus(*bobDeviceId) == lime::PeerDeviceStatus::trusted);

		// now alice migrates to c25519k512, but still supports c25519
		aliceManager->create_user(*aliceDeviceId, c25519k512andc25519, lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// Check peerStatus, it is still trusted as there is no new peer Device for Bob
		BC_ASSERT_TRUE(aliceManager->get_peerDeviceStatus(*bobDeviceId) == lime::PeerDeviceStatus::trusted);

		// alice sends a message to bob, she still uses the c25519only device for Bob as no c25519k512 can be found
		enc = make_shared<lime::EncryptionContext>("bob", lime_tester::messages_pattern[1], policy);
		enc->addRecipient(*bobDeviceId);
		aliceManager->encrypt(*aliceDeviceId, c25519k512andc25519, enc, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		// decrypt with bob Manager
		receivedMessage.clear();
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[1]);

		// Check peerStatus, it is still trusted as there is no new peer Device for Bob
		BC_ASSERT_TRUE(aliceManager->get_peerDeviceStatus(*bobDeviceId) == lime::PeerDeviceStatus::trusted);

		// now bob migrates to c25519k512, but still supports c25519
		bobManager->create_user(*bobDeviceId, c25519k512andc25519, lime_tester::test_x3dh_default_server, lime_tester::OPkInitialBatchSize, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, ++expected_success,lime_tester::wait_for_timeout));

		// alice sends a message to bob, she'll uses a new peerDevice for Bob
		enc = make_shared<lime::EncryptionContext>("bob", lime_tester::messages_pattern[2], policy);
		enc->addRecipient(*bobDeviceId);
		aliceManager->encrypt(*aliceDeviceId, c25519k512andc25519, enc, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		// decrypt with bob Manager
		receivedMessage.clear();
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[2]);

		// Check peerStatus, it is now untrusted as there is a new peer Device for Bob
		BC_ASSERT_TRUE(aliceManager->get_peerDeviceStatus(*bobDeviceId) == lime::PeerDeviceStatus::untrusted);

		// alice changes her mind and encrypt to bob using specifically the c25519 only session
		enc = make_shared<lime::EncryptionContext>("bob", lime_tester::messages_pattern[3], policy);
		enc->addRecipient(*bobDeviceId);
		aliceManager->encrypt(*aliceDeviceId, c25519only, enc, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		// decrypt with bob Manager
		receivedMessage.clear();
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDeviceId, "bob", *aliceDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[3]);

		// Check peerStatus, the last encryption set the bob/c25519 peerDevice back to be the active one
		BC_ASSERT_TRUE(aliceManager->get_peerDeviceStatus(*bobDeviceId) == lime::PeerDeviceStatus::trusted);

		// bob encrypt to alice using specifically the c25519k512 session
		enc = make_shared<lime::EncryptionContext>("alice", lime_tester::messages_pattern[4], policy);
		enc->addRecipient(*aliceDeviceId);
		bobManager->encrypt(*bobDeviceId, c25519k512andc25519, enc, callback);
		BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
		// decrypt with alice Manager
		receivedMessage.clear();
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDeviceId, "alice", *bobDeviceId, enc->m_recipients[0].DRmessage, enc->m_cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
		BC_ASSERT_TRUE(receivedMessage == lime_tester::messages_pattern[4]);

		// Check peerStatus, the last decryption set the bob/c25519k512 peerDevice back to be the active one -> untrusted
		BC_ASSERT_TRUE(aliceManager->get_peerDeviceStatus(*bobDeviceId) == lime::PeerDeviceStatus::untrusted);

		// delete the users
		if (cleanDatabase) {
			for (const auto &algo:c25519k512andc25519) {
				aliceManager->delete_user(DeviceId(*aliceDeviceId, algo), callback);
				BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			}
			for (const auto &algo:c25519k512andc25519) {
				bobManager->delete_user(DeviceId(*bobDeviceId, algo), callback);
				BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));
			}
			remove(dbFilenameAlice.data());
			remove(dbFilenameBob.data());
		}
	} catch (BctbxException &e) {
		LIME_LOGE << e;
		BC_FAIL("");
	}
#endif // defined(EC25519_ENABLED) && defined(HAVE_BCTBXPQ)
}

static test_t tests[] = {
	TEST_NO_TAG("Basic", multialgos_basic),
	TEST_NO_TAG("four users", multialgos_four_users_basic),
	TEST_NO_TAG("four users migration", multialgos_four_users_migration),
	TEST_NO_TAG("Peer status", multialgos_peerStatus)
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
