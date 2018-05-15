/*
	lime_localStorage.hpp
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

#ifndef lime_localStorage_hpp
#define lime_localStorage_hpp

#include "soci/soci.h"

namespace lime {

	/**
	 * Database access class
	 * relies on SOCI
	 */
	class Db {
	public:
		/// soci connexion to DB
		soci::session	sql;

		Db()=delete; // we can't create a new DB holder without DB filename

		/**
		 * @brief Constructor, open and check DB validity, create or update db schema is needed
		 *
		 * @param[in]	filename	The path to DB file
		 */
		Db(std::string filename);
		~Db(){sql.close();};

		/**
		 * @brief Check for existence, retrieve Uid for local user based on its deviceId(GRUU) and curve from table lime_LocalUsers
		 *
		 * @param[in]	deviceId	a string holding the user to look for in DB, shall be its GRUU
		 * @param[out]	Uid		the DB internal Id matching given userId(if found in DB, 0 otherwise)
		 * @param[out]	curveId		the curve selected at user creation
		 * @param[out]	url		the url of the X3DH server this user is registered on
		 *
		 */
		void load_LimeUser(const std::string &deviceId, long int &Uid, lime::CurveId &curveId, std::string &url);

		/**
		 * @brief if exists, delete user
		 *
		 * @param[in]	deviceId	a string holding the user to look for in DB, shall be its GRUU
		 *
		 */
		void delete_LimeUser(const std::string &deviceId);

		/**
		 * @brief Delete old stale sessions and old stored message key
		 * 	- DR Session in stale status for more than DRSession_limboTime are deleted
		 * 	- MessageKey stored linked to a session who received more than maxMessagesReceivedAfterSkip are deleted
		 * 	Note : The messagekeys count is on a chain, so if we have in a chain
		 * 	Received1 Skip1 Skip2 Received2 Received3 Skip3 Received4
		 * 	The counter will be reset to 0 when we insert Skip3 (when Received4 arrives) so Skip1 and Skip2 won't be deleted until we got the counter above max on this chain
		 * 	Once we moved to next chain(as soon as peer got an answer from us and replies), the count won't be reset anymore
		 */
		void clean_DRSessions();

		/**
		 * @brief Delete old stale SPk. Apply to all users in localStorage
		 * 	- SPk in stale status for more than SPK_limboTime_days are deleted
		 */
		void clean_SPk();

		/**
		 * @brief Get a list of deviceIds of all local users present in localStorage
		 *
		 * @param[out]	deviceIds	the list of all local users (their device Id)
		 */
		void get_allLocalDevices(std::vector<std::string> &deviceIds);

		/**
		 * @brief set the identity verified flag for peer device
		 *
		 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
		 * @param[in]	Ik		the EdDSA peer public identity key, formatted as in RFC8032
		 * @param[in]	status		value of flag to set
		 *
		 * throw an exception if given key doesn't match the one present in local storage
		 * if peer Device is not present in local storage and status is true, it is added, if status is false, it is just ignored
		 */
		void set_PeerDevicesVerifiedStatus(const std::string &peerDeviceId, const std::vector<uint8_t> &Ik, bool status);

		/**
		 * @brief get the identity verified flag for peer device
		 *
		 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
		 *
		 * @return the stored Verified status, false if peer Device is not present in local Storage
		 */
		bool get_PeerDevicesIdentityVerifiedStatus(const std::string &peerDeviceId);

	};

#ifdef EC25519_ENABLED
#endif

#ifdef EC448_ENABLED
#endif

}

#endif /* lime_localStorage_hpp */
