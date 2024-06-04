/*
	lime-tester-utils.cpp
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
#include <vector>
#include <string>
#include <mutex>
#include "lime_settings.hpp"
#include "lime/lime.hpp"
#include "lime_keys.hpp"
#include "lime_x3dh.hpp"
#include "lime_crypto_primitives.hpp"
#include "lime_double_ratchet_protocol.hpp"
#include "lime-tester-utils.hpp"

#include <bctoolbox/exception.hh>
#include <soci/soci.h>

using namespace::std;
using namespace::soci;

namespace lime_tester {

// default url and ports of X3DH servers
std::string test_x3dh_server_url{"localhost"};
std::string test_x3dh_c25519_server_port{"25519"};
std::string test_x3dh_c448_server_port{"25520"};
std::string test_x3dh_domainA_server_port{"25521"};
std::string test_x3dh_domainB_server_port{"25522"};
std::string test_x3dh_domainC_server_port{"25523"};
std::string test_x3dh_c25519_stop_on_request_limit_server_port{"25524"};
std::string test_x3dh_c448_stop_on_request_limit_server_port{"25525"};
std::string test_x3dh_c25519k512_server_port{"25526"};
std::string test_x3dh_c25519k512_stop_on_request_limit_server_port{"25527"};

// for testing purpose RNG, no need to be a good one
std::random_device rd;
std::uniform_int_distribution<int> uniform_dist(0,255);

// default value for the timeout
int wait_for_timeout=4000;
// bundle request restriction timespan (as configured on stop on request limit server). 20s (in ms)
int bundle_request_limit_timespan=20000;

// bundle request limit as configured on test server
int bundle_request_limit=5;

// default value for initial OPk batch size, keep it small so not too many OPks generated
uint16_t OPkInitialBatchSize=3;

// messages with calibrated length to test the optimize encryption policies
// with a short one, any optimize policy shall go for the DRmessage encryption
std::string shortMessage{"Short Message"};
// with a long one(>80 <176) optimizeUploadSzie policy shall go for the cipherMessage encryption, but the optimizeGlobalBandwith stick to DRmessage (with 2 recipients)
std::string longMessage{"This message is long enough to automatically switch to cipher Message mode when at least two recipients are involved."};
// with a very long one(>176) all optimize policies shall go for the cipherMessage encryption(with 2 recipients)
std::string veryLongMessage{"This message is long enough to automatically switch to cipher Message mode when at least two recipients are involved. This message is long enough to automatically switch to cipher Message mode when at least two recipients are involved."};

std::vector<std::string> messages_pattern = {
	{"Frankly, my dear, I don't give a damn."},
	{"I'm gonna make him an offer he can't refuse."},
	{"You don't understand! I coulda had class. I coulda been a contender. I could've been somebody, instead of a bum, which is what I am."},
	{"Toto, I've a feeling we're not in Kansas anymore."},
	{"Here's looking at you, kid."},
	{"Go ahead, make my day."},
	{"All right, Mr. DeMille, I'm ready for my close-up."},
	{"May the Force be with you."},
	{"Fasten your seatbelts. It's going to be a bumpy night."},
	{"You talking to me?"},
	{"What we've got here is failure to communicate."},
	{"I love the smell of napalm in the morning. "},
	{"Love means never having to say you're sorry."},
	{"The stuff that dreams are made of."},
	{"E.T. phone home."},
	{"They call me Mister Tibbs!"},
	{"Rosebud."},
	{"Made it, Ma! Top of the world!"},
	{"I'm as mad as hell, and I'm not going to take this anymore!"},
	{"Louis, I think this is the beginning of a beautiful friendship."},
	{"A census taker once tried to test me. I ate his liver with some fava beans and a nice Chianti."},
	{"Bond. James Bond."},
	{"There's no place like home. "},
	{"I am big! It's the pictures that got small."},
	{"Show me the money!"},
	{"Why don't you come up sometime and see me?"},
	{"I'm walking here! I'm walking here!"},
	{"Play it, Sam. Play 'As Time Goes By.'"},
	{"You can't handle the truth!"},
	{"I want to be alone."},
	{"After all, tomorrow is another day!"},
	{"Round up the usual suspects."},
	{"I'll have what she's having."},
	{"You know how to whistle, don't you, Steve? You just put your lips together and blow."},
	{"You're gonna need a bigger boat."},
	{"Badges? We ain't got no badges! We don't need no badges! I don't have to show you any stinking badges!"},
	{"I'll be back."},
	{"Today, I consider myself the luckiest man on the face of the earth."},
	{"If you build it, he will come."},
	{"My mama always said life was like a box of chocolates. You never know what you're gonna get."},
	{"We rob banks."},
	{"Plastics."},
	{"We'll always have Paris."},
	{"I see dead people."},
	{"Stella! Hey, Stella!"},
	{"Oh, Jerry, don't let's ask for the moon. We have the stars."},
	{"Shane. Shane. Come back!"},
	{"Well, nobody's perfect."},
	{"It's alive! It's alive!"},
	{"Houston, we have a problem."},
	{"You've got to ask yourself one question: 'Do I feel lucky?' Well, do ya, punk?"},
	{"You had me at 'hello.'"},
	{"One morning I shot an elephant in my pajamas. How he got in my pajamas, I don't know."},
	{"There's no crying in baseball!"},
	{"La-dee-da, la-dee-da."},
	{"A boy's best friend is his mother."},
	{"Greed, for lack of a better word, is good."},
	{"Keep your friends close, but your enemies closer."},
	{"As God is my witness, I'll never be hungry again."},
	{"Well, here's another nice mess you've gotten me into!"},
	{"Say 'hello' to my little friend!"},
	{"What a dump."},
	{"Mrs. Robinson, you're trying to seduce me. Aren't you?"},
	{"Gentlemen, you can't fight in here! This is the War Room!"},
	{"Elementary, my dear Watson."},
	{"Take your stinking paws off me, you damned dirty ape."},
	{"Of all the gin joints in all the towns in all the world, she walks into mine."},
	{"Here's Johnny!"},
	{"They're here!"},
	{"Is it safe?"},
	{"Wait a minute, wait a minute. You ain't heard nothin' yet!"},
	{"No wire hangers, ever!"},
	{"Mother of mercy, is this the end of Rico?"},
	{"Forget it, Jake, it's Chinatown."},
	{"I have always depended on the kindness of strangers."},
	{"Hasta la vista, baby."},
	{"Soylent Green is people!"},
	{"Open the pod bay doors, please, HAL."},
	{"Striker: Surely you can't be serious. "},
	{"Rumack: I am serious...and don't call me Shirley."},
	{"Yo, Adrian!"},
	{"Hello, gorgeous."},
	{"Toga! Toga!"},
	{"Listen to them. Children of the night. What music they make."},
	{"Oh, no, it wasn't the airplanes. It was Beauty killed the Beast."},
	{"My precious."},
	{"Attica! Attica!"},
	{"Sawyer, you're going out a youngster, but you've got to come back a star!"},
	{"Listen to me, mister. You're my knight in shining armor. Don't you forget it. You're going to get back on that horse, and I'm going to be right behind you, holding on tight, and away we're gonna go, go, go!"},
	{"Tell 'em to go out there with all they got and win just one for the Gipper."},
	{"A martini. Shaken, not stirred."},
	{"Who's on first."},
	{"Cinderella story. Outta nowhere. A former greenskeeper, now, about to become the Masters champion. It looks like a mirac...It's in the hole! It's in the hole! It's in the hole!"},
	{"Life is a banquet, and most poor suckers are starving to death!"},
	{"I feel the need - the need for speed!"},
	{"Carpe diem. Seize the day, boys. Make your lives extraordinary."},
	{"Snap out of it!"},
	{"My mother thanks you. My father thanks you. My sister thanks you. And I thank you."},
	{"Nobody puts Baby in a corner."},
	{"I'll get you, my pretty, and your little dog, too!"},
	{"I'm the king of the world!"},
	{"I have come here to chew bubble gum and kick ass, and I'm all out of bubble gum."}
};

/**
 * @return a string describing the given curve
 */
