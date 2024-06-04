/*
	lime_double_ratchet-tester.cpp
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
#include "lime-tester.hpp"
#include "lime-tester-utils.hpp"
#include "lime_localStorage.hpp"

#include <bctoolbox/tester.h>
#include <bctoolbox/exception.hh>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

#include "bctoolbox/crypto.h"


using namespace::std;
using namespace::lime;

static std::shared_ptr<RNG> RNG_context;

static int start_RNG_before_all(void) {
	RNG_context = make_RNG();
	return 0;
}

static int stop_RNG_after_all(void) {
	return 0;
}

/**
  * @param[in]	period		altern sended each <period> messages (sequence will anyways always start with alice send - bob receive - bob send)
  * @param[in]	skip_period	same than above but for reception skipping: at each begining of skip_period, skip reception of skip_length messages
  * @param[in]	skip_length	see previous: number of messages to be skipped
  * @param[in]	skip_delay	number of messages sending before the skip messages are received
  *				ex: if message 5 is skipped and skip_delay is 10, message 5 will be received after message 15 was sent - and may be received
  *				All delayed messaged are received in their order of sending at the end of message stack processing
  */
template <typename Curve>
static void dr_skippedMessages_basic_test(const uint8_t period=1, const uint8_t skip_period=255, const uint8_t skip_length=0, const uint8_t skip_delay=0, const std::string db_filename="dr_skipped_message_basic_tmp") {
	std::shared_ptr<DR> alice, bob;
	std::shared_ptr<lime::Db> aliceLocalStorage, bobLocalStorage;
	std::string aliceFilename(db_filename);
	std::string bobFilename(db_filename);
	aliceFilename.append(".alice.sqlite3");
	bobFilename.append(".bob.sqlite3");
	std::vector<uint8_t> aliceUserId{'a','l','i','c','e'};
	std::vector<uint8_t> bobUserId{'b','o','b'};

	//clean tmp files
	remove(aliceFilename.data());
	remove(bobFilename.data());

	// create sessions
	auto alice_db_mutex = make_shared<std::recursive_mutex>();
	auto bob_db_mutex = make_shared<std::recursive_mutex>();
	lime_tester::dr_sessionsInit<Curve>(alice, bob, aliceLocalStorage, bobLocalStorage, aliceFilename, alice_db_mutex, bobFilename, bob_db_mutex, true, RNG_context);
	std::vector<std::vector<uint8_t>> cipher;
	std::vector<std::vector<RecipientInfos>> recipients;
	std::vector<uint8_t> messageSender; // hold status of message: 0 not sent, 1 sent by Alice, 2 sent by Bob, 3 received
	std::vector<std::string> plainMessage;

	// resize vectors to hold all materials
	cipher.resize(lime_tester::messages_pattern.size());
	recipients.resize(lime_tester::messages_pattern.size());
	plainMessage.resize(lime_tester::messages_pattern.size());
	messageSender.resize(lime_tester::messages_pattern.size(), 0);

	bool aliceSender=true;
	for (size_t i=0; i<lime_tester::messages_pattern.size(); i++) {
		/* sending */
		if (aliceSender) {
			// alice encrypt a message
			recipients[i].emplace_back("bob",alice);
			std::vector<uint8_t> plaintext{lime_tester::messages_pattern[i].begin(), lime_tester::messages_pattern[i].end()};
			std::vector<uint8_t> cipherMessage{};
			encryptMessage(recipients[i], plaintext, bobUserId, "alice", cipher[i], lime::EncryptionPolicy::optimizeUploadSize, aliceLocalStorage);

			messageSender[i] = 1;
			if (i%period == 0) {
				aliceSender=false;
			}
		} else {
			// bob encrypt a message
			recipients[i].emplace_back("alice",bob);
			std::vector<uint8_t> plaintext{lime_tester::messages_pattern[i].begin(), lime_tester::messages_pattern[i].end()};
			std::vector<uint8_t> cipherMessage{};
			encryptMessage(recipients[i], plaintext, aliceUserId, "bob", cipher[i], lime::EncryptionPolicy::optimizeUploadSize, bobLocalStorage);

			messageSender[i] = 2;
			if (i%period == 0) {
				aliceSender=true;
			}
		}

		/* receiving (or later): immediate reception is skipped for skip_length messages eack skip_period messages */
		if ((i==0) || !(i%skip_period<skip_length)) { // do not skip the first message otherwise bob wont be able to write to alice
			if (messageSender[i]==2) {
				// alice decrypt it
				std::vector<shared_ptr<DR>> recipientDRSessions{};
				recipientDRSessions.push_back(alice);
				std::vector<uint8_t> plainBuffer{};
				decryptMessage("bob", "alice", aliceUserId, recipientDRSessions, recipients[i][0].DRmessage, cipher[i], plainBuffer);
				plainMessage[i] = std::string{plainBuffer.begin(), plainBuffer.end()};
				BC_ASSERT_TRUE(plainMessage[i] == lime_tester::messages_pattern[i]);


				messageSender[i]=3;
			} else if (messageSender[i]==1) {
				// bob decrypt it
				std::vector<shared_ptr<DR>> recipientDRSessions{};
				recipientDRSessions.push_back(bob);
				std::vector<uint8_t> plainBuffer{};
				decryptMessage("alice", "bob", bobUserId, recipientDRSessions, recipients[i][0].DRmessage, cipher[i], plainBuffer);
				plainMessage[i] = std::string{plainBuffer.begin(), plainBuffer.end()};
				BC_ASSERT_TRUE(plainMessage[i] == lime_tester::messages_pattern[i]);

				messageSender[i]=3;
			} else {
				BC_FAIL("That should never happend, something is wrong in the test not the lib");
			}
		}

		/* Do we have some old message to decrypt */
		if (i>=skip_delay) {
			for (size_t j=0; j<i-skip_delay; j++) {
				if (messageSender[j]==2) {
					// alice decrypt it
					std::vector<shared_ptr<DR>> recipientDRSessions{};
					recipientDRSessions.push_back(alice);
					std::vector<uint8_t> plainBuffer{};
					decryptMessage("bob", "alice", aliceUserId, recipientDRSessions, recipients[j][0].DRmessage, cipher[j], plainBuffer);
					plainMessage[j] = std::string{plainBuffer.begin(), plainBuffer.end()};
					BC_ASSERT_TRUE(plainMessage[j] == lime_tester::messages_pattern[j]);

					messageSender[j]=3;
				} else if (messageSender[j]==1) {
					// bob decrypt it
					std::vector<shared_ptr<DR>> recipientDRSessions{};
					recipientDRSessions.push_back(bob);
					std::vector<uint8_t> plainBuffer{};
					decryptMessage("alice", "bob", bobUserId, recipientDRSessions, recipients[j][0].DRmessage, cipher[j], plainBuffer);
					plainMessage[j] = std::string{plainBuffer.begin(), plainBuffer.end()};
					BC_ASSERT_TRUE(plainMessage[j] == lime_tester::messages_pattern[j]);

					messageSender[j]=3;
				}
			}
		}

	}

	/* Do we have some old message to decrypt(ignore delay we're at the end of test */
	for (size_t j=0; j<lime_tester::messages_pattern.size(); j++) {
		if (messageSender[j]==2) {
			// alice decrypt it
			std::vector<shared_ptr<DR>> recipientDRSessions{};
			recipientDRSessions.push_back(alice);
			std::vector<uint8_t> plainBuffer{};
			decryptMessage("bob", "alice", aliceUserId, recipientDRSessions, recipients[j][0].DRmessage, cipher[j], plainBuffer);
			plainMessage[j] = std::string{plainBuffer.begin(), plainBuffer.end()};
			BC_ASSERT_TRUE(plainMessage[j] == lime_tester::messages_pattern[j]);

			messageSender[j]=3;
		} else if (messageSender[j]==1) {
			// bob decrypt it
			std::vector<shared_ptr<DR>> recipientDRSessions{};
			recipientDRSessions.push_back(bob);
			std::vector<uint8_t> plainBuffer{};
			decryptMessage("alice", "bob", bobUserId, recipientDRSessions, recipients[j][0].DRmessage, cipher[j], plainBuffer);
			plainMessage[j] = std::string{plainBuffer.begin(), plainBuffer.end()};
			BC_ASSERT_TRUE(plainMessage[j] == lime_tester::messages_pattern[j]);

			messageSender[j]=3;
		}
	}

	// same same
	for (size_t i=0; i<lime_tester::messages_pattern.size(); i++) {
		BC_ASSERT_TRUE(plainMessage[i] == lime_tester::messages_pattern[i]);
	}

	if (cleanDatabase) {
		remove(aliceFilename.data());
		remove(bobFilename.data());
	}
}

