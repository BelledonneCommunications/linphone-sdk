/*
	lime_lime-tester.cpp
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

int wait_for_timeout=4000;
std::string test_x3dh_server_url{"localhost"};
std::string test_x3dh_c25519_server_port{"25519"};
std::string test_x3dh_c448_server_port{"25520"};

struct events_counters_t {
	int operation_success;
	int operation_failed;
	events_counters_t() : operation_success{0}, operation_failed{0} {};
	bool operator==(const events_counters_t &b) const {return this->operation_success==b.operation_success && this->operation_failed==b.operation_failed;}
};

static belle_sip_stack_t *stack=NULL;
static belle_http_provider_t *prov=NULL;

static int wait_for(belle_sip_stack_t*s1,int* counter,int value,int timeout) {
	int retry=0;
#define SLEEP_TIME 50
	while (*counter!=value && retry++ <(timeout/SLEEP_TIME)) {
		if (s1) belle_sip_stack_sleep(s1,SLEEP_TIME);
	}
	if (*counter!=value) return FALSE;
	else return TRUE;
}

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


/* This function will destroy and recreate managers given in parameter, force deleting all internal cache and start back from what is in local Storage */
static void managersClean(std::unique_ptr<LimeManager> &alice, std::unique_ptr<LimeManager> &bob, std::string aliceDb, std::string bobDb) {
	alice = nullptr;
	bob = nullptr;
	alice = unique_ptr<lime::LimeManager>(new lime::LimeManager(aliceDb, prov));
	bob = std::unique_ptr<lime::LimeManager>(new lime::LimeManager(bobDb, prov));
	bctbx_message("Trash and reload alice and bob LimeManagers");
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

	events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						bctbx_error("Lime operation failed : %s", anythingToSay.data());
					}
				});

	// create Manager
	auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, prov));
	auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, prov));

	// create Random devices names
	auto aliceDevice1 = makeRandomDeviceName("alice.d1.");
	auto bobDevice1 = makeRandomDeviceName("bob.d1.");

	// create users alice.d1 and bob.d1
	try {
		aliceManager->create_user(*aliceDevice1, x3dh_server_url, curve, callback);
		bobManager->create_user(*bobDevice1, x3dh_server_url, curve, callback);
		expected_success +=2; // we have two asynchronous operation on going
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success, expected_success,wait_for_timeout)); // we must get a callback saying all went well
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to create Lime Users");
		return; // no reason to continue the test
	}

	// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// alice.d1 encrypts a message for bob.d1, bob replies
	auto aliceRecipients = make_shared<std::vector<recipientData>>();
	aliceRecipients->emplace_back(*bobDevice1);
	auto aliceMessage = make_shared<std::vector<uint8_t>>(lime_messages_pattern[0].begin(), lime_messages_pattern[0].end());
	auto aliceCipherMessage = make_shared<std::vector<uint8_t>>();

	auto bobRecipients = make_shared<std::vector<recipientData>>();
	bobRecipients->emplace_back(*aliceDevice1);
	auto bobMessage = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[1].begin(), lime_messages_pattern[1].end());
	auto bobCipherMessage = make_shared<std::vector<uint8_t>>();

	try {
		// alice encrypt
		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,++expected_success,wait_for_timeout)); // we must get a callback saying all went well

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// bob decrypt
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_TRUE(DR_message_holdsX3DHInit((*aliceRecipients)[0].cipherHeader)); // new sessions created, they must convey X3DH init message
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice1, "bob", *aliceDevice1, (*aliceRecipients)[0].cipherHeader, *aliceCipherMessage, receivedMessage));
		std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[0]);

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// bob encrypt
		bobManager->encrypt(*bobDevice1, make_shared<const std::string>("alice"), bobRecipients, bobMessage, bobCipherMessage, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,++expected_success,wait_for_timeout)); // we must get a callback saying all went well

		// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
		if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

		// alice decrypt
		BC_ASSERT_FALSE(DR_message_holdsX3DHInit((*bobRecipients)[0].cipherHeader)); // Bob didn't initiate a new session created, so no X3DH init message
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDevice1, "alice", *bobDevice1, (*bobRecipients)[0].cipherHeader, *bobCipherMessage, receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[1]);
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Initial session establishment between alice.d1 and bob.d2 failed");
		return; // no reason to continue the test
	}

	try {
		// Alice encrypt maxSendingChain messages to bob, none shall have the X3DH init
		for (auto i=0; i<lime::settings::maxSendingChain; i++) {
			// alice encrypt
			aliceMessage->assign(lime_messages_pattern[i%lime_messages_pattern.size()].begin(), lime_messages_pattern[i%lime_messages_pattern.size()].end());
			aliceCipherMessage->clear();
			aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
			BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,++expected_success,wait_for_timeout)); // we must get a callback saying all went well

			// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
			if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

			BC_ASSERT_FALSE(DR_message_holdsX3DHInit((*aliceRecipients)[0].cipherHeader)); // it's an ongoing session, no X3DH init
			/*
			// bob decrypt, it's not really needed here but...
			std::vector<uint8_t> receivedMessage{};
			BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice1, "bob", *aliceDevice1, (*aliceRecipients)[0].cipherHeader, *aliceCipherMessage, receivedMessage));
			std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
			BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[i%lime_messages_pattern.size()]);
			*/
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to encrypt long batch");
		return; // no reason to continue the test
	}

	// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	try {
		// alice encrypt, we are over the maximum number, so Alice shall fetch a new key on server and start a new session
		aliceMessage->assign(lime_messages_pattern[0].begin(), lime_messages_pattern[0].end());
		aliceCipherMessage->clear();
		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,++expected_success,wait_for_timeout)); // we must get a callback saying all went well

		// bob decrypt, it's not really needed here but...
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_TRUE(DR_message_holdsX3DHInit((*aliceRecipients)[0].cipherHeader)); // we started a new session
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice1, "bob", *aliceDevice1, (*aliceRecipients)[0].cipherHeader, *aliceCipherMessage, receivedMessage));
		std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[0]);
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail final exchange");
		return; // no reason to continue the test
	}

	// delete the users so the remote DB will be clean too
	try {
		aliceManager->delete_user(*aliceDevice1, callback);
		bobManager->delete_user(*bobDevice1, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success+2,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to delete users");
		return; // no reason to continue the test
	}
}
static void x3dh_sending_chain_limit() {
#ifdef EC25519_ENABLED
	x3dh_sending_chain_limit_test(lime::CurveId::c25519, "lime_x3dh_multiple_DRsessions", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data());
	x3dh_sending_chain_limit_test(lime::CurveId::c25519, "lime_x3dh_multiple_DRsessions", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c25519_server_port).data(), false);