const std::string curveId(lime::CurveId curve) {
	switch (curve) {
		case lime::CurveId::c25519 :
			return "C255519";
		case lime::CurveId::c448 :
			return "C448";
		case lime::CurveId::c25519k512 :
			return "C25519K512";
		default:
			return "UNSET";
	}
	return "UNSET";
}

/**
 * @brief Simple RNG function, used to generate random values for testing purpose, they do not need to be real random
 * so use directly std::random_device
 */
void randomize(uint8_t *buffer, const size_t size) {
	for (size_t i=0; i<size; i++) {
		buffer[i] = (uint8_t)lime_tester::uniform_dist(rd);
	}
}
/**
 * @brief Create and initialise the two sessions given in parameter. Alice as sender session and Bob as receiver one
 *	Alice must then send the first message, once bob got it, sessions are fully initialised
 *	if fileName doesn't exists as a DB, it will be created, caller shall then delete it if needed
 */
template<typename Curve>
void dr_sessionsInit(std::shared_ptr<DR> &alice, std::shared_ptr<DR> &bob, std::shared_ptr<lime::Db> &localStorageAlice, std::shared_ptr<lime::Db> &localStorageBob, std::string dbFilenameAlice, std::shared_ptr<std::recursive_mutex> db_mutex_alice, std::string dbFilenameBob, std::shared_ptr<std::recursive_mutex> db_mutex_bob, bool initStorage, std::shared_ptr<RNG> RNG_context) {
	if (initStorage==true) {
		// create or load Db
		localStorageAlice = std::make_shared<lime::Db>(dbFilenameAlice, db_mutex_alice);
		localStorageBob = std::make_shared<lime::Db>(dbFilenameBob, db_mutex_bob);
	}

	/* generate key pair for bob */
	auto tempECDH = make_keyExchange<Curve>();
	tempECDH->createKeyPair(RNG_context);
	SignedPreKey<Curve> bobSPk{tempECDH->get_selfPublic(), tempECDH->get_secret()};

	/* generate a shared secret and AD */
	lime::DRChainKey SK;
	lime::SharedADBuffer AD;
	lime_tester::randomize(SK.data(), SK.size());
	lime_tester::randomize(AD.data(), AD.size());

	// insert the peer Device (with dummy datas in lime_PeerDevices and lime_LocalUsers tables, not used in the DR tests but needed to satisfy foreign key condition on session insertion)
	long int aliceUid,bobUid,bobDid,aliceDid;
	localStorageAlice->sql<<"INSERT INTO lime_LocalUsers(UserId, Ik, server) VALUES ('dummy', 1, 'dummy')";
	localStorageAlice->sql<<"select last_insert_rowid()",soci::into(aliceUid);
	localStorageAlice->sql<<"INSERT INTO lime_PeerDevices(DeviceId, Ik) VALUES ('dummy', 1)";
	localStorageAlice->sql<<"select last_insert_rowid()",soci::into(aliceDid);

	localStorageBob->sql<<"INSERT INTO lime_LocalUsers(UserId, Ik, server) VALUES ('dummy', 1, 'dummy')";
	localStorageBob->sql<<"select last_insert_rowid()",soci::into(bobUid);
	localStorageBob->sql<<"INSERT INTO lime_PeerDevices(DeviceId, Ik) VALUES ('dummy', 1)";
	localStorageBob->sql<<"select last_insert_rowid()",soci::into(bobDid);

	// create DR sessions
	std::vector<uint8_t> X3DH_initMessage{};
	DSA<Curve, lime::DSAtype::publicKey> dummyPeerIk{}; // DR session creation gets the peerDeviceId and peerIk but uses it only if peerDid is 0, give dummy, we're focusing on DR here
	alice = make_DR_for_sender<Curve>(localStorageAlice, SK, AD, bobSPk.cpublicKey(), aliceDid, "dummyPeerDevice", dummyPeerIk, aliceUid, X3DH_initMessage, RNG_context);
	bob = make_DR_for_receiver<Curve>(localStorageBob, SK, AD, bobSPk, bobDid, "dummyPeerDevice", 0, dummyPeerIk, bobUid, RNG_context);
}