static void dr_skippedMessages_basic(void) {
#ifdef EC25519_ENABLED
	/* send batch of 10 messages, delay by 15 one message each time we reach the end of the batch*/
	dr_skippedMessages_basic_test<C255>(10, 10, 1, 15, "dr_skipMessage_1_X25519");
	/* delayed messages covering more than a batch */
	dr_skippedMessages_basic_test<C255>(3, 7, 4, 17, "dr_skipMessage_2_X25519");
#endif
#ifdef EC448_ENABLED
	dr_skippedMessages_basic_test<C448>(10, 10, 1, 15, "dr_skipMessage_1_X448");
	dr_skippedMessages_basic_test<C448>(5, 5, 1, 10, "dr_skipMessage_2_X448");
#endif
#ifdef HAVE_BCTBXPQ
	dr_skippedMessages_basic_test<C255K512>(10, 10, 1, 15, "dr_skipMessage_1_C255K512");
	dr_skippedMessages_basic_test<C255K512>(5, 5, 1, 10, "dr_skipMessage_2_C255K512");
#endif
}

/* alice send <period> messages to bob, and bob replies with <period> messages and so on until the end of message pattern list  */
template <typename Curve>
static void dr_long_exchange_test(uint8_t period=1, std::string db_filename="dr_long_exchange_tmp") {
	std::shared_ptr<DR> alice, bob;
	std::shared_ptr<lime::Db> aliceLocalStorage, bobLocalStorage;
	std::string aliceFilename(db_filename);
	std::string bobFilename(db_filename);
	aliceFilename.append(".alice.sqlite3");
	bobFilename.append(".bob.sqlite3");
	std::vector<uint8_t> aliceUserId{'a','l','i','c','e'};
	std::vector<uint8_t> bobUserId{'b','o','b'};
	// create sessions
	auto alice_db_mutex = make_shared<std::recursive_mutex>();
	auto bob_db_mutex = make_shared<std::recursive_mutex>();
	lime_tester::dr_sessionsInit<Curve>(alice, bob, aliceLocalStorage, bobLocalStorage, aliceFilename, alice_db_mutex, bobFilename, bob_db_mutex, true, RNG_context);
	std::vector<uint8_t> aliceCipher, bobCipher;

	bool aliceSender=true;

	for (size_t i=0; i<lime_tester::messages_pattern.size(); i++) {
		if (aliceSender) {
			// alice encrypt a message
			std::vector<RecipientInfos> recipients;
			recipients.emplace_back("bob",alice);
			std::vector<uint8_t> plaintext{lime_tester::messages_pattern[i].begin(), lime_tester::messages_pattern[i].end()};
			std::vector<uint8_t> cipherMessage{};
			encryptMessage(recipients, plaintext, bobUserId, "alice", cipherMessage, lime::EncryptionPolicy::optimizeUploadSize, aliceLocalStorage);

			// bob decrypt it
			std::vector<shared_ptr<DR>> recipientDRSessions{};
			recipientDRSessions.push_back(bob);
			std::vector<uint8_t> plainBuffer{};
			decryptMessage("alice", "bob", bobUserId, recipientDRSessions, recipients[0].DRmessage, cipherMessage, plainBuffer);
			std::string plainMessage{plainBuffer.cbegin(), plainBuffer.cend()};

			// same same?
			BC_ASSERT_TRUE(plainMessage==lime_tester::messages_pattern[i]);
			if (i%period == 0) {
				aliceSender=false;
				/* destroy and reload bob sessions */
				auto bobSessionId=bob->dbSessionId();
				bob = nullptr; // release and destroy bob DR context
				bob = make_DR_from_localStorage<Curve>(bobLocalStorage, bobSessionId, RNG_context);
			}
		} else {
			// bob replies
			std::vector<RecipientInfos> recipients;
			recipients.emplace_back("alice",bob);
			std::vector<uint8_t> plaintext{lime_tester::messages_pattern[i].begin(), lime_tester::messages_pattern[i].end()};
			std::vector<uint8_t> cipherMessage{};
			encryptMessage(recipients, plaintext, aliceUserId, "bob", cipherMessage, lime::EncryptionPolicy::optimizeUploadSize, bobLocalStorage);

			// alice decrypt it
			std::vector<shared_ptr<DR>> recipientDRSessions{};
			recipientDRSessions.push_back(alice);
			std::vector<uint8_t> plainBuffer{};
			decryptMessage("bob", "alice", aliceUserId, recipientDRSessions, recipients[0].DRmessage, cipherMessage, plainBuffer);
			std::string plainMessage{plainBuffer.cbegin(), plainBuffer.cend()};

			// same same?
			BC_ASSERT_TRUE(plainMessage==lime_tester::messages_pattern[i]);
			if (i%period == 0) {
				aliceSender=true;
				/* destroy and reload alice sessions */
				auto aliceSessionId=alice->dbSessionId();
				alice = nullptr; // release and destroy alice DR context
				alice = make_DR_from_localStorage<Curve>(aliceLocalStorage, aliceSessionId, RNG_context);
			}
		}
	}

	if (cleanDatabase) {
		// remove temporary db file
		remove(aliceFilename.data());
		remove(bobFilename.data());
	}
}
static void dr_long_exchange1(void) {
#ifdef EC25519_ENABLED
	dr_long_exchange_test<C255>(1, "dr_long_exchange_1_X25519");
#endif
#ifdef EC448_ENABLED
	dr_long_exchange_test<C448>(1, "dr_long_exchange_1_X448");
#endif
#ifdef HAVE_BCTBXPQ
	dr_long_exchange_test<C255K512>(1, "dr_long_exchange_1_C255K512");
#endif
}
static void dr_long_exchange3(void) {
#ifdef EC25519_ENABLED
	dr_long_exchange_test<C255>(3, "dr_long_exchange_3_X25519");
#endif
#ifdef EC448_ENABLED
	dr_long_exchange_test<C448>(3, "dr_long_exchange_3_X448");
#endif
#ifdef HAVE_BCTBXPQ
	dr_long_exchange_test<C255K512>(3, "dr_long_exchange_3_C255K512");
#endif
}
static void dr_long_exchange10(void) {
#ifdef EC25519_ENABLED
	dr_long_exchange_test<C255>(10, "dr_long_exchange_10_X25519");
#endif
#ifdef EC448_ENABLED
	dr_long_exchange_test<C448>(10, "dr_long_exchange_10_X448");
#endif
#ifdef HAVE_BCTBXPQ
	dr_long_exchange_test<C255K512>(10, "dr_long_exchange_10_C255K512");
#endif
}

