/*
	lime_massive_group-tester.cpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2018  Belledonne Communications SARL

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

#include "lime/lime.hpp"
#include "lime_log.hpp"
#include "lime-tester.hpp"
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

static belle_sip_stack_t *bc_stack=NULL;
static belle_http_provider_t *prov=NULL;

// maximum runtime target for a bench run, the group size will be adjusted to match it
// if running on curve 448 and 25519, 2 runs of benchTime are performed
uint64_t maximumBenchTime = 120000; // in ms

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
 * Scenario:
 * - Set up a group of deviceNumber devices
 * In oneTalking mode:
 * - first device post a message to all the others -> each of them decrypt (they will all have to create sessions)
 * - do it again : first device post a message to all the others -> each of them decrypt (they use already existing sessions)
 * In allTalking mode:
 * - every device takes turn to send messages to all the others
 */
static void group_basic_test(const lime::CurveId curve, const std::string &dbBaseFilename, const std::string &x3dh_server_url, const int deviceNumber, bool oneTalking=false, bool oneDecrypt=false) {

	auto groupName = make_shared<std::string>("group Name");

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


	// create DBs and managers
	std::unique_ptr<LimeManager> manager; // only one manager at a tine, creating a new one will close the existing

	// vectors: they will share matching indexes
	std::vector<std::shared_ptr<std::string>> devicesId; // a vector of devicesId
	std::vector<std::string> dbFilename; // a vector of dbfilenames

	auto base_deviceId = *(lime_tester::makeRandomDeviceName("alice.")); // the base user name, each manager gets one user
	base_deviceId.append(".d");

	try {
		uint64_t start=0,span,startEncrypt=0;
		if (bench) { // use LOGE for bench report to avoid being flooded by debug logs
			LIME_LOGE<<"Running a group of "<<to_string(deviceNumber)<<" on curve"<<((curve==lime::CurveId::c25519)?"25519": ((curve==lime::CurveId::c448)?"448":"25519/Kyber 512"))<<endl;
			start = bctbx_get_cur_time_ms();
		}
		// loop on all devices and create basics
		for (auto i=0; i<deviceNumber; i++) {
			// db filename is base.d<index>
			dbFilename.push_back(dbBaseFilename);
			dbFilename.back().append(".d").append(to_string(i)).append(".sqlite3");
			remove(dbFilename.back().data()); // delete the database file if already exists

			// create manager
			manager = make_unique<LimeManager>(dbFilename.back(), X3DHServerPost);

			// create device Id : base_deviceId<index>
			auto deviceId = base_deviceId;
			deviceId.append(to_string(i));
			devicesId.push_back(make_shared<std::string>(deviceId));

			// create user
			manager->create_user(deviceId, x3dh_server_url, curve, std::min(lime_tester::OPkInitialBatchSize+i, 200), callback); // give them at least <index> OPk at creation, no more than 200 as the server as a limit on it
			expected_success++;
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success, expected_success,lime_tester::wait_for_timeout));
		}

		if (bench) {
			span = bctbx_get_cur_time_ms() - start;
			LIME_LOGE<<"Generate and register users in "<<to_string(span)<<" ms ("<<to_string(span/deviceNumber)<<" ms/user)"<<std::endl;
			start = bctbx_get_cur_time_ms();
		}

		// loop on all devices and take turns into sending to all the others
		auto sendersNumber = oneTalking?2:deviceNumber;
		for (auto i=0; i<sendersNumber; i++) {
			// when only one device is talking: the device 0 send twice a message
			auto senderIndex = oneTalking?0:i;

			// create the list of recipients
			auto recipients = make_shared<std::vector<RecipientData>>();
			for (auto j=0; j<deviceNumber; j++) {
				if (j!=senderIndex) { // don't write to self
					recipients->emplace_back(*(devicesId[j]));
				}
			}

			// select a message to encrypt
			auto messages_pattern_index = i%lime_tester::messages_pattern.size();
			auto message = make_shared<const std::vector<uint8_t>>(lime_tester::messages_pattern[messages_pattern_index].begin(), lime_tester::messages_pattern[messages_pattern_index].end());

			if (bench) {
				startEncrypt = bctbx_get_cur_time_ms();
			}

			// get a manager for this device
			manager = make_unique<LimeManager>(dbFilename[senderIndex], X3DHServerPost);
			//encrypt
			auto cipherMessage = make_shared<std::vector<uint8_t>>();
			manager->encrypt(*(devicesId[senderIndex]), groupName, recipients, message, cipherMessage, callback);
			BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,++expected_success,lime_tester::wait_for_timeout));

			if (bench) {
				if (i==0) { // first run shall be the longest as we have deviceNumber-1 sessions to establish
					span = bctbx_get_cur_time_ms() - startEncrypt;
					LIME_LOGE<<"first message encrypt(all session to establish) in "<<to_string(span)<<" ms ("<<to_string(float(span)/float(deviceNumber))<<" ms/recipient)"<<std::endl;
				}
				if (i==1 && senderIndex==0) { // second message in oneTalking mode
					span = bctbx_get_cur_time_ms() - startEncrypt;
					LIME_LOGE<<"second message encrypt(no session to establish) in "<<to_string(span)<<" ms ("<<to_string(float(span)/float(deviceNumber))<<" ms/recipient)"<<std::endl;
				}
				if (i==deviceNumber-1) { // last run shall be the fastest as we have no session to establish
					span = bctbx_get_cur_time_ms() - startEncrypt;
					LIME_LOGE<<"last message encrypt(no session to establish) in "<<to_string(span)<<" ms ("<<to_string(float(span)/float(deviceNumber))<<" ms/recipient)"<<std::endl;
				}
				startEncrypt = bctbx_get_cur_time_ms();
			}

			// now loop on all device and decrypt the message
			auto recipientDecryptIndex = 0; // recipient has not exactly the same index than all device as skip sender device
			for (auto j=0; j<deviceNumber; j++) {
				if (j==senderIndex) { // don't got a message for us
					continue;
				}
				// get a manager for this device
				manager = make_unique<LimeManager>(dbFilename[j], X3DHServerPost);
				std::vector<uint8_t> receivedMessage{};
				BC_ASSERT_TRUE(manager->decrypt(*(devicesId[j]), *groupName, *(devicesId[senderIndex]), (*recipients)[recipientDecryptIndex].DRmessage, *cipherMessage, receivedMessage) != lime::PeerDeviceStatus::fail);
				auto receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
				BC_ASSERT_TRUE(receivedMessageString == lime_tester::messages_pattern[messages_pattern_index]);
				recipientDecryptIndex++;
				if (oneDecrypt) {
					break; // just decrypt one message
				}
			}

			if (bench) {
				if (i==0) { // first run shall be the longest as we have deviceNumber-1 sessions to establish
					span = bctbx_get_cur_time_ms() - startEncrypt;
					LIME_LOGE<<"first message decrypts in "<<to_string(span)<<" ms ("<<to_string(float(span)/float(oneDecrypt?1:deviceNumber-1))<<" ms/recipient)"<<std::endl;
				}
				if (i==1 && senderIndex==0) { // second message in oneTalking mode
					span = bctbx_get_cur_time_ms() - startEncrypt;
					LIME_LOGE<<"second message decrypts in "<<to_string(span)<<" ms ("<<to_string(float(span)/float(oneDecrypt?1:deviceNumber-1))<<" ms/recipient)"<<std::endl;
				}
				if (i==deviceNumber-1) { // last run shall be the shortest as we have no session to establish
					span = bctbx_get_cur_time_ms() - startEncrypt;
					LIME_LOGE<<"last message decrypts round in "<<to_string(span)<<" ms ("<<to_string(float(span)/float(oneDecrypt?1:deviceNumber-1))<<" ms/recipient)"<<std::endl;
				}
			}
		}

		if (cleanDatabase) {
			// loop on all devices and create basics
			for (auto i=0; i<deviceNumber; i++) {
				// get a manager for this device
				manager = make_unique<LimeManager>(dbFilename[i], X3DHServerPost);
				manager->delete_user(*(devicesId[i]), callback);
				expected_success++;
				BC_ASSERT_TRUE(lime_tester::wait_for(bc_stack,&counters.operation_success,expected_success,lime_tester::wait_for_timeout));
			}

			for (auto i=0; i<deviceNumber; i++) {
				remove(dbFilename[i].data()); // delete the database file if already exists
			}
		}

	} catch (BctbxException &e) {
		LIME_LOGE << e;
		BC_FAIL("");
	}
}