#ifdef HAVE_BCTBXPQ
template<>
void dr_sessionsInit<C255K512>(std::shared_ptr<DR> &alice, std::shared_ptr<DR> &bob, std::shared_ptr<lime::Db> &localStorageAlice, std::shared_ptr<lime::Db> &localStorageBob, std::string dbFilenameAlice, std::shared_ptr<std::recursive_mutex> db_mutex_alice, std::string dbFilenameBob, std::shared_ptr<std::recursive_mutex> db_mutex_bob, bool initStorage, std::shared_ptr<RNG> RNG_context) {
	if (initStorage==true) {
		// create or load Db
		localStorageAlice = std::make_shared<lime::Db>(dbFilenameAlice, db_mutex_alice);
		localStorageBob = std::make_shared<lime::Db>(dbFilenameBob, db_mutex_bob);
	}

	/* generate EC key pair for bob */
	auto tempECDH = make_keyExchange<C255K512::EC>();
	tempECDH->createKeyPair(RNG_context);
	Xpair<C255K512::EC> bobECKeyPair{tempECDH->get_selfPublic(), tempECDH->get_secret()};

	/* generate KEM key pair for bob */
	Kpair<C255K512::KEM> bobKEMKeyPair;
	auto tempKEM = make_KEM<C255K512::KEM>();
	tempKEM->createKeyPair(bobKEMKeyPair);
	ARrKey<C255K512> bobPK(bobECKeyPair.publicKey(), bobKEMKeyPair.publicKey());
	ARsKey<C255K512> bobKeyPair(bobECKeyPair, bobKEMKeyPair);

	/* generate a shared secret and AD */
	lime::DRChainKey SK;
	lime::SharedADBuffer AD;
	lime_tester::randomize(SK.data(), SK.size());
	lime_tester::randomize(AD.data(), AD.size());

	// insert the peer Device (with dummy datas in lime_PeerDevices and lime_LocalUsers tables, not used in the DR tests but needed to satisfy foreign key condition on session insertion)
	long int aliceUid,bobUid,bobDid,aliceDid;
	localStorageAlice->sql<<"INSERT INTO lime_LocalUsers(UserId, Ik, server) VALUES ('dummy', 1, 'dummy')";
	localStorageAlice->sql<<"select last_insert_rowid()",soci::into(aliceUid);
	localStorageAlice->sql<<"INSERT INTO lime_PeerDevices(DeviceId, Ik) VALUES ('dummy', 1)";
	localStorageAlice->sql<<"select last_insert_rowid()",soci::into(aliceDid);

	localStorageBob->sql<<"INSERT INTO lime_LocalUsers(UserId, Ik, server) VALUES ('dummy', 1, 'dummy')";
	localStorageBob->sql<<"select last_insert_rowid()",soci::into(bobUid);
	localStorageBob->sql<<"INSERT INTO lime_PeerDevices(DeviceId, Ik) VALUES ('dummy', 1)";
	localStorageBob->sql<<"select last_insert_rowid()",soci::into(bobDid);

	// create DR sessions
	std::vector<uint8_t> X3DH_initMessage{};
	DSA<C255K512::EC, lime::DSAtype::publicKey> dummyPeerIk{}; // DR session creation gets the peerDeviceId and peerIk but uses it only if peerDid is 0, give dummy, we're focusing on DR here
	alice = make_DR_for_sender<C255K512>(localStorageAlice, SK, AD, bobPK, aliceDid, "dummyPeerDevice", dummyPeerIk, aliceUid, X3DH_initMessage, RNG_context);
	bob = make_DR_for_receiver<C255K512>(localStorageBob, SK, AD, bobKeyPair, bobDid, "dummyPeerDevice", 0, dummyPeerIk, bobUid, RNG_context);
}
#endif //HAVE_BCTBXPQ