/* Basic exchange alice send a message to bob and he replies so the session is established
 *
 * @param DRsession 		Alice and Bob double ratchet initialized sessions
 * @param localStorage		Alice and Bob database access pointer
 * @param filename 		ALice and Bob database string(file path) access
 * @param message		Alice and Bob message to be encrypted and sent
 * @param checkEncryptionPolicy if set, perform a check on the encryption policy in emitted DR messages headers
 * @param getEncryptionPolicy	Expected encryptionPolicy for generated DR messages: DRMessage or cipherMessage
 * @param setEncryptionPolicy	Policy requested, DRMessage, cipherMessage, optimizeSize
 */
template <typename Curve>
static void dr_simple_exchange(std::shared_ptr<DR> &DRsessionAlice, std::shared_ptr<DR> &DRsessionBob,
			std::shared_ptr<lime::Db> &localStorageAlice, std::shared_ptr<lime::Db> &localStorageBob,
			std::string &filenameAlice, std::string &filenameBob,
			std::string &messageAlice, std::string &messageBob,
			bool checkEncryptionPolicy,
			lime::EncryptionPolicy getEncryptionPolicyAlice, lime::EncryptionPolicy getEncryptionPolicyBob,
			lime::EncryptionPolicy setEncryptionPolicyAlice, lime::EncryptionPolicy setEncryptionPolicyBob) {
	// create sessions: alice sender, bob receiver
	auto alice_db_mutex = make_shared<std::recursive_mutex>();
	auto bob_db_mutex = make_shared<std::recursive_mutex>();
	lime_tester::dr_sessionsInit<Curve>(DRsessionAlice, DRsessionBob, localStorageAlice, localStorageBob, filenameAlice, alice_db_mutex, filenameBob, bob_db_mutex, true, RNG_context);
	std::vector<uint8_t> aliceCipher, bobCipher;
	std::vector<uint8_t> aliceUserId{'a','l','i','c','e'};
	std::vector<uint8_t> bobUserId{'b','o','b'};

	// alice encrypt a message
	std::vector<RecipientInfos> recipients;
	recipients.emplace_back("bob",DRsessionAlice);
	std::vector<uint8_t> plaintextAlice{messageAlice.begin(), messageAlice.end()};
	encryptMessage(recipients, plaintextAlice, bobUserId, "alice", aliceCipher, setEncryptionPolicyAlice, localStorageAlice);

	if (checkEncryptionPolicy) {
		bool is_directEncryptionType = lime_tester::DR_message_payloadDirectEncrypt(recipients[0].DRmessage);
		if (getEncryptionPolicyAlice == lime::EncryptionPolicy::DRMessage) {
			BC_ASSERT_TRUE(is_directEncryptionType);
			BC_ASSERT_EQUAL((int)aliceCipher.size(), 0, int, "%d"); // in direct Encryption mode, cipherMessage is empty
		} else {
			BC_ASSERT_FALSE(is_directEncryptionType);
		}
	}

	// bob decrypt it
	std::vector<shared_ptr<DR>> recipientDRSessions{};
	recipientDRSessions.push_back(DRsessionBob);
	std::vector<uint8_t> plainBuffer{};
	auto ret = decryptMessage("alice", "bob", bobUserId, recipientDRSessions, recipients[0].DRmessage, aliceCipher, plainBuffer);
	BC_ASSERT_TRUE(ret == DRsessionBob);
	if (ret != DRsessionBob) return;
	std::string plainMessageBob{plainBuffer.begin(), plainBuffer.end()};

	// same same?
	BC_ASSERT_TRUE(plainMessageBob==messageAlice);

	// bob replies
	recipients.clear();
	recipients.emplace_back("alice",DRsessionBob);
	std::vector<uint8_t> plaintextBob{messageBob.begin(), messageBob.end()};
	encryptMessage(recipients, plaintextBob, aliceUserId, "bob", bobCipher, setEncryptionPolicyBob, localStorageBob);

	if (checkEncryptionPolicy) { // we must check Bob encryption type
		bool is_directEncryptionType = lime_tester::DR_message_payloadDirectEncrypt(recipients[0].DRmessage);
		if (getEncryptionPolicyBob == lime::EncryptionPolicy::DRMessage) {
			BC_ASSERT_TRUE(is_directEncryptionType);
			BC_ASSERT_EQUAL((int)bobCipher.size(), 0, int, "%d"); // in direct Encryption mode, cipherMessage is empty
		} else {
			BC_ASSERT_FALSE(is_directEncryptionType);
			BC_ASSERT_NOT_EQUAL((int)bobCipher.size(), 0, int, "%d"); // in cipher message Encryption mode, cipherMessage is not empty
		}
	}

	// alice decrypt it
	recipientDRSessions.clear();
	recipientDRSessions.push_back(DRsessionAlice);
	plainBuffer.clear();
	BC_ASSERT_TRUE(decryptMessage("bob", "alice", aliceUserId, recipientDRSessions, recipients[0].DRmessage, bobCipher, plainBuffer) == DRsessionAlice);
	std::string plainMessageAlice{plainBuffer.begin(), plainBuffer.end()};

	// same same?
	BC_ASSERT_TRUE(plainMessageAlice==messageBob);

}

