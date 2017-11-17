/*
	lime-tester-utils.cpp
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

#include <vector>
#include <string>
#include "lime_utils.hpp"
#include "lime/lime.hpp"
#include "lime_keys.hpp"
#include "lime_double_ratchet_protocol.hpp"
#include "bctoolbox/exception.hh"
#include "lime-tester-utils.hpp"

#include "soci/soci.h"
#include "soci/sqlite3/soci-sqlite3.h"
using namespace::std;
using namespace::soci;

namespace lime {
std::vector<std::string> lime_messages_pattern = {
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
 * @brief Create and initialise the two sessions given in parameter. Alice as sender session and Bob as receiver one
 *	Alice must then send the first message, once bob got it, sessions are fully initialised
 *	if fileName doesn't exists as a DB, it will be created, caller shall then delete it if needed
 */
template <typename Curve>
void dr_sessionsInit(std::shared_ptr<DR<Curve>> &alice, std::shared_ptr<DR<Curve>> &bob, std::shared_ptr<lime::Db> &localStorageAlice, std::shared_ptr<lime::Db> &localStorageBob, std::string dbFilenameAlice, std::string dbFilenameBob, bool initStorage) {
	if (initStorage==true) {
		// create or load Db
		localStorageAlice = std::make_shared<lime::Db>(dbFilenameAlice);
		localStorageBob = std::make_shared<lime::Db>(dbFilenameBob);
	}

	// create and init a RNG needed for shared secret generation
	bctbx_rng_context_t *RNG = bctbx_rng_context_new();

	/* generate key pair for bob */
	bctbx_ECDHContext_t *tempECDH = ECDHInit<Curve>();
	bctbx_ECDHCreateKeyPair(tempECDH, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, RNG);
	KeyPair<X<Curve>> bobKeyPair{tempECDH->selfPublic, tempECDH->secret};
	bctbx_DestroyECDHContext(tempECDH);

	/* generate a shared secret and AD */
	lime::DRChainKey SK;
	lime::SharedADBuffer AD;
	bctbx_rng_get(RNG, SK.data(), SK.size());
	bctbx_rng_get(RNG, AD.data(), AD.size());
	bctbx_rng_context_free(RNG);

	// insert the peer Device (with dummy datas in lime_PeerDevices and lime_LocalUsers tables, not used in the DR tests but needed to satisfy foreign key condition on session insertion)
	long int aliceUid,bobUid,bobDid,aliceDid;
	localStorageAlice->sql<<"INSERT INTO lime_LocalUsers(UserId, Ik, server) VALUES ('dummy', 1, 'dummy')";
	localStorageAlice->sql<<"select last_insert_rowid()",soci::into(aliceUid);
	localStorageAlice->sql<<"INSERT INTO lime_PeerDevices(DeviceId, Uid, Ik) VALUES ('dummy', :Uid, 1)", soci::use(aliceUid);
	localStorageAlice->sql<<"select last_insert_rowid()",soci::into(aliceDid);

	localStorageBob->sql<<"INSERT INTO lime_LocalUsers(UserId, Ik, server) VALUES ('dummy', 1, 'dummy')";
	localStorageBob->sql<<"select last_insert_rowid()",soci::into(bobUid);
	localStorageBob->sql<<"INSERT INTO lime_PeerDevices(DeviceId, Uid, Ik) VALUES ('dummy', :Uid, 1)", soci::use(bobUid);
	localStorageBob->sql<<"select last_insert_rowid()",soci::into(bobDid);

	// create DR sessions
	std::vector<uint8_t> X3DH_initMessage{};
	alice = std::make_shared<DR<Curve>>(localStorageAlice.get(), SK, AD, bobKeyPair.publicKey(), aliceDid, X3DH_initMessage);
	bob = std::make_shared<DR<Curve>>(localStorageBob.get(), SK, AD, bobKeyPair, bobDid);
}