/**
 * @brief Create and initialise all requested DR sessions for specified number of devices between two or more users
 * users is a vector of users(already sized to correct size, matching usernames size), each one holds a vector of devices(already sized for each device)
 * This function will create and instanciate in each device a vector of vector of DR sessions towards all other devices: each device vector holds a bidimentionnal array indexed by userId and deviceId.
 * Session init is done considering as initial sender the lowest id user and in it the lowest id device
 * createdDBfiles is filled with all filenames of DB created to allow easy deletion
 */
template <typename Curve>
void dr_devicesInit(std::string dbBaseFilename, std::vector<std::vector<std::vector<std::vector<sessionDetails<Curve>>>>> &users, std::vector<std::string> &usernames, std::vector<std::string> &createdDBfiles, std::shared_ptr<RNG> RNG_context) {
	createdDBfiles.clear();
	/* each device must have a db, produce filename for them from provided base name and given username */
	for (size_t i=0; i<users.size(); i++) { // loop on users
		for (size_t j=0; j<users[i].size(); j++) { // loop on devices
			// create the db for this device, filename would be <dbBaseFilename>.<username>.<dev><deviceIndex>.sqlite3
			std::string dbFilename{dbBaseFilename};
			dbFilename.append(".").append(usernames[i]).append(".dev").append(std::to_string(j)).append(".sqlite3");

			remove(dbFilename.data());
			createdDBfiles.push_back(dbFilename);

			std::shared_ptr<lime::Db> localStorage = std::make_shared<lime::Db>(dbFilename, make_shared<std::recursive_mutex>());

			users[i][j].resize(users.size()); // each device holds a vector towards all users, dimension it
			// instanciate the session details for all needed sessions
			for (size_t i_fw=0; i_fw<users.size(); i_fw++) { // loop on the rest of users
				users[i][j][i_fw].resize(users[i_fw].size()); // [i][j][i_fw] holds a vector of sessions toward user i_fw devices
				for (size_t j_fw=0; j_fw<users[i].size(); j_fw++) { // loop on the rest of devices
					if (!((i_fw==i) && (j_fw==j))) { // no session towards ourself
						/* create session details with self and peer userId, user and device index */
						sessionDetails<Curve> sessionDetail{usernames[i], i, j, usernames[i_fw], i_fw, j_fw};
						sessionDetail.localStorage = localStorage;

						users[i][j][i_fw][j_fw]=std::move(sessionDetail);
					}
				}
			}
		}
	}


	/* users is a vector of users, each of them has a vector of devices, each device hold a vector of sessions */
	/* sessions init are done by users[i] to all users[>i] (same thing intra device) */
	for (size_t i=0; i<users.size(); i++) { // loop on users
		for (size_t j=0; j<users[i].size(); j++) { // loop on devices
			for (size_t j_fw=j+1; j_fw<users[i].size(); j_fw++) { // loop on the rest of our devices
				dr_sessionsInit<Curve>(users[i][j][i][j_fw].DRSession, users[i][j_fw][i][j].DRSession, users[i][j][i][j_fw].localStorage, users[i][j_fw][i][j].localStorage, " ", nullptr, " ", nullptr, false, RNG_context);
			}
			for (size_t i_fw=i+1; i_fw<users.size(); i_fw++) { // loop on the rest of users
				for (size_t j_fw=0; j_fw<users[i].size(); j_fw++) { // loop on the rest of devices
					dr_sessionsInit<Curve>(users[i][j][i_fw][j_fw].DRSession, users[i_fw][j_fw][i][j].DRSession, users[i][j][i_fw][j_fw].localStorage, users[i_fw][j_fw][i][j].localStorage, " ", nullptr, " ", nullptr, false, RNG_context);
				}
			}
		}
	}
}