template <typename Curve>
static void dr_simple_exchange(std::shared_ptr<DR> &DRsessionAlice, std::shared_ptr<DR> &DRsessionBob,
			std::shared_ptr<lime::Db> &localStorageAlice, std::shared_ptr<lime::Db> &localStorageBob,
			std::string &filenameAlice, std::string &filenameBob) {

	dr_simple_exchange<Curve>(DRsessionAlice, DRsessionBob,
			localStorageAlice, localStorageBob,
			filenameAlice, filenameBob,
			lime_tester::messages_pattern[0], lime_tester::messages_pattern[1], // default: use messages_pattern 0 and 1
			false, // do not check
			lime::EncryptionPolicy::DRMessage, lime::EncryptionPolicy::DRMessage, //useless as we are not checking anything
			lime::EncryptionPolicy::optimizeUploadSize, lime::EncryptionPolicy::optimizeUploadSize); // set to default
}

/* alice send a message to bob, and he replies */
template <typename Curve>
static void dr_basic_test(std::string db_filename) {
	std::shared_ptr<DR> alice, bob;
	std::shared_ptr<lime::Db> localStorageAlice, localStorageBob;
	std::string aliceFilename(db_filename);
	std::string bobFilename(db_filename);
	aliceFilename.append(".alice.sqlite3");
	bobFilename.append(".bob.sqlite3");

	// remove temporary db file if they are here
	remove(aliceFilename.data());
	remove(bobFilename.data());

	dr_simple_exchange<Curve>(alice, bob, localStorageAlice, localStorageBob, aliceFilename, bobFilename);

	if (cleanDatabase) {
		remove(aliceFilename.data());
		remove(bobFilename.data());
	}
}

static void dr_basic(void) {
#ifdef EC25519_ENABLED
	dr_basic_test<C255>("dr_basic_X25519");
#endif
#ifdef EC448_ENABLED
	dr_basic_test<C448>("dr_basic_X448");
#endif
#ifdef HAVE_BCTBXPQ
	dr_basic_test<C255K512>("dr_basic_C255K512");
#endif
}
/* Bob decrypt patterns: Bob DB holds in DR_session id 1 a session able to decrypt the pattern message giving messages_pattern[5] */
template <typename Curve>
static void dr_pattern_test(std::string db_filename, std::vector<uint8_t> &DRmessage, std::vector<uint8_t> &cipherText) {
	std::shared_ptr<DR> bob;
	std::shared_ptr<lime::Db> localStorageBob;
	std::string bobFilename(db_filename);
	bobFilename.append(".bob.pattern.sqlite3");
	auto bob_db_mutex = make_shared<std::recursive_mutex>();
	std::vector<uint8_t> bobUserId{'b','o','b'};

	remove(bobFilename.data());
	std::string resourceFilename("data/");
	resourceFilename.append(bobFilename);
	// Copy the DB from resource file as we do not want to modify it
	char *pattern_filepath = bc_tester_res(resourceFilename.c_str());
	{
		#include <fstream>
		std::ifstream  src(pattern_filepath, std::ios::binary);
		std::ofstream  dst(bobFilename,   std::ios::binary);
		dst << src.rdbuf();
	}
	// This should work but for some reason fail to build on Debian10/Clang7
	//std::filesystem::copy(pattern_filepath, bobFilename);
	bctbx_free(pattern_filepath);

	// Load the DR session with id 1 from the DB
	localStorageBob = std::make_shared<lime::Db>(bobFilename, bob_db_mutex);
	bob = make_DR_from_localStorage<Curve>(localStorageBob, 1, RNG_context);
	BC_ASSERT_TRUE(bob->isActive());
	if (!bob->isActive()) return; // skip the test if we were not able to load bob's session from pattern

	// Bob decrypts the message
	std::vector<shared_ptr<DR>> recipientDRSessions{};
	recipientDRSessions.push_back(bob);
	std::vector<uint8_t> plainBuffer{};
	// decryptMessage returns the session used to successfully decrypt it: it should be the only one given to it
	BC_ASSERT_TRUE(decryptMessage("alice", "bob", bobUserId, recipientDRSessions, DRmessage, cipherText, plainBuffer)==bob);
	std::string plainMessageBob{plainBuffer.begin(), plainBuffer.end()};
	BC_ASSERT_TRUE(plainMessageBob==lime_tester::messages_pattern[5]);

	//auto bob2 = std::make_shared<DR<C255K512>>(localStorageBob, 1, RNG_context);
	if (cleanDatabase) {
		remove(bobFilename.data());
	}
}

