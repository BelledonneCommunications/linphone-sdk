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
#include "lime_crypto_primitives.hpp"
#include <mutex>

namespace lime {

	/**
	 * @brief Database access class
	 *
	 * relies on SOCI
	 */
	class Db {
	public:
		/// soci connexion to DB
		soci::session	sql;
		/// mutex on database access
		std::shared_ptr<std::recursive_mutex> m_db_mutex;

		Db()=delete; // we can't create a new DB holder without DB filename

		/**
		 * @brief Open and check DB validity, create or update db schema is needed
		 *
		 * @param[in]	filename	The path to DB file
		 * @param[in]	db_mutex	database access mutex
		 */
		Db(const std::string &filename, std::shared_ptr<std::recursive_mutex> db_mutex);
		~Db(){sql.close();};

		void load_LimeUser(const std::string &deviceId, long int &Uid, lime::CurveId &curveId, std::string &url, const bool allStatus=false);
		void delete_LimeUser(const std::string &deviceId);
		void clean_DRSessions();
		void clean_SPk();
		bool is_updateRequested(const std::string &deviceId);
		void set_updateTs(const std::string &deviceId);
		void set_peerDeviceStatus(const std::string &peerDeviceId, const std::vector<uint8_t> &Ik, lime::PeerDeviceStatus status);
		void set_peerDeviceStatus(const std::string &peerDeviceId, lime::PeerDeviceStatus status);
		lime::PeerDeviceStatus get_peerDeviceStatus(const std::string &peerDeviceId);
		lime::PeerDeviceStatus get_peerDeviceStatus(const std::list<std::string> &peerDeviceIds);
		bool is_localUser(const std::string &deviceId);
		void delete_peerDevice(const std::string &peerDeviceId);
		template <typename Curve>
		long int check_peerDevice(const std::string &peerDeviceId, const DSA<Curve, lime::DSAtype::publicKey> &peerIk, const bool updateInvalid=false);
		template <typename Curve>
		long int store_peerDevice(const std::string &peerDeviceId, const DSA<Curve, lime::DSAtype::publicKey> &peerIk);
		void start_transaction();
		void commit_transaction();
		void rollback_transaction();
	};

	/* this templates are instanciated once in the lime_localStorage.cpp file, explicitly tell anyone including this header that there is no need to re-instanciate them */
#ifdef EC25519_ENABLED
	extern template long int Db::check_peerDevice<C255>(const std::string &peerDeviceId, const DSA<C255, lime::DSAtype::publicKey> &Ik, const bool updateInvalid);
	extern template long int Db::store_peerDevice<C255>(const std::string &peerDeviceId, const DSA<C255, lime::DSAtype::publicKey> &Ik);
#endif

#ifdef EC448_ENABLED
	extern template long int Db::check_peerDevice<C448>(const std::string &peerDeviceId, const DSA<C448, lime::DSAtype::publicKey> &Ik, const bool updateInvalid);
	extern template long int Db::store_peerDevice<C448>(const std::string &peerDeviceId, const DSA<C448, lime::DSAtype::publicKey> &Ik);
#endif

}

#endif /* lime_localStorage_hpp */