bool DR_message_payloadDirectEncrypt(std::vector<uint8_t> &message) {
	// checks on length to at least perform more checks
	if (message.size()<4) return false;
	// check protocol version
	if (message[0] != static_cast<uint8_t>(lime::double_ratchet_protocol::DR_v01)) return false;
	return !!(message[1]&static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::payload_direct_encryption_flag));
}
bool DR_message_holdsAsymmetricKeys(std::vector<uint8_t> &message) {
	// checks on length to at least perform more checks
	if (message.size()<4) return false;
	// check protocol version
	if (message[0] != static_cast<uint8_t>(lime::double_ratchet_protocol::DR_v01)) return false;
	return !(message[1]&static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::KEM_pk_index));
}

bool DR_message_holdsX3DHInit(std::vector<uint8_t> &message) {
	bool dummy;
	return  DR_message_holdsX3DHInit(message, dummy);
}

bool DR_message_holdsX3DHInit(std::vector<uint8_t> &message, bool &haveOPk) {
	// checks on length
	if (message.size()<4) return false;

	// check protocol version
	if (message[0] != static_cast<uint8_t>(lime::double_ratchet_protocol::DR_v01)) return false;
	// check message type: we must have a X3DH init message
	if (!(message[1]&static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::X3DH_init_flag))) return false;
	bool payload_direct_encryption = !!(message[1]&static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::payload_direct_encryption_flag));

	/* check message length :
	 * message with payload not included (DR payload is a fixed 32 byte random seed)
	 * - header<3 bytes>, X3DH init packet, Ns+PN<4 bytes>, DHs<X<Curve>::keyLength>, Cipher message RandomSeed<32 bytes>, key auth tag<16 bytes> = <55 + X<Curve>::keyLengh + X3DH init size>
	 * message with payload included
	 * - header<3 bytes>, X3DH init packet, Ns+PN<4 bytes>, DHs<X<Curve>::keyLength>,  Payload<variable size>, key auth tag<16 bytes> = <23 + X<Curve>::keyLengh + X3DH init size>
	 */
	// EC only: X3DH init size = OPk_flag<1 byte> + selfIK<DSA<Curve>::keyLength> + EK<X<Curve>::keyLength> + SPk id<4 bytes> + OPk id (if flag is 1)<4 bytes>
	// EC/KEM: X3DH init size = OPk_flag<1 byte> + selfIK<DSA<Curve>::keyLength> + EK<X<Curve>::keyLength> + CT<K<Curve>::ctLength> + SPk id<4 bytes> + OPk id (if flag is 1)<4 bytes>
	switch (message[2]) {
		case static_cast<uint8_t>(lime::CurveId::c25519):
			if (message[3] == 0x00) { // no OPk in the X3DH init message
				if (payload_direct_encryption) {
					if (message.size() <= (23 + X<C255, lime::Xtype::publicKey>::ssize() + 5 + DSA<C255, lime::DSAtype::publicKey>::ssize() + X<C255, lime::Xtype::publicKey>::ssize())) return false;
				} else {
					if (message.size() != (55 + X<C255, lime::Xtype::publicKey>::ssize() + 5 + DSA<C255, lime::DSAtype::publicKey>::ssize() + X<C255, lime::Xtype::publicKey>::ssize())) return false;
				}
				haveOPk=false;
			} else { // OPk present in the X3DH init message
				if (payload_direct_encryption) {
					if (message.size() <= (23 + X<C255, lime::Xtype::publicKey>::ssize() + 9 + DSA<C255, lime::DSAtype::publicKey>::ssize() + X<C255, lime::Xtype::publicKey>::ssize())) return false;
				} else {
					if (message.size() != (55 + X<C255, lime::Xtype::publicKey>::ssize() + 9 + DSA<C255, lime::DSAtype::publicKey>::ssize() + X<C255, lime::Xtype::publicKey>::ssize())) return false;
				}
				haveOPk=true;
			}
			return true;
		break;
		case static_cast<uint8_t>(lime::CurveId::c448):
			if (message[3] == 0x00) { // no OPk in the X3DH init message
				if (payload_direct_encryption) {
					if (message.size() <= (23 + X<C448, lime::Xtype::publicKey>::ssize() + 5 + DSA<C448, lime::DSAtype::publicKey>::ssize() + X<C448, lime::Xtype::publicKey>::ssize())) return false;
				} else {
					if (message.size() != (55 + X<C448, lime::Xtype::publicKey>::ssize() + 5 + DSA<C448, lime::DSAtype::publicKey>::ssize() + X<C448, lime::Xtype::publicKey>::ssize())) return false;
				}
				haveOPk=false;
			} else { // OPk present in the X3DH init message
				if (payload_direct_encryption) {
					if (message.size() <= (23 + X<C448, lime::Xtype::publicKey>::ssize() + 9 + DSA<C448, lime::DSAtype::publicKey>::ssize() + X<C448, lime::Xtype::publicKey>::ssize())) return false;
				} else {
					if (message.size() != (55 + X<C448, lime::Xtype::publicKey>::ssize() + 9 + DSA<C448, lime::DSAtype::publicKey>::ssize() + X<C448, lime::Xtype::publicKey>::ssize())) return false;
				}
				haveOPk=true;
			}
			return true;

		break;
#ifdef HAVE_BCTBXPQ
		case static_cast<uint8_t>(lime::CurveId::c25519k512):
			if (message[3] == 0x00) { // no OPk in the X3DH init message
				if (payload_direct_encryption) {
					if (message.size() <= (23 + X<C255, lime::Xtype::publicKey>::ssize() + K<K512, lime::Ktype::publicKey>::ssize() + K<K512, lime::Ktype::cipherText>::ssize() + 5 + DSA<C255, lime::DSAtype::publicKey>::ssize() + K<K512, lime::Ktype::cipherText>::ssize() + X<C255, lime::Xtype::publicKey>::ssize())) return false;
				} else {
					if (message.size() != (55 + X<C255, lime::Xtype::publicKey>::ssize() + K<K512, lime::Ktype::publicKey>::ssize() + K<K512, lime::Ktype::cipherText>::ssize() + 5 + DSA<C255, lime::DSAtype::publicKey>::ssize() + K<K512, lime::Ktype::cipherText>::ssize() + X<C255, lime::Xtype::publicKey>::ssize())) return false;
				}
				haveOPk=false;
			} else { // OPk present in the X3DH init message
				if (payload_direct_encryption) {
					if (message.size() <= (23 + X<C255, lime::Xtype::publicKey>::ssize() + K<K512, lime::Ktype::publicKey>::ssize() + K<K512, lime::Ktype::cipherText>::ssize() + 9 + DSA<C255, lime::DSAtype::publicKey>::ssize() + K<K512, lime::Ktype::cipherText>::ssize() + X<C255, lime::Xtype::publicKey>::ssize())) return false;
				} else {
					if (message.size() != (55 + X<C255, lime::Xtype::publicKey>::ssize() + K<K512, lime::Ktype::publicKey>::ssize() + K<K512, lime::Ktype::cipherText>::ssize() + 9 + DSA<C255, lime::DSAtype::publicKey>::ssize() + K<K512, lime::Ktype::cipherText>::ssize() + X<C255, lime::Xtype::publicKey>::ssize())) return false;
				}
				haveOPk=true;
			}
			return true;
#endif // HAVE_BCTBXPQ
		default:
			return false;
	}
}