static void dr_pattern(void) {
#ifdef EC25519_ENABLED
	std::vector<uint8_t> DRmessage255 = {0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0xf3, 0x87, 0xec, 0x2f, 0xc0, 0x17, 0xdd, 0x0b, 0xce, 0x9d, 0xb6, 0xff, 0xd1, 0x63, 0x16, 0xb3, 0xec, 0xb1, 0x1a, 0x71, 0x56, 0x65, 0x78, 0x59, 0x6b, 0xee, 0xcf, 0x89, 0x2e, 0xc7, 0xab, 0x35, 0xa9, 0x2f, 0x4d, 0xbb, 0xc6, 0xb8, 0x71, 0x39, 0xf3, 0x87, 0x48, 0x7b, 0xdc, 0xdd, 0xcd, 0xbb, 0xea, 0x2d, 0xdc, 0x18, 0x0c, 0xf8, 0x49, 0xc5, 0xbc, 0xb2, 0xa9, 0x62, 0x6d, 0xd9, 0xe3, 0x89, 0x97, 0xe8, 0x93, 0xa4, 0x2f, 0x9e, 0xcb, 0x13, 0x4f, 0x3b, 0xf3, 0xbc, 0x0b, 0xf6, 0x07, 0xf8};
	std::vector<uint8_t> cipherText255 = {0x0f, 0x09, 0xcf, 0x06, 0xc0, 0xe0, 0xa6, 0xc4, 0xb1, 0x90, 0xb5, 0xd4, 0x5d, 0xe8, 0xf9, 0x27, 0xbe, 0x8d, 0x04, 0x08, 0x2c, 0x9b, 0x05, 0xd2, 0xf6, 0x44, 0xa4, 0x5b, 0x42, 0xfe, 0x89, 0x9c, 0x07, 0x70, 0x55, 0x7b, 0xe6, 0x1f};
	dr_pattern_test<C255>("dr_basic_X25519", DRmessage255, cipherText255);
#endif
#ifdef EC448_ENABLED
	std::vector<uint8_t> DRmessage448 = {0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x4e, 0xa0, 0xf4, 0x9f, 0x43, 0x73, 0xb7, 0xda, 0x02, 0x26, 0x1c, 0x49, 0xd5, 0x43, 0x8a, 0xb7, 0xe5, 0x9b, 0xc8, 0xe4, 0x0b, 0xb1, 0xf1, 0x57, 0x50, 0x7d, 0xb8, 0xf6, 0xa4, 0xd8, 0x6d, 0xca, 0x05, 0x71, 0x1d, 0x37, 0x09, 0xd4, 0xb3, 0x5c, 0x51, 0x5d, 0x51, 0x50, 0x9b, 0x41, 0xe9, 0xf8, 0x0b, 0x59, 0x5f, 0x87, 0xd8, 0x61, 0xff, 0x63, 0xe7, 0x19, 0xfa, 0x46, 0x0e, 0x6b, 0x20, 0x61, 0x12, 0x74, 0xac, 0xd9, 0xd2, 0xb2, 0x86, 0x3e, 0x1f, 0x0a, 0x1f, 0xf6, 0x11, 0xe5, 0x1c, 0x9c, 0x8b, 0x86, 0x25, 0x02, 0x41, 0xc6, 0xe7, 0xde, 0x03, 0x13, 0x90, 0xe0, 0x46, 0xfa, 0x6a, 0xaa, 0x63, 0x25, 0xb3, 0x7f, 0x25, 0xa6, 0x21, 0x77};
	std::vector<uint8_t> cipherText448 = {0xa9, 0xa5, 0x85, 0x4a, 0xec, 0xdd, 0x44, 0x50, 0xcb, 0xbe, 0x2a, 0x0a, 0xdf, 0x47, 0x8d, 0x9b, 0x0f, 0x8c, 0xa8, 0x47, 0x82, 0x54, 0x83, 0x7f, 0xbd, 0xf7, 0x0f, 0xd5, 0x6c, 0x05, 0xc0, 0xe6, 0x1a, 0x92, 0xf2, 0xac, 0x09, 0x0b};
	dr_pattern_test<C448>("dr_basic_X448", DRmessage448, cipherText448);
#endif
}

/* alice send a message to bob, and he replies. Both users have 3 devices */
template <typename Curve>
static void dr_multidevice_exchange(std::string db_filename,
		std::string &message,
		bool checkEncryptionPolicy,
		lime::EncryptionPolicy getEncryptionPolicy, lime::EncryptionPolicy setEncryptionPolicy) {
	/* we have 2 users "alice" and "bob" with 3 devices each */
	std::vector<std::string> usernames{"alice", "bob"};
	std::vector<uint8_t> bobUserId{'b','o','b'};
	std::vector<std::vector<std::vector<std::vector<lime_tester::sessionDetails<Curve>>>>> users;

	/* give correct size to our users vector for users and devices count */
	users.resize(usernames.size());
	for (auto &user : users) user.resize(3);

	/* init and instanciate, session will be then found in a 4 dimensional vector indexed this way : [self user id][self device id][peer user id][peer device id] */
	std::vector<std::string> created_db_files{};
	lime_tester::dr_devicesInit(db_filename, users, usernames, created_db_files, RNG_context);

	/* Send a message from alice.dev0 to all bob device(and copy to alice devices too) */
	std::vector<RecipientInfos> recipients;
	for (size_t u=0; u<users.size(); u++) { // loop users
		for (size_t d=0; d<users[u].size(); d++) { // devices
			if (u!=0 || d!=0) { // sender is users 0, device 0, do not encode for him
				std::string devId{users[0][0][u][d].peer_userId}; // source is 0,0 dest is u,d
				devId.append("@").append(std::to_string(users[0][0][u][d].peer_deviceIndex)); //deviceId is peerUserId@peerDeviceId
				recipients.emplace_back(devId, users[0][0][u][d].DRSession);
			}
		}
	}

	std::string sourceId = usernames[0];
	sourceId.append("@").append(to_string(0)); // source deviceId shall be alice@0
	std::vector<std::vector<uint8_t>> cipherHeader;
	std::vector<uint8_t> cipherMessage;
	std::vector<uint8_t> plaintext{message.begin(), message.end()};

	encryptMessage(recipients, plaintext, bobUserId, sourceId, cipherMessage, setEncryptionPolicy, users[0][0][1][0].localStorage);

	if (checkEncryptionPolicy) { // we must check encryption type
		bool is_directEncryptionType = lime_tester::DR_message_payloadDirectEncrypt(recipients[0].DRmessage); // check on recipients[0], they shall all be the same
		if (getEncryptionPolicy == lime::EncryptionPolicy::DRMessage) {
			BC_ASSERT_TRUE(is_directEncryptionType);
			BC_ASSERT_EQUAL((int)cipherMessage.size(), 0, int, "%d"); // in direct Encryption mode, cipherMessage is empty
		} else {
			BC_ASSERT_FALSE(is_directEncryptionType);
		}
	}

	size_t recipientsIndex=0;
	// Now try decrypt the messages received on every device
	// pop the headers from front in recipients so loop on the exact same order as when building it
	for (size_t u=0; u<users.size(); u++) { // loop users
		for (size_t d=0; d<users[u].size(); d++) { // devices
			if (u!=0 || d!=0) { // sender is users 0, device 0, do not decode with it
				RecipientInfos recipient = recipients[recipientsIndex];
				// store our DRSession in a vector as interface request a vector of DRSession to try them all, we may have several sessions with one peer device
				std::vector<shared_ptr<DR>> recipientDRSessions{};
				recipientDRSessions.push_back(users[u][d][0][0].DRSession); // we are u,d receiving from 0,0

				std::vector<uint8_t> plaintext_back;
				BC_ASSERT_TRUE(decryptMessage(sourceId, recipient.deviceId, bobUserId, recipientDRSessions, recipient.DRmessage, cipherMessage, plaintext_back) != nullptr); // recipient id is username 1

				// convert back the output vector to a string
				std::string plaintext_back_string{plaintext_back.begin(), plaintext_back.end()};

				BC_ASSERT_TRUE(plaintext_back_string==message);

				recipientsIndex++;
			}
		}
	}

	if (cleanDatabase) {
		for (auto &filename : created_db_files) {
			remove(filename.data());
		}
	}
}

