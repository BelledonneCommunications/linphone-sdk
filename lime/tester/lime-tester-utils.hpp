/*
	lime-tester-utils.hpp
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

#ifndef lime_tester_utils_hpp
#define lime_tester_utils_hpp

#include "bctoolbox/crypto.h"
#include "lime_double_ratchet.hpp"
#include "lime_localStorage.hpp"

#include "soci/sqlite3/soci-sqlite3.h"

namespace lime {

extern std::vector<std::string> lime_messages_pattern;

/**
 * @brief Create and initialise the two sessions given in parameter. Alice as sender session and Bob as receiver one
 *	Alice must then send the first message, once bob got it, sessions are fully initialised
 *	if fileName doesn't exists as a DB, it will be created, caller shall then delete it if needed
 */
template <typename Curve>
void dr_sessionsInit(std::shared_ptr<DR<Curve>> &alice, std::shared_ptr<DR<Curve>> &bob, std::shared_ptr<lime::Db> &localStorageAlice, std::shared_ptr<lime::Db> &localStorageBob, std::string dbFilenameAlice, std::string dbFilenameBob, bool initStorage=true) {
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

/* non efficient but used friendly structure to store all details about a session */
/* the self_xx are redundants but it's for testing purpose */
template <typename Curve>
struct sessionDetails {
	std::string self_userId;
	std::size_t self_userIndex;
	std::size_t self_deviceIndex;
	std::string peer_userId;
	std::size_t peer_userIndex;
	std::size_t peer_deviceIndex;
	std::shared_ptr<DR<Curve>> DRSession; // Session to reach recipient
	std::shared_ptr<lime::Db> localStorage; // db linked to device
	sessionDetails() : self_userId{}, self_userIndex{0}, self_deviceIndex{0}, peer_userId{}, peer_userIndex{0}, peer_deviceIndex{0}, DRSession{}, localStorage{} {};
	sessionDetails(std::string &s_userId, size_t s_userIndex, size_t s_deviceIndex, std::string &p_userId, size_t p_userIndex, size_t p_deviceIndex)
		: self_userId{s_userId}, self_userIndex{s_userIndex}, self_deviceIndex{s_deviceIndex}, peer_userId{p_userId}, peer_userIndex{p_userIndex}, peer_deviceIndex{p_deviceIndex}, DRSession{}, localStorage{} {};
};

/**
 * @brief Create and initialise all requested DR sessions for specified number of devices between two or more users
 * users is a vector of users(already sized to correct size, matching usernames size), each one holds a vector of devices(already sized for each device)
 * This function will create and instanciate in each device a vector of vector of DR sessions towards all other devices: each device vector holds a bidimentionnal array indexed by userId and deviceId.
 * Session init is done considering as initial sender the lowest id user and in it the lowest id device
 */
template <typename Curve>
void dr_devicesInit(std::string dbBaseFilename, std::vector<std::vector<std::vector<std::vector<sessionDetails<Curve>>>>> &users, std::vector<std::string> &usernames) {
	/* each device must have a db, produce filename for them from provided base name and given username */
	for (size_t i=0; i<users.size(); i++) { // loop on users
		for (size_t j=0; j<users[i].size(); j++) { // loop on devices
			// create the db for this device, filename would be <dbBaseFilename>.<username>.<dev><deviceIndex>.sqlite3
			std::string dbFilename{dbBaseFilename};
			dbFilename.append(".").append(usernames[i]).append(".dev").append(std::to_string(j)).append(".sqlite3");

			remove(dbFilename.data());

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

/* return true if the message buffer is a valid DR message holding a X3DH init one in its header */
bool DR_message_holdsX3DHInit(std::vector<uint8_t> &message);

/* return true if the message buffer is a valid DR message holding a X3DH init one in its header and copy the X3DH init message in the provided buffer */
bool DR_message_extractX3DHInit(std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage);

/* Open provided DB and look for DRSessions established between selfDevice and peerDevice
 * Populate the sessionsId vector with the Ids of sessions found
 * return the id of the active session if one is found, 0 otherwise */
long int get_DRsessionsId(const std::string &dbFilename, const std::string &selfDeviceId, const std::string &peerDeviceId, std::vector<long int> &sessionsId);

/**
 * @brief append a random suffix to user name to avoid collision if test server is user by several tests runs
 *
 * @param[in] basename
 *
 * @return a shared ptr towards a string holding name+ 6 chars random suffix
 */
std::shared_ptr<std::string> makeRandomDeviceName(const char *basename);

} // namespace lime

#endif