namespace {
size_t DR_Message_X3DHInitSize(uint8_t curveId, bool OPkFlag) {
	switch (curveId) {
		case static_cast<uint8_t>(lime::CurveId::c25519):
			return lime::double_ratchet_protocol::X3DHinitSize<C255>(OPkFlag);
		case static_cast<uint8_t>(lime::CurveId::c448):
			return lime::double_ratchet_protocol::X3DHinitSize<C448>(OPkFlag);
#ifdef HAVE_BCTBXPQ
		case static_cast<uint8_t>(lime::CurveId::c25519k512):
			return lime::double_ratchet_protocol::X3DHinitSize<C255K512>(OPkFlag);
#endif
	}
	return 0;
}
}

bool DR_message_extractX3DHInit(std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) {
	if (DR_message_holdsX3DHInit(message) == false) return false;

	// compute size
	size_t X3DH_length = DR_Message_X3DHInitSize(message[2], message[3]==0x01);

	// copy it in buffer
	X3DH_initMessage.assign(message.begin()+3, message.begin()+3+X3DH_length);
	return true;
}

/* return true if the message buffer is a valid DR message holding an X3DH init message, copy its SPk id in the given parameter */
bool DR_message_extractX3DHInit_SPkId(std::vector<uint8_t> &message, uint32_t &SPkId) {
	if (DR_message_holdsX3DHInit(message) == false) return false;

	// compute position of SPkId in message
	size_t SPkIdPos = 3+1; // DR message header + OPK flag
	switch (message[2]) {
		case static_cast<uint8_t>(lime::CurveId::c25519):
			SPkIdPos += DSA<C255, lime::DSAtype::publicKey>::ssize() + X<C255, lime::Xtype::publicKey>::ssize();
			break;
		case static_cast<uint8_t>(lime::CurveId::c448):
			SPkIdPos += DSA<C448, lime::DSAtype::publicKey>::ssize() + X<C448, lime::Xtype::publicKey>::ssize();
			break;
#ifdef HAVE_BCTBXPQ
		case static_cast<uint8_t>(lime::CurveId::c25519k512):
			SPkIdPos += DSA<C255, lime::DSAtype::publicKey>::ssize() + X<C255, lime::Xtype::publicKey>::ssize() + K<K512, lime::Ktype::cipherText>::ssize();
			break;
#endif
	}

	SPkId = message[SPkIdPos]<<24 |
		message[SPkIdPos+1]<<16 |
		message[SPkIdPos+2]<<8  |
		message[SPkIdPos+3];
	return true;
}

/* return the Ns header field */
uint16_t DR_message_get_Ns(std::vector<uint8_t> &message) {
	if (message.size()<4) return 0;
	size_t X3DHInitSize = 0;
	if ((message[1]&static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::X3DH_init_flag))) {
		X3DHInitSize = DR_Message_X3DHInitSize(message[2], message[3]==0x01);
	}
	return static_cast<uint16_t>(message[3+X3DHInitSize])<<8 | static_cast<uint16_t>(message[4+X3DHInitSize]);
}