template <typename Curve>
static void dr_multidevice_basic_test(std::string db_filename) {
	dr_multidevice_exchange<Curve>(db_filename,
			lime_tester::messages_pattern[0],
			false, // do not check
			lime::EncryptionPolicy::optimizeUploadSize, lime::EncryptionPolicy::optimizeUploadSize); // default setting
}
static void dr_multidevice_basic(void) {
#ifdef EC25519_ENABLED
	dr_multidevice_basic_test<C255>("dr_multidevice_basic_C25519");
#endif
#ifdef EC448_ENABLED
	dr_multidevice_basic_test<C448>("dr_multidevice_basic_C448");
#endif
#ifdef HAVE_BCTBXPQ
	dr_multidevice_basic_test<C255K512>("dr_multidevice_basic_C255K512");
#endif
}


/* After session is established, more than limit messages are skipped */
template <typename Curve>
static void dr_skip_too_much_test(std::string db_filename) {
	std::shared_ptr<DR> alice, bob;
	std::shared_ptr<lime::Db> localStorageAlice, localStorageBob;
	std::string aliceFilename(db_filename);
	std::string bobFilename(db_filename);
	aliceFilename.append(".alice.sqlite3");
	bobFilename.append(".bob.sqlite3");
	std::vector<uint8_t> aliceUserId{'a','l','i','c','e'};
	std::vector<uint8_t> bobUserId{'b','o','b'};

	// remove temporary db file if they are here
	remove(aliceFilename.data());
	remove(bobFilename.data());

	// fully establish session
	dr_simple_exchange<Curve>(alice, bob, localStorageAlice, localStorageBob, aliceFilename, bobFilename);

	// encrypt maxMessageSkip+2 messages
	std::vector<uint8_t> aliceCipher{};
	std::vector<RecipientInfos> recipients;
	recipients.emplace_back("bob",alice);
	std::vector<uint8_t> plaintextAlice{lime_tester::messages_pattern[1].begin(), lime_tester::messages_pattern[1].end()};
	for (auto i=0; i<lime::settings::maxMessageSkip+2; i++) { // we can skip maxMessageSkip, so encrypt +2 and we will skip +1
		// alice encrypt a message, just discard it, it's not the point to decrypt it
		encryptMessage(recipients, plaintextAlice, bobUserId, "alice", aliceCipher, lime::EncryptionPolicy::optimizeUploadSize, localStorageAlice);
	}

	// now decrypt the last encrypted message, it shall fail: too much skiped messages
	std::vector<shared_ptr<DR>> recipientDRSessions{};
	recipientDRSessions.push_back(bob);
	std::vector<uint8_t> plainBuffer{};
	BC_ASSERT_TRUE (decryptMessage("alice", "bob", bobUserId, recipientDRSessions, recipients[0].DRmessage, aliceCipher, plainBuffer) == nullptr); // decrypt must fail without throwing any exception


	// Now same thing but with a DH ratchet in the middle so we change chain
	// remove temporary db file if they are here
	remove(aliceFilename.data());
	remove(bobFilename.data());

	// fully establish session
	dr_simple_exchange<Curve>(alice, bob, localStorageAlice, localStorageBob, aliceFilename, bobFilename);

	// alice encrypt 1 message, bob decipher it. Alice uses sending chain n, receiving chain n
	aliceCipher.clear();
	recipients.clear();
	recipients.emplace_back("bob",alice);
	plaintextAlice.assign(lime_tester::messages_pattern[1].begin(), lime_tester::messages_pattern[1].end());
	encryptMessage(recipients, plaintextAlice, bobUserId, "alice", aliceCipher, lime::EncryptionPolicy::optimizeUploadSize, localStorageAlice);

	// bob decrypt it - Bob perform a DH Ratchet and than have receiving chain n, sending chain n+1
	recipientDRSessions.clear();
	recipientDRSessions.push_back(bob);
	plainBuffer.clear();
	decryptMessage("alice", "bob", bobUserId, recipientDRSessions, recipients[0].DRmessage, aliceCipher, plainBuffer);
	std::string plainMessageBob{plainBuffer.begin(), plainBuffer.end()};

	// same same?
	BC_ASSERT_TRUE(plainMessageBob==lime_tester::messages_pattern[1]);

	// bob replies : receiving chain n, sending chain n+1
	std::vector<uint8_t> bobCipher{};
	recipients.clear();
	recipients.emplace_back("alice",bob);
	std::vector<uint8_t> plaintextBob{lime_tester::messages_pattern[2].begin(), lime_tester::messages_pattern[2].end()};
	encryptMessage(recipients, plaintextBob, aliceUserId, "bob", bobCipher, lime::EncryptionPolicy::optimizeUploadSize, localStorageBob);

	// alice did not get bob reply, and encrypt maxMessageSkip/2 messages, with sending chain n (receiving chain is still n too))
	aliceCipher.clear();
	std::vector<RecipientInfos> lostRecipients;
	lostRecipients.emplace_back("bob",alice);
	plaintextAlice.assign(lime_tester::messages_pattern[2].begin(), lime_tester::messages_pattern[2].end());
	for (auto i=0; i<lime::settings::maxMessageSkip/2; i++) {
		// alice encrypt a message, just discard it, it's not the point to decrypt it
		encryptMessage(lostRecipients, plaintextAlice, bobUserId, "alice", aliceCipher, lime::EncryptionPolicy::optimizeUploadSize, localStorageAlice);
	}

	// alice now decrypt bob's message performing a DH ratchet, after that she has sending chain n+1, receiving chain n+1
	recipientDRSessions.clear();
	recipientDRSessions.push_back(alice);
	plainBuffer.clear();
	decryptMessage("bob", "alice", aliceUserId, recipientDRSessions, recipients[0].DRmessage, bobCipher, plainBuffer);
	std::string plainMessageAlice{plainBuffer.begin(), plainBuffer.end()};

	// same same?
	BC_ASSERT_TRUE(plainMessageAlice==lime_tester::messages_pattern[2]);

	// alice then encrypt some maxMessageSkip/2 + 3(in case maxMessageSkip was odd number), with sending chain n+1
	aliceCipher.clear();
	lostRecipients.clear();
	lostRecipients.emplace_back("bob",alice);
	plaintextAlice.assign(lime_tester::messages_pattern[2].begin(), lime_tester::messages_pattern[2].end());
	for (auto i=0; i<lime::settings::maxMessageSkip/2+3; i++) {
		// alice encrypt a message, just discard it, it's not the point to decrypt it
		encryptMessage(lostRecipients, plaintextAlice, bobUserId, "alice", aliceCipher, lime::EncryptionPolicy::optimizeUploadSize, localStorageAlice);
	}

	// now decrypt the last encrypted message, it shall fail: bob is on receiving chain n and missed maxMessageSkip/2 on it  + maxMessageSkip/2+3 in receiving chain n+1
	recipientDRSessions.clear();
	recipientDRSessions.push_back(bob);
	plainBuffer.clear();
	BC_ASSERT_TRUE (decryptMessage("alice", "bob", bobUserId, recipientDRSessions, lostRecipients[0].DRmessage, aliceCipher, plainBuffer) == nullptr); // decrypt must fail without throwing any exception

	if (cleanDatabase) {
		remove(aliceFilename.data());
		remove(bobFilename.data());
	}
}

