/*
	lime_localStorage.hpp
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

#ifndef lime_localStorage_hpp
#define lime_localStorage_hpp

#include "soci/soci.h"

namespace lime {

	class Db {
	public:
		// soci connexion to DB
		soci::session	sql;

		Db()=delete; // we can't create a new DB holder without DB filename
		Db(std::string filename);
		~Db(){sql.close();};

		void load_LimeUser(const std::string &userId, long int &Uid, lime::CurveId &curveId, std::string &url);
		void delete_LimeUser(const std::string &userId);
		void clean_DRSessions();
	};

#ifdef EC25519_ENABLED
#endif

#ifdef EC448_ENABLED
#endif

}

#endif /* lime_localStorage_hpp */