/**
 * @brief Create and initialise all requested DR sessions for specified number of devices between two or more users
 * users is a vector of users(already sized to correct size, matching usernames size), each one holds a vector of devices(already sized for each device)
 * This function will create and instanciate in each device a vector of vector of DR sessions towards all other devices: each device vector holds a bidimentionnal array indexed by userId and deviceId.
 * Session init is done considering as initial sender the lowest id user and in it the lowest id device
 * createdDBfiles is filled with all filenames of DB created to allow easy deletion
 */
template <typename Curve>
void dr_devicesInit(std::string dbBaseFilename, std::vector<std::vector<std::vector<std::vector<sessionDetails<Curve>>>>> &users, std::vector<std::string> &usernames, std::vector<std::string> &createdDBfiles) {
	createdDBfiles.clear();
	/* each device must have a db, produce filename for them from provided base name and given username */
	for (size_t i=0; i<users.size(); i++) { // loop on users
		for (size_t j=0; j<users[i].size(); j++) { // loop on devices
			// create the db for this device, filename would be <dbBaseFilename>.<username>.<dev><deviceIndex>.sqlite3
			std::string dbFilename{dbBaseFilename};
			dbFilename.append(".").append(usernames[i]).append(".dev").append(std::to_string(j)).append(".sqlite3");

			remove(dbFilename.data());
			createdDBfiles.push_back(dbFilename);

			std::shared_ptr<lime::Db> localStorage = std::make_shared<lime::Db>(dbFilename);

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
				dr_sessionsInit(users[i][j][i][j_fw].DRSession, users[i][j_fw][i][j].DRSession, users[i][j][i][j_fw].localStorage, users[i][j_fw][i][j].localStorage, " ", " ", false);
			}
			for (size_t i_fw=i+1; i_fw<users.size(); i_fw++) { // loop on the rest of users
				for (size_t j_fw=0; j_fw<users[i].size(); j_fw++) { // loop on the rest of devices
					dr_sessionsInit(users[i][j][i_fw][j_fw].DRSession, users[i_fw][j_fw][i][j].DRSession, users[i][j][i_fw][j_fw].localStorage, users[i_fw][j_fw][i][j].localStorage, " ", " ", false);
				}
			}
		}
	}
}


bool DR_message_holdsX3DHInit(std::vector<uint8_t> &message) {
	// checks on length
	if (message.size()<4) return false;

	// check protocol version
	if (message[0] != static_cast<uint8_t>(lime::double_ratchet_protocol::DR_v01)) return false;
	// check message type: we must have a X3DH init message
	if (message[1] !=static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::x3dhinit)) return false;

	// check packet length, packet is :
	// header<3 bytes>, X3DH init packet, Ns+PN<4 bytes>, DHs<X<Curve>::keyLength>, Cipher message Key+tag: DRMessageKey + DRMessageIV <48 bytes>, key auth tag<16 bytes> = <71 + X<Curve>::keyLengh + X3DH init size>
	// X3DH init size = OPk_flag<1 byte> + selfIK<ED<Curve>::keyLength> + EK<X<Curve>::keyLenght> + SPk id<4 bytes> + OPk id (if flag is 1)<4 bytes>
	switch (message[2]) {
		case static_cast<uint8_t>(lime::CurveId::c25519):
			if (message[3] == 0x00) { // no OPk in the X3DH init message
				if (message.size() != (71 + X<C255>::keyLength() + 5 + ED<C255>::keyLength() + X<C255>::keyLength())) return false;
			} else { // OPk present in the X3DH init message
				if (message.size() != (71 + X<C255>::keyLength() + 9 + ED<C255>::keyLength() + X<C255>::keyLength())) return false;
			}
			return true;
		break;
		case static_cast<uint8_t>(lime::CurveId::c448):
			if (message[3] == 0x00) { // no OPk in the X3DH init message
				if (message.size() != (71 + X<C448>::keyLength() + 5 + ED<C448>::keyLength() + X<C448>::keyLength())) return false;
			} else { // OPk present in the X3DH init message
				if (message.size() != (71 + X<C448>::keyLength() + 9 + ED<C448>::keyLength() + X<C448>::keyLength())) return false;
			}
			return true;

		break;
		default:
			return false;
	}
}