static void dr_skip_too_much(void) {
#ifdef EC25519_ENABLED
	dr_skip_too_much_test<C255>("dr_skip_too_much_C25519");
#endif
#ifdef EC448_ENABLED
	dr_skip_too_much_test<C448>("dr_skip_too_much_C448");
#endif
#ifdef HAVE_BCTBXPQ
	dr_skip_too_much_test<C255K512>("dr_skip_too_much_C255K512");
#endif
}

/* alice send a message to bob, and he replies */
template <typename Curve>
static void dr_encryptionPolicy_basic_test(std::string db_filename) {
	std::shared_ptr<DR> alice, bob;
	std::shared_ptr<lime::Db> localStorageAlice, localStorageBob;
	std::string aliceFilename(db_filename);
	std::string bobFilename(db_filename);
	aliceFilename.append(".alice.sqlite3");
	bobFilename.append(".bob.sqlite3");

	// remove temporary db file if they are here
	remove(aliceFilename.data());
	remove(bobFilename.data());

	/* short message, force optimizeSize policy -> direct encryption(we have only one recipient)*/
	dr_simple_exchange<Curve>(alice, bob, localStorageAlice, localStorageBob, aliceFilename, bobFilename, lime_tester::shortMessage, lime_tester::shortMessage,
			true,
			lime::EncryptionPolicy::DRMessage, lime::EncryptionPolicy::DRMessage, // we expect direct message encryption when we have only one recipient
			lime::EncryptionPolicy::optimizeUploadSize, lime::EncryptionPolicy::optimizeUploadSize); // and force optimizeSize

	/* long message, force optimizeSize policy -> direct encryption(we have only one recipient)*/
	dr_simple_exchange<Curve>(alice, bob, localStorageAlice, localStorageBob, aliceFilename, bobFilename, lime_tester::longMessage, lime_tester::longMessage,
			true,
			lime::EncryptionPolicy::DRMessage, lime::EncryptionPolicy::DRMessage, // we expect direct message encryption as we have only one recipient even if the message is long
			lime::EncryptionPolicy::optimizeUploadSize, lime::EncryptionPolicy::optimizeUploadSize); // and force optimizeSize

	/* short message, force cipher Message policy -> cipher Message encryption */
	dr_simple_exchange<Curve>(alice, bob, localStorageAlice, localStorageBob, aliceFilename, bobFilename, lime_tester::shortMessage, lime_tester::shortMessage,
			true,
			lime::EncryptionPolicy::cipherMessage, lime::EncryptionPolicy::cipherMessage, // we expect cipher message encryption
			lime::EncryptionPolicy::cipherMessage, lime::EncryptionPolicy::cipherMessage); // when we force it

	/* long message, force cipher Message policy -> cipher Message encryption */
	dr_simple_exchange<Curve>(alice, bob, localStorageAlice, localStorageBob, aliceFilename, bobFilename, lime_tester::longMessage, lime_tester::longMessage,
			true,
			lime::EncryptionPolicy::cipherMessage, lime::EncryptionPolicy::cipherMessage, // we expect cipher message encryption
			lime::EncryptionPolicy::cipherMessage, lime::EncryptionPolicy::cipherMessage); // when we force it

	/* short message, force DRMessage policy -> DRMessage encryption */
	dr_simple_exchange<Curve>(alice, bob, localStorageAlice, localStorageBob, aliceFilename, bobFilename, lime_tester::shortMessage, lime_tester::shortMessage,
			true,
			lime::EncryptionPolicy::DRMessage, lime::EncryptionPolicy::DRMessage, // we expect DR message encryption
			lime::EncryptionPolicy::DRMessage, lime::EncryptionPolicy::DRMessage); // when we force it

	/* long message, force DRMessage policy -> DRMessage encryption */
	dr_simple_exchange<Curve>(alice, bob, localStorageAlice, localStorageBob, aliceFilename, bobFilename, lime_tester::longMessage, lime_tester::longMessage,
			true,
			lime::EncryptionPolicy::DRMessage, lime::EncryptionPolicy::DRMessage, // we expect DR message encryption
			lime::EncryptionPolicy::DRMessage, lime::EncryptionPolicy::DRMessage); // when we force it

	if (cleanDatabase) {
		remove(aliceFilename.data());
		remove(bobFilename.data());
	}
}

static void dr_encryptionPolicy_basic(void) {
#ifdef EC25519_ENABLED
	dr_encryptionPolicy_basic_test<C255>("dr_encryptionPolicy_X25519");
#endif
#ifdef EC448_ENABLED
	dr_encryptionPolicy_basic_test<C448>("dr_encryptionPolicy_X448");
#endif
#ifdef HAVE_BCTBXPQ
	dr_encryptionPolicy_basic_test<C255K512>("dr_encryptionPolicy_C255K512");
#endif
}

template <typename Curve>
static void dr_encryptionPolicy_multidevice_test(std::string db_filename) {

	/* short message, forced optimizeSize policy(which shall be the default anyway) -> direct encryption(even if we have more thant one recipient) */
	dr_multidevice_exchange<Curve>(db_filename,
			lime_tester::shortMessage,
			true,
			lime::EncryptionPolicy::DRMessage, // check we have direct encryotion
			lime::EncryptionPolicy::optimizeUploadSize); // do nothing about payload encryption policy: use default

	/* long message, forced optimizeSize policy(which shall be the default anyway) -> cipher message  encryption(we have more than one recipient) */
	dr_multidevice_exchange<Curve>(db_filename,
			lime_tester::longMessage,
			true,
			lime::EncryptionPolicy::cipherMessage,// check we have cipher message encryption
			lime::EncryptionPolicy::optimizeUploadSize); // do nothing about payload encryption policy: use default

	/* short message, forced DRMessage policy -> direct encryption(even if we have more thant one recipient) */
	dr_multidevice_exchange<Curve>(db_filename,
			lime_tester::shortMessage,
			true,
			lime::EncryptionPolicy::DRMessage, // check we have direct encryotion
			lime::EncryptionPolicy::DRMessage); // do nothing about payload encryption policy: use default

	/* long message, forced DRMessage -> direct encryption encryption(we have more thant one recipient) */
	dr_multidevice_exchange<Curve>(db_filename,
			lime_tester::longMessage,
			true,
			lime::EncryptionPolicy::DRMessage,// check we have cipher message encryption
			lime::EncryptionPolicy::DRMessage); // do nothing about payload encryption policy: use default

	/* short message, forced cipherMessage policy -> cipherMessage encryption(even if we have small messages) */
	dr_multidevice_exchange<Curve>(db_filename,
			lime_tester::shortMessage,
			true,
			lime::EncryptionPolicy::cipherMessage, // check we have direct encryotion
			lime::EncryptionPolicy::cipherMessage); // do nothing about payload encryption policy: use default

	/* long message, forced cipherMessage -> cipherMessage encryption encryption */
	dr_multidevice_exchange<Curve>(db_filename,
			lime_tester::longMessage,
			true,
			lime::EncryptionPolicy::cipherMessage,// check we have cipher message encryption
			lime::EncryptionPolicy::cipherMessage); // do nothing about payload encryption policy: use default
}