#endif
#ifdef EC448_ENABLED
	x3dh_sending_chain_limit_test(lime::CurveId::c448, "lime_x3dh_multiple_DRsessions", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data());
	x3dh_sending_chain_limit_test(lime::CurveId::c448, "lime_x3dh_multiple_DRsessions", std::string("https://").append(test_x3dh_server_url).append(":").append(test_x3dh_c448_server_port).data(), false);
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

	events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						bctbx_error("Lime operation failed : %s", anythingToSay.data());
					}
				});

	// create Manager
	auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, prov));
	auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, prov));

	// create Random devices names
	auto aliceDevice1 = makeRandomDeviceName("alice.d1.");
	auto bobDevice1 = makeRandomDeviceName("bob.d1.");

	// create users alice.d1 and bob.d1
	try {
		aliceManager->create_user(*aliceDevice1, x3dh_server_url, curve, callback);
		bobManager->create_user(*bobDevice1, x3dh_server_url, curve, callback);
		expected_success +=2; // we have two asynchronous operation on going
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success, expected_success,wait_for_timeout)); // we must get a callback saying all went well
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to create Lime Users");
		return; // no reason to continue the test
	}

	// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// alice.d1 encrypts a message for bob.d1 and bob.d1 encrypts a message for alice.d1
	auto aliceRecipients = make_shared<std::vector<recipientData>>();
	aliceRecipients->emplace_back(*bobDevice1);
	auto bobRecipients = make_shared<std::vector<recipientData>>();
	bobRecipients->emplace_back(*aliceDevice1);
	auto aliceMessage = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[0].begin(), lime_messages_pattern[0].end());
	auto aliceCipherMessage = make_shared<std::vector<uint8_t>>();
	auto bobMessage = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[1].begin(), lime_messages_pattern[1].end());
	auto bobCipherMessage = make_shared<std::vector<uint8_t>>();

	try {
		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), aliceRecipients, aliceMessage, aliceCipherMessage, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,++expected_success,wait_for_timeout)); // we must get a callback saying all went well
		bobManager->encrypt(*bobDevice1, make_shared<const std::string>("alice"), bobRecipients, bobMessage, bobCipherMessage, callback);
		expected_success += 1;
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to encrypt crossed messages between alice.d1 and bob.d1");
		return; // no reason to continue the test
	}

	// DB are cleans, so we shall have only one session between pairs and it shall have Id 1, check it
	try {
		std::vector<long int> aliceSessionsId{};
		auto aliceActiveSessionId =  get_DRsessionsId(dbFilenameAlice, *aliceDevice1, *bobDevice1, aliceSessionsId);
		BC_ASSERT_EQUAL(aliceActiveSessionId, 1, long int, "%ld");
		BC_ASSERT_EQUAL(aliceSessionsId.size(), 1, size_t, "%ld");

		std::vector<long int> bobSessionsId{};
		auto bobActiveSessionId =  get_DRsessionsId(dbFilenameBob, *bobDevice1, *aliceDevice1, bobSessionsId);
		BC_ASSERT_EQUAL(bobActiveSessionId, 1, long int, "%ld");
		BC_ASSERT_EQUAL(bobSessionsId.size(), 1, size_t, "%ld");
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Couldn't fetch sessionsId from Db");
		return;
	}

	// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// both decrypt the messages, they have crossed on the network
	try {
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_TRUE(DR_message_holdsX3DHInit((*aliceRecipients)[0].cipherHeader)); // new sessions created, they must convey X3DH init message
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice1, "bob", *aliceDevice1, (*aliceRecipients)[0].cipherHeader, *aliceCipherMessage, receivedMessage));
		std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[0]);

		BC_ASSERT_TRUE(DR_message_holdsX3DHInit((*bobRecipients)[0].cipherHeader)); // new sessions created, they must convey X3DH init message
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDevice1, "alice", *bobDevice1, (*bobRecipients)[0].cipherHeader, *bobCipherMessage, receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[1]);
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to decrypt swap message betwwen alice.d1 and bob.d1");
		return; // no reason to continue the test
	}

	// Now DB shall holds 2 sessions each, and both shall have their active as session 2 as the active one must be the last one used for decrypt
	try {
		std::vector<long int> aliceSessionsId{};
		auto aliceActiveSessionId =  get_DRsessionsId(dbFilenameAlice, *aliceDevice1, *bobDevice1, aliceSessionsId);
		BC_ASSERT_EQUAL(aliceActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(aliceSessionsId.size(), 2, size_t, "%ld");

		std::vector<long int> bobSessionsId{};
		auto bobActiveSessionId =  get_DRsessionsId(dbFilenameBob, *bobDevice1, *aliceDevice1, bobSessionsId);
		BC_ASSERT_EQUAL(bobActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(bobSessionsId.size(), 2, size_t, "%ld");
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Couldn't fetch sessionsId from Db");
		return;
	}

	// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// bob.d1 encrypts a message for alice.d1, it shall use the active session, which is his session 2(used to decrypt alice message) but matched session 1(used to encrypt the first message) in alice DB
	bobRecipients = make_shared<std::vector<recipientData>>();
	bobRecipients->emplace_back(*aliceDevice1);
	bobMessage = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[2].begin(), lime_messages_pattern[2].end());
	bobCipherMessage = make_shared<std::vector<uint8_t>>();

	try {
		bobManager->encrypt(*bobDevice1, make_shared<const std::string>("alice"), bobRecipients, bobMessage, bobCipherMessage, callback);
		expected_success += 1;
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to encrypt message from bob.d1 to alice.d1");
		return; // no reason to continue the test
	}

	// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// check nothing has changed in terms of active and sessions Id in DB
	try {
		std::vector<long int> aliceSessionsId{};
		auto aliceActiveSessionId =  get_DRsessionsId(dbFilenameAlice, *aliceDevice1, *bobDevice1, aliceSessionsId);
		BC_ASSERT_EQUAL(aliceActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(aliceSessionsId.size(), 2, size_t, "%ld");

		std::vector<long int> bobSessionsId{};
		auto bobActiveSessionId =  get_DRsessionsId(dbFilenameBob, *bobDevice1, *aliceDevice1, bobSessionsId);
		BC_ASSERT_EQUAL(bobActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(bobSessionsId.size(), 2, size_t, "%ld");
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Couldn't fetch sessionsId from Db");
		return;
	}

	// alice decrypt the messages it shall set back session 1 to be the active one as bob used this one to encrypt to alice, they have now converged on a session
	try {
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_FALSE(DR_message_holdsX3DHInit((*bobRecipients)[0].cipherHeader)); // it is not a new session, no more X3DH message
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDevice1, "alice", *bobDevice1, (*bobRecipients)[0].cipherHeader, *bobCipherMessage, receivedMessage));
		std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[2]);
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to decrypt swap message betwwen alice.d1 and bob.d1");
		return; // no reason to continue the test
	}

	// check we have the expected configuration: 2 sessions in each base, session 1 active for alice, session 2 for bob
	try {
		std::vector<long int> aliceSessionsId{};
		auto aliceActiveSessionId =  get_DRsessionsId(dbFilenameAlice, *aliceDevice1, *bobDevice1, aliceSessionsId);
		BC_ASSERT_EQUAL(aliceActiveSessionId, 1, long int, "%ld");
		BC_ASSERT_EQUAL(aliceSessionsId.size(), 2, size_t, "%ld");

		std::vector<long int> bobSessionsId{};
		auto bobActiveSessionId =  get_DRsessionsId(dbFilenameBob, *bobDevice1, *aliceDevice1, bobSessionsId);
		BC_ASSERT_EQUAL(bobActiveSessionId, 2, long int, "%ld");
		BC_ASSERT_EQUAL(bobSessionsId.size(), 2, size_t, "%ld");
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Couldn't fetch sessionsId from Db");
		return;
	}

	// delete the users so the remote DB will be clean too
	try {
		aliceManager->delete_user(*aliceDevice1, callback);
		bobManager->delete_user(*bobDevice1, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success+2,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to delete users");
		return; // no reason to continue the test
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

	events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						bctbx_error("Lime operation failed : %s", anythingToSay.data());
					}
				});

	// create Manager
	auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, prov));
	auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, prov));

	// create Random devices names
	auto aliceDevice1 = makeRandomDeviceName("alice.d1.");
	auto bobDevice1 = makeRandomDeviceName("bob.d1.");
	auto bobDevice2 = makeRandomDeviceName("bob.d2.");
	auto bobDevice3 = makeRandomDeviceName("bob.d3.");
	auto bobDevice4 = makeRandomDeviceName("bob.d4.");

	// create users alice.d1 and bob.d1,d2,d3,d4
	try {
		aliceManager->create_user(*aliceDevice1, x3dh_server_url, curve, callback);
		bobManager->create_user(*bobDevice1, x3dh_server_url, curve, callback);
		bobManager->create_user(*bobDevice2, x3dh_server_url, curve, callback);
		bobManager->create_user(*bobDevice3, x3dh_server_url, curve, callback);
		bobManager->create_user(*bobDevice4, x3dh_server_url, curve, callback);
		expected_success +=5; // we have two asynchronous operation on going
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success, expected_success,wait_for_timeout)); // we must get a callback saying all went well
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to create Lime Users");
		return; // no reason to continue the test
	}

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
		messages[i] = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[i].begin(), lime_messages_pattern[i].end());
	}

	try {
		for (size_t i=0; i<messages.size(); i++) {
			aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients[i], messages[i], cipherMessage[i], callback);
			expected_success++;
		}
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to encrypt messages from alice.d1 to bob.d1");
		return; // no reason to continue the test
	}

	// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	try {
		std::vector<uint8_t> X3DH_initMessageBuffer{}; // store the X3DH init message extracted from first message of the burst to be able to compare it to the following one, they must be the same.
		// loop on cipher message and decrypt them bob Manager
		for (size_t i=0; i<messages.size(); i++) {
			for (auto &recipient : *(recipients[i])) {
				std::vector<uint8_t> receivedMessage{};
				BC_ASSERT_TRUE(DR_message_holdsX3DHInit(recipient.cipherHeader)); // new sessions created, they must convey X3DH init message
				if (i==0) { // first message of the burst, extract and store the X3DH init message
					DR_message_extractX3DHInit(recipient.cipherHeader, X3DH_initMessageBuffer);
				} else { // following message of the burst, extract X3DH init message and compare it to the first one, they must be the same, we shall not create new sessions
					std::vector<uint8_t> X3DH_initMessageBuffer_next{};
					DR_message_extractX3DHInit(recipient.cipherHeader, X3DH_initMessageBuffer_next);
					BC_ASSERT_TRUE(X3DH_initMessageBuffer == X3DH_initMessageBuffer_next);
				}
				BC_ASSERT_TRUE(bobManager->decrypt(recipient.deviceId, "bob", *aliceDevice1, recipient.cipherHeader, *(cipherMessage[i]), receivedMessage));
				std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
				BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[i]);
			}
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to decrypt messages from Alice to bob.d1");
		return; // no reason to continue the test
	}

	// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// now alice will request encryption of message without waiting for callback to:
	// clean previously used buffer
	for (size_t i=0; i<messages.size(); i++) {
		cipherMessage[i] = make_shared<std::vector<uint8_t>>();
		recipients[i] = make_shared<std::vector<recipientData>>();
		// use pattern messages not used yet
		messages[i] = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[messages.size()+i].begin(), lime_messages_pattern[messages.size()+i].end());
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

	try {
		for (size_t i=0; i<messages.size(); i++) {
			aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients[i], messages[i], cipherMessage[i], callback);
			expected_success++;
		}
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to encrypt messages from alice.d1 to bob.d1, bob.d2, bob.d3 and bob.d4");
		return; // no reason to continue the test
	}

	// destroy and reload the Managers(tests everything is correctly saved/load from local Storage)
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// decrypt all sent messages and check if they match
	try {
		// recipients holds:
		// recipients[0] -> bob.d1, bob.d2
		// recipents[1] -> bob.d1
		// recipients[2] -> bob.d2
		// Check on these that the X3DH init message are matching (we didn't create a second session an encryption was queued correctly)
		std::vector<uint8_t> X3DH_initMessageBuffer1{};
		std::vector<uint8_t> X3DH_initMessageBuffer2{};
		// recipients[0][0] and recipients[1][0]
		DR_message_extractX3DHInit((*recipients[0])[0].cipherHeader, X3DH_initMessageBuffer1);
		DR_message_extractX3DHInit((*recipients[1])[0].cipherHeader, X3DH_initMessageBuffer2);
		BC_ASSERT_TRUE(X3DH_initMessageBuffer1 == X3DH_initMessageBuffer2);
		// recipients[0][1] and recipients[2][0]
		DR_message_extractX3DHInit((*recipients[0])[1].cipherHeader, X3DH_initMessageBuffer1);
		DR_message_extractX3DHInit((*recipients[2])[0].cipherHeader, X3DH_initMessageBuffer2);
		BC_ASSERT_TRUE(X3DH_initMessageBuffer1 == X3DH_initMessageBuffer2);

		// in recipient[0] we have a message encrypted for bob.d1 and bob.d2
		std::vector<uint8_t> receivedMessage{};
		std::string receivedMessageString{};
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[0])[0].deviceId, "bob", *aliceDevice1, (*recipients[0])[0].cipherHeader, *(cipherMessage[0]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[messages.size()+0]);
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[0])[1].deviceId, "bob", *aliceDevice1, (*recipients[0])[1].cipherHeader, *(cipherMessage[0]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[messages.size()+0]);

		// in recipient[1] we have a message encrypted to bob.d1
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[1])[0].deviceId, "bob", *aliceDevice1, (*recipients[1])[0].cipherHeader, *(cipherMessage[1]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[messages.size()+1]);

		// in recipient[2] we have a message encrypted to bob.d2
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[2])[0].deviceId, "bob", *aliceDevice1, (*recipients[2])[0].cipherHeader, *(cipherMessage[2]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[messages.size()+2]);

		// in recipient[2] we have a message encrypted to bob.d3
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[3])[0].deviceId, "bob", *aliceDevice1, (*recipients[3])[0].cipherHeader, *(cipherMessage[3]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[messages.size()+3]);

		// in recipient[2] we have a message encrypted to bob.d4
		BC_ASSERT_TRUE(bobManager->decrypt((*recipients[4])[0].deviceId, "bob", *aliceDevice1, (*recipients[4])[0].cipherHeader, *(cipherMessage[4]), receivedMessage));
		receivedMessageString = std::string{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[messages.size()+4]);

	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to decrypt messages from alice.d1 to bob.d1, bob.d2, bob.d3 and bob.d4");
		return; // no reason to continue the test
	}

	// everything went well so delete the users so the remote DB will be clean too
	try {
		aliceManager->delete_user(*aliceDevice1, callback);
		bobManager->delete_user(*bobDevice1, callback);
		bobManager->delete_user(*bobDevice2, callback);
		bobManager->delete_user(*bobDevice3, callback);
		bobManager->delete_user(*bobDevice4, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success+5,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to delete users");
		return; // no reason to continue the test
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

	events_counters_t counters={};
	int expected_success=0;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						bctbx_error("Lime operation failed : %s", anythingToSay.data());
					}
				});

	// create Manager
	auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, prov));
	auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, prov));

	// create Random devices names
	auto aliceDevice1 = makeRandomDeviceName("alice.d1.");
	auto bobDevice1 = makeRandomDeviceName("bob.d1.");

	// create users alice.d1 and bob.d1
	try {
		aliceManager->create_user(*aliceDevice1, x3dh_server_url, curve, callback);
		bobManager->create_user(*bobDevice1, x3dh_server_url, curve, callback);
		expected_success +=2; // we have two asynchronous operation on going
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success, expected_success,wait_for_timeout)); // we must get callback saying all went well
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this
	} catch (BctbxException &e) {
		BCTBX_SLOGE << e;
		BC_FAIL("Fail to create Lime Users");
		return; // no reason to continue the test
	}

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
		messages[i] = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[i].begin(), lime_messages_pattern[i].end());
	}

	try {
		for (size_t i=0; i<messages.size(); i++) {
			aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients[i], messages[i], cipherMessage[i], callback);
			expected_success++;
		}
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE << e;
		BC_FAIL("Fail to encrypt messages from alice.d1 to bob.d1");
		return; // no reason to continue the test
	}

	/* destroy and reload the Managers(tests everything is correctly saved/load from local Storage) */
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// bob.d1 decrypt the messages
	try {
		std::vector<uint8_t> X3DH_initMessageBuffer{}; // store the X3DH init message extracted from first message of the burst to be able to compare it to the following one, they must be the same.
		// loop on cipher message and decrypt them bob Manager
		for (size_t i=0; i<messages.size(); i++) {
			for (auto &recipient : *(recipients[i])) {
				std::vector<uint8_t> receivedMessage{};
				BC_ASSERT_TRUE(DR_message_holdsX3DHInit(recipient.cipherHeader)); // new sessions created, they must convey X3DH init message
				if (i==0) { // first message of the burst, extract and store the X3DH init message
					DR_message_extractX3DHInit(recipient.cipherHeader, X3DH_initMessageBuffer);
				} else { // following message of the burst, extract X3DH init message and compare it to the first one, they must be the same, we shall not create new sessions
					std::vector<uint8_t> X3DH_initMessageBuffer_next{};
					DR_message_extractX3DHInit(recipient.cipherHeader, X3DH_initMessageBuffer_next);
					BC_ASSERT_TRUE(X3DH_initMessageBuffer == X3DH_initMessageBuffer_next);
				}
				BC_ASSERT_TRUE(bobManager->decrypt(recipient.deviceId, "bob", *aliceDevice1, recipient.cipherHeader, *(cipherMessage[i]), receivedMessage));
				std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
				BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[i]);
			}
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE << e;
		BC_FAIL("Fail to decrypt messages from Alice to bob.d1");
		return; // no reason to continue the test
	}

	// everything went well so delete the users so the remote DB will be clean too
	try {
		aliceManager->delete_user(*aliceDevice1, callback);
		bobManager->delete_user(*bobDevice1, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success+2,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE << e;
		BC_FAIL("Fail to delete users");
		return; // no reason to continue the test
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

	events_counters_t counters={};
	int expected_success=1;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						bctbx_error("Lime operation failed : %s", anythingToSay.data());
					}
				});

	// create Manager
	auto aliceManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, prov));
	auto bobManager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameBob, prov));

	// create Random devices names
	auto aliceDevice1 = makeRandomDeviceName("alice.d1.");
	auto bobDevice1 = makeRandomDeviceName("bob.d1.");
	auto bobDevice2 = makeRandomDeviceName("bob.d2.");

	// create users
	try {
		aliceManager->create_user(*aliceDevice1, x3dh_server_url, curve, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well
		bobManager->create_user(*bobDevice1, x3dh_server_url, curve, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well
		bobManager->create_user(*bobDevice2, x3dh_server_url, curve, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;
		BC_FAIL("Fail to create Lime Users");
		return; // no reason to continue the test
	}

	/* destroy and reload the Managers(tests everything is correctly saved/load from local Storage) */
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// alice send a message to bob
	auto recipients = make_shared<std::vector<recipientData>>();
	recipients->emplace_back(*bobDevice1);
	recipients->emplace_back(*bobDevice2);
	auto message = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[0].begin(), lime_messages_pattern[0].end());
	auto cipherMessage = make_shared<std::vector<uint8_t>>();

	try {
		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to encrypt message from Alice to bob.d1, bob.d2");
		return; // no reason to continue the test
	}

	try {
		// loop on cipher message and decrypt with bob Manager
		for (auto &recipient : *recipients) {
			std::vector<uint8_t> receivedMessage{};
			BC_ASSERT_TRUE(DR_message_holdsX3DHInit(recipient.cipherHeader)); // new sessions created, they must convey X3DH init message
			BC_ASSERT_TRUE(bobManager->decrypt(recipient.deviceId, "bob", *aliceDevice1, recipient.cipherHeader, *cipherMessage, receivedMessage));
			std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
			BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[0]);
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;
		BC_FAIL("Fail to decrypt message from Alice to bob.d1, bob.d2");
		return; // no reason to continue the test
	}

	// cleaning
	recipients->clear();
	message = nullptr;
	cipherMessage->clear();
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// encrypt another one, same recipients(we shall have no new X3DH session but still the X3DH init message)
	recipients->emplace_back(*bobDevice1);
	recipients->emplace_back(*bobDevice2);
	message = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[1].begin(), lime_messages_pattern[1].end());

	try {
		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well

		// loop on cipher message and decrypt with bob Manager
		for (auto &recipient : *recipients) {
			std::vector<uint8_t> receivedMessage{};
			BC_ASSERT_TRUE(DR_message_holdsX3DHInit(recipient.cipherHeader)); // again X3DH message as no one ever responded to alice.d1
			BC_ASSERT_TRUE(bobManager->decrypt(recipient.deviceId, "bob", *aliceDevice1, recipient.cipherHeader, *cipherMessage, receivedMessage));
			std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
			BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[1]);
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Second encrypt/decrypt message from Alice to bob.d1, bob.d2 failed");
		return; // no reason to continue the test
	}

	// cleaning
	recipients->clear();
	message = nullptr;
	cipherMessage->clear();
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}


	// bob.d1 reply to alice and copy bob.d2
	recipients->emplace_back(*aliceDevice1);
	recipients->emplace_back(*bobDevice2);
	message = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[2].begin(), lime_messages_pattern[2].end());

	try {
		bobManager->encrypt(*bobDevice1, make_shared<const std::string>("alice"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;
		BC_FAIL("bob.d1 reply to alice.d1 and bob.d2failed");
		return; // no reason to continue the test
	}


	// decrypt it
	try {
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_FALSE(DR_message_holdsX3DHInit((*recipients)[0].cipherHeader)); // alice.d1 to bob.d1 already set up the DR Session, we shall not have any  X3DH message here
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDevice1, "alice", *bobDevice1, (*recipients)[0].cipherHeader, *cipherMessage, receivedMessage));
		std::string receivedMessageStringAlice{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageStringAlice == lime_messages_pattern[2]);
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("alice.d1 couldn't decipher bob.d1 response");
		return; // no reason to continue the test
	}

	try {
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_TRUE(DR_message_holdsX3DHInit((*recipients)[1].cipherHeader)); // bob.d1 to bob.d1 is a new session, we must have a X3DH message here
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice2, "alice", *bobDevice1, (*recipients)[1].cipherHeader, *cipherMessage, receivedMessage));
		std::string receivedMessageStringBob{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageStringBob == lime_messages_pattern[2]);
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;
		BC_FAIL("bob.d2 couldn't decipher bob.d1 response");
		return; // no reason to continue the test
	}

	// cleaning
	recipients->clear();
	message = nullptr;
	cipherMessage->clear();
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// Now do bob.d2 to alice and bob.d1 every one has an open session towards everyo
	recipients->emplace_back(*aliceDevice1);
	recipients->emplace_back(*bobDevice1);
	message = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[3].begin(), lime_messages_pattern[3].end());

	try {
		bobManager->encrypt(*bobDevice2, make_shared<const std::string>("alice"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well

		// decrypt it
		std::vector<uint8_t> receivedMessage{};
		BC_ASSERT_FALSE(DR_message_holdsX3DHInit((*recipients)[0].cipherHeader)); // alice.d1 to bob.d2 already set up the DR Session, we shall not have any  X3DH message here
		BC_ASSERT_TRUE(aliceManager->decrypt(*aliceDevice1, "alice", *bobDevice2, (*recipients)[0].cipherHeader, *cipherMessage, receivedMessage));
		std::string receivedMessageStringAlice{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageStringAlice == lime_messages_pattern[3]);
		receivedMessage.clear();
		BC_ASSERT_FALSE(DR_message_holdsX3DHInit((*recipients)[1].cipherHeader)); // bob.d1 to bob.d2 already set up the DR Session, we shall not have any  X3DH message here
		BC_ASSERT_TRUE(bobManager->decrypt(*bobDevice1, "alice", *bobDevice2, (*recipients)[1].cipherHeader, *cipherMessage, receivedMessage));
		std::string receivedMessageStringBob{receivedMessage.begin(), receivedMessage.end()};
		BC_ASSERT_TRUE(receivedMessageStringBob == lime_messages_pattern[3]);
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;
		BC_FAIL("bob.d2 to alice.d1 and bob.d1 failed");
		return; // no reason to continue the test
	}

	// cleaning
	recipients->clear();
	message = nullptr;
	cipherMessage->clear();
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// encrypt another one from alice to bob.d1 and .d2, it must not send X3DH init anymore
	recipients->emplace_back(*bobDevice1);
	recipients->emplace_back(*bobDevice2);
	message = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[4].begin(), lime_messages_pattern[4].end());

	try {
		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well

		// loop on cipher message and decrypt with bob Manager
		for (auto &recipient : *recipients) {
			std::vector<uint8_t> receivedMessage{};
			BC_ASSERT_FALSE(DR_message_holdsX3DHInit(recipient.cipherHeader)); // bob.d1 and d2 both responded, so no more X3DH message shall be sent
			BC_ASSERT_TRUE(bobManager->decrypt(recipient.deviceId, "bob", *aliceDevice1, recipient.cipherHeader, *cipherMessage, receivedMessage));
			std::string receivedMessageString{receivedMessage.begin(), receivedMessage.end()};
			BC_ASSERT_TRUE(receivedMessageString == lime_messages_pattern[4]);
		}
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;
		BC_FAIL("Second encrypt/decrypt message from Alice to bob.d1, bob.d2 failed");
		return; // no reason to continue the test
	}

	// cleaning
	recipients->clear();
	message = nullptr;
	cipherMessage->clear();
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// try to encrypt to an unknown user, it must fail in callback
	recipients->emplace_back("bob.d3");
	message = make_shared<const std::vector<uint8_t>>(lime_messages_pattern[5].begin(), lime_messages_pattern[5].end());

	try {
		aliceManager->encrypt(*aliceDevice1, make_shared<const std::string>("bob"), recipients, message, cipherMessage, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_failed,1,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Attempt to encrypt to an unknown user generate an exception instead of simple failure message in callback");
		return; // no reason to continue the test
	}


	// cleaning
	recipients->clear();
	message = nullptr;
	cipherMessage->clear();
	if (!continuousSession) { managersClean (aliceManager, bobManager, dbFilenameAlice, dbFilenameBob);}

	// everything went well so delete the users so the remote DB will be clean too
	try {
		aliceManager->delete_user(*aliceDevice1, callback);
		bobManager->delete_user(*bobDevice1, callback);
		bobManager->delete_user(*bobDevice2, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success+2,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to delete users");
		return; // no reason to continue the test
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

	events_counters_t counters={};
	int expected_success=1;

	limeCallback callback([&counters](lime::callbackReturn returnCode, std::string anythingToSay) {
					if (returnCode == lime::callbackReturn::success) {
						counters.operation_success++;
					} else {
						counters.operation_failed++;
						BCTBX_SLOGI <<"Insert Lime user failed : "<< anythingToSay.data() ;
					}
				});
	// we need a LimeManager
	auto Manager = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAlice, prov));

	auto aliceDeviceName = makeRandomDeviceName("alice.");

	/* create a user in a fresh database */
	try {
		Manager->create_user(*aliceDeviceName, x3dh_server_url, curve, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well
		if (counters.operation_failed == 1) return; // skip the end of the test if we can't do this
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to create Lime User");
		return; // no reason to continue the test
	}

	/* Try to load alice from from DB */
	try {
		auto alice = load_LimeUser(dbFilenameAlice, *aliceDeviceName, prov);
		/* no need to wait here, it shall load alice immediately */
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to create Lime User");
		return; // no reason to continue the test
	}

	bool gotExpectedException = false;
	/* Try to create the same user in the same data base, it must fail */
	try {
		auto alice = insert_LimeUser(dbFilenameAlice, *aliceDeviceName, x3dh_server_url, curve, prov, callback);
		/* no need to wait here, it must fail immediately */
	} catch (BctbxException &e) {
		gotExpectedException = true;
	}
	if (!gotExpectedException) {
		// we didn't got any exception on trying to insert alice again in DB
		BC_FAIL("Insert twice the same userID in a DB without getting any exception");
		return;
	}

	/* Try to load a user which is not in DB, it must fail */
	gotExpectedException = false;
	try {
		auto alice = load_LimeUser(dbFilenameAlice, "bob", prov);
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
		// delete Alice, wait for callback confirmation from server
		Manager->delete_user(*aliceDeviceName, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well
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
	BC_ASSERT_FALSE(wait_for(stack,&counters.operation_failed,savedCounters.operation_failed+1,wait_for_timeout/2)); // give we few seconds to possible call to the callback(it shall not occurs
	// check we didn't land into call back as any call to it will modify the counters
	BC_ASSERT_TRUE(counters == savedCounters);


	/* Create Alice again */
	try {
		std::shared_ptr<LimeGeneric> alice = insert_LimeUser(dbFilenameAlice, *aliceDeviceName, x3dh_server_url, curve, prov, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well
		/* wait on this one but we shall get a fail from server */
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to create Lime User");
		return; // no reason to continue the test
	}

	// create another manager with a fresh DB
	std::string dbFilenameAliceTmp{dbFilenameAlice};
	dbFilenameAliceTmp.append(".tmp.sqlite3");

	// create a manager and try to create alice again, it shall pass local creation(db is empty) but server shall reject it
	auto ManagerTmp = std::unique_ptr<LimeManager>(new LimeManager(dbFilenameAliceTmp, prov));
	try {
		ManagerTmp->create_user(*aliceDeviceName, x3dh_server_url, curve, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_failed,counters.operation_failed+1,wait_for_timeout)); // we must get a callback saying all went well
		/* wait on this one but we shall get a fail from server */
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;;
		BC_FAIL("Fail to create Lime User");
		return; // no reason to continue the test
	}

	/* Delete alice again to leave clean base */
	try {
		// delete Alice, wait for callback confirmation from server
		Manager->delete_user(*aliceDeviceName, callback);
		BC_ASSERT_TRUE(wait_for(stack,&counters.operation_success,expected_success++,wait_for_timeout)); // we must get a callback saying all went well
	} catch (BctbxException &e) {
		BCTBX_SLOGE <<e;
		BC_FAIL("Delete Lime user raised exception");
		return;
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
	TEST_NO_TAG("Multiple sessions between pair", x3dh_multiple_DRsessions),
	TEST_NO_TAG("Sending chain limit", x3dh_sending_chain_limit),
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