/* Open provided DB and look for DRSessions established between selfDevice and peerDevice
 * Populate the sessionsId vector with the Ids of sessions found
 * return the id of the active session if one is found, 0 otherwise */
long int get_DRsessionsId(const std::string &dbFilename, const std::string &selfDeviceId, const std::string &peerDeviceId, std::vector<long int> &sessionsId) noexcept{
	sessionsId.clear();
	sessionsId.resize(25); // no more than 25 sessions id fetched
	std::vector<int> status(25);
	try {
		soci::session sql("sqlite3", dbFilename); // open the DB
		soci::statement st = (sql.prepare << "SELECT s.sessionId, s.Status FROM DR_sessions as s INNER JOIN lime_PeerDevices as d on s.Did = d.Did INNER JOIN lime_LocalUsers as u on u.Uid = s.Uid WHERE u.UserId = :selfId AND d.DeviceId = :peerId ORDER BY s.Status DESC, s.Did;", into(sessionsId), into(status), use(selfDeviceId), use(peerDeviceId));
		st.execute();
		if (st.fetch()) { // all retrieved session shall fit in the arrays no need to go on several fetch
			// check we don't have more than one active session
			if (status.size()>=2 && status[0]==1 && status[1]==1) {
				throw BCTBX_EXCEPTION << "In DB "<<dbFilename<<" local user "<<selfDeviceId<<" and peer device "<<peerDeviceId<<" share more than one active session";
			}

			// return the active session id if there is one
			if (status.size()>=1 && status[0]==1) {
				return sessionsId[0];
			}
		}
		sessionsId.clear();
		return 0;

	} catch (exception &e) { // swallow any error on DB
		LIME_LOGE<<"Got an error on DB: "<<e.what();
		sessionsId.clear();
		return 0;
	}
}

/* Open provided DB, look for DRSessions established between selfDevice and peerDevice, count the stored message keys in all these sessions
 * return 0 if no sessions found or no user found
 */
unsigned int get_StoredMessageKeyCount(const std::string &dbFilename, const std::string &selfDeviceId, const std::string &peerDeviceId) noexcept{
	try {
		soci::session sql("sqlite3", dbFilename); // open the DB
		unsigned int mkCount=0;
		sql<< "SELECT count(m.MK) FROM DR_sessions as s INNER JOIN lime_PeerDevices as d on s.Did = d.Did INNER JOIN lime_LocalUsers as u on u.Uid = s.Uid INNER JOIN DR_MSk_DHr as c on c.sessionId = s.sessionId INNER JOIN DR_MSk_Mk as m ON m.DHid=c.DHid WHERE u.UserId = :selfId AND d.DeviceId = :peerId ORDER BY s.Status DESC, s.Did;", into(mkCount), use(selfDeviceId), use(peerDeviceId);
		if (sql.got_data()) {
			return mkCount;
		} else {
			return 0;
		}

	} catch (exception &e) { // swallow any error on DB
		LIME_LOGE<<"Got an error while getting the MK count in DB: "<<e.what();
		return 0;
	}
}

/* For the given deviceId, count the number of associated SPk and return the Id of the active one(if any)
 * return true if an active one was found
 */
bool get_SPks(const std::string &dbFilename, const std::string &selfDeviceId, size_t &count, uint32_t &activeId) noexcept{
	try {
		soci::session sql("sqlite3", dbFilename); // open the DB
		count=0;
		sql<< "SELECT count(SPKid) FROM X3DH_SPK as s INNER JOIN lime_LocalUsers as u on u.Uid = s.Uid WHERE u.UserId = :selfId;", into(count), use(selfDeviceId);
		if (sql.got_data()) {
			sql<< "SELECT SPKid FROM X3DH_SPK as s INNER JOIN lime_LocalUsers as u on u.Uid = s.Uid WHERE u.UserId = :selfId AND s.Status=1 LIMIT 1;", into(activeId), use(selfDeviceId);
			if (sql.got_data()) {
				return true;
			} else {
				return false;
			}
		} else {
			return false;
		}
	} catch (exception &e) { // swallow any error on DB
		LIME_LOGE<<"Got an error while getting the SPk count in DB: "<<e.what();
		count=0;
		return false;
	}
}

/* For the given deviceId, count the number of associated OPk
 */
size_t get_OPks(const std::string &dbFilename, const std::string &selfDeviceId) noexcept {
	try {
		soci::session sql("sqlite3", dbFilename); // open the DB
		auto count=0;
		sql<< "SELECT count(OPKid) FROM X3DH_OPK as o INNER JOIN lime_LocalUsers as u on u.Uid = o.Uid WHERE u.UserId = :selfId;", into(count), use(selfDeviceId);
		if (sql.got_data()) {
			return count;
		} else {
			return 0;
		}
	} catch (exception &e) { // swallow any error on DB
		LIME_LOGE<<"Got an error while getting the OPk count in DB: "<<e.what();
		return 0;
	}

}