static void dr_encryptionPolicy_multidevice(void) {
#ifdef EC25519_ENABLED
	dr_encryptionPolicy_multidevice_test<C255>("dr_encryptionPolicy_multidevice_C25519");
#endif
#ifdef EC448_ENABLED
	dr_encryptionPolicy_multidevice_test<C448>("dr_encryptionPolicy_multidevice_C448");
#endif
#ifdef HAVE_BCTBXPQ
	dr_encryptionPolicy_multidevice_test<C255K512>("dr_encryptionPolicy_multidevice_C255K512");
#endif
}
/* Alice send a encrypt a message to Bob, with forced encryption policy but the cipher message is deleted
 * expect an exeption
 *
 * @param DRsession 		Alice and Bob double ratchet initialized sessions
 * @param localStorage		Alice and Bob database access pointer
 * @param filename 		ALice and Bob database string(file path) access
 * @param message		Alice and Bob message to be encrypted and sent
 * @param setEncryptionPolicy	DRMessage or cipherMessage
 */
template <typename Curve>
static void dr_encryptionPolicy_error_test(std::string db_filename, lime::EncryptionPolicy encryptionPolicy) {
	std::shared_ptr<DR> DRsessionAlice, DRsessionBob;
	std::shared_ptr<lime::Db> localStorageAlice, localStorageBob;
	std::string aliceFilename(db_filename);
	std::string bobFilename(db_filename);
	aliceFilename.append(".alice.sqlite3");
	bobFilename.append(".bob.sqlite3");
	std::vector<uint8_t> aliceUserId{'a','l','i','c','e'};
	std::vector<uint8_t> bobUserId{'b','o','b'};

	// create sessions: alice sender, bob receiver
	auto alice_db_mutex = make_shared<std::recursive_mutex>();
	auto bob_db_mutex = make_shared<std::recursive_mutex>();
	lime_tester::dr_sessionsInit<Curve>(DRsessionAlice, DRsessionBob, localStorageAlice, localStorageBob, aliceFilename, alice_db_mutex, bobFilename, bob_db_mutex, true, RNG_context);
	std::vector<uint8_t> aliceCipher, bobCipher;

	// alice encrypt a message
	std::vector<RecipientInfos> recipients;
	recipients.emplace_back("bob",DRsessionAlice);
	std::vector<uint8_t> plaintextAlice{lime_tester::messages_pattern[0].begin(), lime_tester::messages_pattern[0].end()};
	encryptMessage(recipients, plaintextAlice, bobUserId, "alice", aliceCipher, encryptionPolicy, localStorageAlice);

	if (encryptionPolicy == lime::EncryptionPolicy::cipherMessage) { // we shall have a cipherMessage, delete it
		BC_ASSERT_TRUE(aliceCipher.size() > 0);
		aliceCipher.clear();
	}

	if (encryptionPolicy == lime::EncryptionPolicy::DRMessage) { // we shall not have a cipherMessage, create one
		BC_ASSERT_TRUE(aliceCipher.size() == 0);
		aliceCipher.resize(32, 0xaa); // just create a 0xaa filled buffer, its presence shall prevent the perferctly correct message to be decrypted
	}

	// bob decrypt it
	std::vector<shared_ptr<DR>> recipientDRSessions{};
	recipientDRSessions.push_back(DRsessionBob);
	std::vector<uint8_t> plainBuffer{};
	// the decryption shall fail because
	// - in cipher Message mode we deleted the cipherMessage(we will not just decipher the random key and pass it as output)
	// - in DRMessage mode we created a dummy cipher Message and its presence will cogflict with flag in the message header leading to cancelling the decryption
	BC_ASSERT_TRUE(decryptMessage("alice", "bob", bobUserId, recipientDRSessions, recipients[0].DRmessage, aliceCipher, plainBuffer) == nullptr);

	if (cleanDatabase) {
		remove(aliceFilename.data());
		remove(bobFilename.data());
	}
}

static void dr_encryptionPolicy_error(void) {
#ifdef EC25519_ENABLED
	dr_encryptionPolicy_error_test<C255>("dr_encryptionPolicy_error_C25519", lime::EncryptionPolicy::cipherMessage);
	dr_encryptionPolicy_error_test<C255>("dr_encryptionPolicy_error_C25519", lime::EncryptionPolicy::DRMessage);
#endif
#ifdef EC448_ENABLED
	dr_encryptionPolicy_error_test<C448>("dr_encryptionPolicy_error_C448", lime::EncryptionPolicy::cipherMessage);
	dr_encryptionPolicy_error_test<C448>("dr_encryptionPolicy_error_C448", lime::EncryptionPolicy::DRMessage);
#endif
#ifdef HAVE_BCTBXPQ
	dr_encryptionPolicy_error_test<C255K512>("dr_encryptionPolicy_error_C255K512", lime::EncryptionPolicy::cipherMessage);
	dr_encryptionPolicy_error_test<C255K512>("dr_encryptionPolicy_error_C255K512", lime::EncryptionPolicy::DRMessage);
#endif
}

static test_t tests[] = {
	TEST_NO_TAG("Basic", dr_basic),
	TEST_NO_TAG("Pattern", dr_pattern),
	TEST_NO_TAG("Long Exchange 1", dr_long_exchange1),
	TEST_NO_TAG("Long Exchange 3", dr_long_exchange3),
	TEST_NO_TAG("Long Exchange 10", dr_long_exchange10),
	TEST_NO_TAG("Skip message", dr_skippedMessages_basic),
	TEST_NO_TAG("Multidevices", dr_multidevice_basic),
	TEST_NO_TAG("Skip more messages than limit", dr_skip_too_much),
	TEST_NO_TAG("Encryption Policy basic", dr_encryptionPolicy_basic),
	TEST_NO_TAG("Encryption Policy multidevice", dr_encryptionPolicy_multidevice),
	TEST_NO_TAG("Wrong Encryption Policy", dr_encryptionPolicy_error),
};

test_suite_t lime_double_ratchet_test_suite = {
	"Double Ratchet",
	start_RNG_before_all,
	stop_RNG_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests,
	0,
	0
};