static void group_one_talking() {
#ifdef EC25519_ENABLED
	group_basic_test(lime::CurveId::c25519, "group_one_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519_server_port).data(), 10, true);
#endif
#ifdef EC448_ENABLED
	group_basic_test(lime::CurveId::c448, "group_one_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c448_server_port).data(), 10, true);
#endif
#ifdef HAVE_BCTBXPQ
	group_basic_test(lime::CurveId::c25519k512, "group_one_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519k512_server_port).data(), 10, true);
#endif
}

static void group_one_talking_bench() {
	if (!bench) return;
	int deviceNumber=10;
	uint64_t span=0,start = bctbx_get_cur_time_ms();
#ifdef EC25519_ENABLED
	while (span < maximumBenchTime) {
		start = bctbx_get_cur_time_ms();
		group_basic_test(lime::CurveId::c25519, "group_one_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519_server_port).data(), deviceNumber, true);
		// time spent in test is more or less linear to the device number, try to reach the one wich lead to a maximunBenchTime execution
		span = bctbx_get_cur_time_ms() - start;
		LIME_LOGE<<"Curve 25519 group chat test with "<<to_string(deviceNumber)<<" devices ran in "<<to_string(span)<<" ms"<<std::endl;
		deviceNumber *= int(std::max(float(maximumBenchTime)/float(span), 1.0f) * 1.2);
		deviceNumber += 3;
	}
#endif
#ifdef EC448_ENABLED
	deviceNumber=10;
	start = bctbx_get_cur_time_ms();
	span = 0;
	while (span < maximumBenchTime) {
		start = bctbx_get_cur_time_ms();
		group_basic_test(lime::CurveId::c448, "group_one_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c448_server_port).data(), deviceNumber, true);
		// time spent in test is more or less linear to the device number, try to reach the one wich lead to a maximunBenchTime execution
		span = bctbx_get_cur_time_ms() - start;
		LIME_LOGE<<"Curve 448 group chat test with "<<to_string(deviceNumber)<<" devices ran in "<<to_string(span)<<" ms"<<std::endl;
		deviceNumber *= int(std::max(float(maximumBenchTime)/float(span), 1.0f) * 1.2);
		deviceNumber += 3;
	}
#endif
#ifdef HAVE_BCTBXPQ
	deviceNumber=10;
	start = bctbx_get_cur_time_ms();
	span = 0;
	while (span < maximumBenchTime) {
		start = bctbx_get_cur_time_ms();
		group_basic_test(lime::CurveId::c25519k512, "group_one_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519k512_server_port).data(), deviceNumber, true);
		// time spent in test is more or less linear to the device number, try to reach the one wich lead to a maximunBenchTime execution
		span = bctbx_get_cur_time_ms() - start;
		LIME_LOGE<<"Curve 25519/Kyber 512 group chat test with "<<to_string(deviceNumber)<<" devices ran in "<<to_string(span)<<" ms"<<std::endl;
		deviceNumber *= int(std::max(float(maximumBenchTime)/float(span), 1.0f) * 1.2);
		deviceNumber += 3;
	}
#endif
}

static void group_one_talking_one_decrypt_bench() {
	if (!bench) return;
	int deviceNumber=10;
	uint64_t span=0,start = bctbx_get_cur_time_ms();
#ifdef EC25519_ENABLED
	while (span < maximumBenchTime) {
		start = bctbx_get_cur_time_ms();
		group_basic_test(lime::CurveId::c25519, "group_one_talking_one_decrypt", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519_server_port).data(), deviceNumber, true, true);
		// time spent in test is more or less linear to the device number, try to reach the one wich lead to a maximunBenchTime execution
		span = bctbx_get_cur_time_ms() - start;
		LIME_LOGE<<"Curve 25519 group chat test with "<<to_string(deviceNumber)<<" devices ran in "<<to_string(span)<<" ms"<<std::endl;
		deviceNumber *= int(std::max(float(maximumBenchTime)/float(span), 1.0f) * 1.2);
		deviceNumber += 3;
	}
#endif
#ifdef EC448_ENABLED
	deviceNumber=10;
	start = bctbx_get_cur_time_ms();
	span = 0;
	while (span < maximumBenchTime) {
		start = bctbx_get_cur_time_ms();
		group_basic_test(lime::CurveId::c448, "group_one_talking_one_decrypt", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c448_server_port).data(), deviceNumber, true, true);
		// time spent in test is more or less linear to the device number, try to reach the one wich lead to a maximunBenchTime execution
		span = bctbx_get_cur_time_ms() - start;
		LIME_LOGE<<"Curve 448 group chat test with "<<to_string(deviceNumber)<<" devices ran in "<<to_string(span)<<" ms"<<std::endl;
		deviceNumber *= int(std::max(float(maximumBenchTime)/float(span), 1.0f) * 1.2);
		deviceNumber += 3;
	}
#endif
#ifdef HAVE_BCTBXPQ
	deviceNumber=10;
	start = bctbx_get_cur_time_ms();
	span = 0;
	while (span < maximumBenchTime) {
		start = bctbx_get_cur_time_ms();
		group_basic_test(lime::CurveId::c25519k512, "group_one_talking_one_decrypt", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519k512_server_port).data(), deviceNumber, true, true);
		// time spent in test is more or less linear to the device number, try to reach the one wich lead to a maximunBenchTime execution
		span = bctbx_get_cur_time_ms() - start;
		LIME_LOGE<<"Curve 25519/Kyber 512 group chat test with "<<to_string(deviceNumber)<<" devices ran in "<<to_string(span)<<" ms"<<std::endl;
		deviceNumber *= int(std::max(float(maximumBenchTime)/float(span), 1.0f) * 1.2);
		deviceNumber += 3;
	}
#endif
}

static void group_all_talking() {
#ifdef EC25519_ENABLED
	group_basic_test(lime::CurveId::c25519, "group_all_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519_server_port).data(), 10);
#endif
#ifdef EC448_ENABLED
	group_basic_test(lime::CurveId::c448, "group_all_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c448_server_port).data(), 10);
#endif
#ifdef HAVE_BCTBXPQ
	group_basic_test(lime::CurveId::c25519k512, "group_all_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519k512_server_port).data(), 10);
#endif
}

static void group_all_talking_bench() {
	if (!bench) return;
	int deviceNumber=10;
	uint64_t span=0,start = bctbx_get_cur_time_ms();
#ifdef EC25519_ENABLED
	while (span < maximumBenchTime) {
		start = bctbx_get_cur_time_ms();
		group_basic_test(lime::CurveId::c25519, "group_all_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519_server_port).data(), deviceNumber);
		// time spent in test is more or less linear to the square of device number, try to reach the one wich lead to a maximunBenchTime execution
		span = bctbx_get_cur_time_ms() - start;
		LIME_LOGE<<"Curve 25519 group chat test with "<<to_string(deviceNumber)<<" devices ran in "<<to_string(span)<<" ms"<<std::endl;
		deviceNumber *= int(std::sqrt(std::max(float(maximumBenchTime)/float(span), 1.0f)) * 1.2);
		deviceNumber += 3;
	}
#endif
#ifdef EC448_ENABLED
	deviceNumber=10;
	start = bctbx_get_cur_time_ms();
	span = 0;
	while (span < maximumBenchTime) {
		start = bctbx_get_cur_time_ms();
		group_basic_test(lime::CurveId::c448, "group_all_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c448_server_port).data(), deviceNumber);
		// time spent in test is more or less linear to the square of device number, try to reach the one wich lead to a maximunBenchTime execution
		span = bctbx_get_cur_time_ms() - start;
		LIME_LOGE<<"Curve 448 group chat test with "<<to_string(deviceNumber)<<" devices ran in "<<to_string(span)<<" ms"<<std::endl;
		deviceNumber *= int(std::sqrt(std::max(float(maximumBenchTime)/float(span), 1.0f)) * 1.2);
		deviceNumber += 3;
	}
#endif
#ifdef HAVE_BCTBXPQ
	deviceNumber=10;
	start = bctbx_get_cur_time_ms();
	span = 0;
	while (span < maximumBenchTime) {
		start = bctbx_get_cur_time_ms();
		group_basic_test(lime::CurveId::c25519k512, "group_all_talking", std::string("https://").append(lime_tester::test_x3dh_server_url).append(":").append(lime_tester::test_x3dh_c25519k512_server_port).data(), deviceNumber);
		// time spent in test is more or less linear to the square of device number, try to reach the one wich lead to a maximunBenchTime execution
		span = bctbx_get_cur_time_ms() - start;
		LIME_LOGE<<"Curve 25519/Kyber 512 group chat test with "<<to_string(deviceNumber)<<" devices ran in "<<to_string(span)<<" ms"<<std::endl;
		deviceNumber *= int(std::sqrt(std::max(float(maximumBenchTime)/float(span), 1.0f)) * 1.2);
		deviceNumber += 3;
	}
#endif
}

static test_t tests[] = {
	TEST_NO_TAG("One message each", group_all_talking),
	TEST_NO_TAG("One message each Bench", group_all_talking_bench),
	TEST_NO_TAG("One encrypt to all", group_one_talking),
	TEST_NO_TAG("One encrypt to all Bench", group_one_talking_bench),
	TEST_NO_TAG("One encrypt to all Only one decrypt Bench", group_one_talking_one_decrypt_bench),
};

test_suite_t lime_massive_group_test_suite = {
	"massive group",
	http_before_all,
	http_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests,
	0,
	0
};