/* Move back in time all timeStamps by the given amout of days
 * DB holds timeStamps in DR_sessions and X3DH_SPK tables
 */
void forwardTime(const std::string &dbFilename, int days) noexcept {
	try {
		LIME_LOGI<<"Set timestamps back by "<<days<<" days";
		soci::session sql("sqlite3", dbFilename); // open the DB
		/* move back by days all timeStamp, we have some in DR_sessions, X3DH_SPk and LocalUsers tables */
		sql<<"UPDATE DR_sessions SET timeStamp = date (timeStamp, '-"<<days<<" day');";
		sql<<"UPDATE X3DH_SPK SET timeStamp = date (timeStamp, '-"<<days<<" day');";
		sql<<"UPDATE X3DH_OPK SET timeStamp = date (timeStamp, '-"<<days<<" day');";
		sql<<"UPDATE Lime_LocalUsers SET updateTs = date (updateTs, '-"<<days<<" day');";
	} catch (exception &e) { // swallow any error on DB
		LIME_LOGE<<"Got an error forwarding time in DB: "<<e.what();
	}
}

const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
/**
 * @brief append a random suffix to user name to avoid collision if test server is user by several tests runs
 *
 * @param[in] basename
 *
 * @return a shared ptr towards a string holding name+ 6 chars random suffix
 */
std::shared_ptr<std::string> makeRandomDeviceName(const char *basename) {
	auto ret = make_shared<std::string>(basename);
	std::array<uint8_t,6> rnd;
	lime_tester::randomize(rnd.data(), rnd.size());
	for (auto x : rnd) {
		ret->append(1, charset[x%(sizeof(charset)-1)]);
	}
	return ret;
}

// wait for a counter to reach a value or timeout to occur, gives ticks to the belle-sip stack every SLEEP_TIME
int wait_for(belle_sip_stack_t*s1,int* counter,int value,int timeout) {
	int retry=0;
#define SLEEP_TIME 50
	while (*counter!=value && retry++ <(timeout/SLEEP_TIME)) {
		if (s1) belle_sip_stack_sleep(s1,SLEEP_TIME);
	}
	if (*counter!=value) return FALSE;
	else return TRUE;
}

// same as above but multithread proof with a mutex
int wait_for_mutex(belle_sip_stack_t*s1,int* counter,int value,int timeout, std::shared_ptr<std::recursive_mutex> mutex) {
	int retry=0;
#define SLEEP_TIME 50
	while (*counter!=value && retry++ <(timeout/SLEEP_TIME)) {
		std::unique_lock<std::recursive_mutex> lock(*mutex);
		if (s1) belle_sip_stack_sleep(s1,SLEEP_TIME);
		lock.unlock();
	}
	if (*counter!=value) return FALSE;
	else return TRUE;
}

// template instanciation
#ifdef EC25519_ENABLED
	template void dr_sessionsInit<C255>(std::shared_ptr<DR> &alice, std::shared_ptr<DR> &bob, std::shared_ptr<lime::Db> &localStorageAlice, std::shared_ptr<lime::Db> &localStorageBob, std::string dbFilenameAlice, std::shared_ptr<std::recursive_mutex> db_mutex_alice, std::string dbFilenameBob, std::shared_ptr<std::recursive_mutex> db_mutex_bob, bool initStorage, std::shared_ptr<RNG> RNG_context);
	template void dr_devicesInit<C255>(std::string dbBaseFilename, std::vector<std::vector<std::vector<std::vector<sessionDetails<C255>>>>> &users, std::vector<std::string> &usernames, std::vector<std::string> &createdDBfiles, std::shared_ptr<RNG> RNG_context);
#endif
#ifdef EC448_ENABLED
	template void dr_sessionsInit<C448>(std::shared_ptr<DR> &alice, std::shared_ptr<DR> &bob, std::shared_ptr<lime::Db> &localStorageAlice, std::shared_ptr<lime::Db> &localStorageBob, std::string dbFilenameAlice, std::shared_ptr<std::recursive_mutex> db_mutex_alice, std::string dbFilenameBob, std::shared_ptr<std::recursive_mutex> db_mutex_bob, bool initStorage, std::shared_ptr<RNG> RNG_context);
	template void dr_devicesInit<C448>(std::string dbBaseFilename, std::vector<std::vector<std::vector<std::vector<sessionDetails<C448>>>>> &users, std::vector<std::string> &usernames, std::vector<std::string> &createdDBfiles, std::shared_ptr<RNG> RNG_context);
#endif
#ifdef HAVE_BCTBXPQ
	template void dr_devicesInit<C255K512>(std::string dbBaseFilename, std::vector<std::vector<std::vector<std::vector<sessionDetails<C255K512>>>>> &users, std::vector<std::string> &usernames, std::vector<std::string> &createdDBfiles, std::shared_ptr<RNG> RNG_context);
#endif //HAVE_BCTBXPQ
} // namespace lime_tester