bool DR_message_extractX3DHInit(std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) {
	if (DR_message_holdsX3DHInit(message) == false) return false;

	// compute size
	size_t X3DH_length = 5;
	if (message[2] == static_cast<uint8_t>(lime::CurveId::c25519)) { // curve 25519
		X3DH_length += ED<C255>::keyLength() + X<C255>::keyLength();
	} else { // curve 448
		X3DH_length += ED<C448>::keyLength() + X<C448>::keyLength();
	}

	if (message[3] == 0x00) { // there is an OPk id
		X3DH_length += 4;
	}

	// copy it in buffer
	X3DH_initMessage.assign(message.begin()+3, message.begin()+3+X3DH_length);
	return true;
}


/* Open provided DB and look for DRSessions established between selfDevice and peerDevice
 * Populate the sessionsId vector with the Ids of sessions found
 * return the id of the active session if one is found, 0 otherwise */
long int get_DRsessionsId(const std::string &dbFilename, const std::string &selfDeviceId, const std::string &peerDeviceId, std::vector<long int> &sessionsId) {
	soci::session sql(sqlite3, dbFilename); // open the DB
	sessionsId.clear();
	sessionsId.resize(25); // no more than 25 sessions id fetched
	std::vector<int> status(25);
	try {
		soci::statement st = (sql.prepare << "SELECT s.sessionId, s.Status FROM DR_sessions as s INNER JOIN lime_PeerDevices as d on s.Did = d.Did INNER JOIN lime_LocalUsers as u on u.Uid = d.Uid WHERE u.UserId = :selfId AND d.DeviceId = :peerId ORDER BY s.Status DESC, s.Did;", into(sessionsId), into(status), use(selfDeviceId), use(peerDeviceId));
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
		BCTBX_SLOGE<<"Got an error on DB: "<<e.what();
		sessionsId.clear();
		return 0;
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
	bctbx_rng_context_t *RNG = bctbx_rng_context_new();
	std::array<uint8_t,6> rnd;
	bctbx_rng_get(RNG, rnd.data(), rnd.size());
	for (auto x : rnd) {
		ret->append(1, charset[x%(sizeof(charset)-1)]);
	}
	bctbx_rng_context_free(RNG);
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

// template instanciation
#ifdef EC25519_ENABLED
	template void dr_sessionsInit<C255>(std::shared_ptr<DR<C255>> &alice, std::shared_ptr<DR<C255>> &bob, std::shared_ptr<lime::Db> &localStorageAlice, std::shared_ptr<lime::Db> &localStorageBob, std::string dbFilenameAlice, std::string dbFilenameBob, bool initStorage); 
	template void dr_devicesInit<C255>(std::string dbBaseFilename, std::vector<std::vector<std::vector<std::vector<sessionDetails<C255>>>>> &users, std::vector<std::string> &usernames, std::vector<std::string> &createdDBfiles);
#endif
#ifdef EC448_ENABLED
	template void dr_sessionsInit<C448>(std::shared_ptr<DR<C448>> &alice, std::shared_ptr<DR<C448>> &bob, std::shared_ptr<lime::Db> &localStorageAlice, std::shared_ptr<lime::Db> &localStorageBob, std::string dbFilenameAlice, std::string dbFilenameBob, bool initStorage); 
	template void dr_devicesInit<C448>(std::string dbBaseFilename, std::vector<std::vector<std::vector<std::vector<sessionDetails<C448>>>>> &users, std::vector<std::string> &usernames, std::vector<std::string> &createdDBfiles);
#endif


} // namespace lime

