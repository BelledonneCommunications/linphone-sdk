/*
 * Copyright (c) 2016-2021 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BCTBX_UTILS_H
#define BCTBX_UTILS_H

#include <regex>
#include <string>
#include <vector>

#include "bctoolbox/port.h"

namespace bctoolbox {

	namespace Utils {
		BCTBX_PUBLIC std::vector<std::string> split (const std::string &str, const std::string &delimiter);

		BCTBX_PUBLIC inline std::vector<std::string> split (const std::string &str, char delimiter) {
			return split(str, std::string(1, delimiter));
		}

		template<typename T>
		inline const T &getEmptyConstRefObject () {
			static const T object{};
			return object; 
		}

		BCTBX_PUBLIC std::string fold (const std::string &str);
		BCTBX_PUBLIC std::string unfold (const std::string &str);
		
		// Replace all "from" by "to" in source. Use 'recursive' to avoid replacing what has been replaced.
		BCTBX_PUBLIC void replace(std::string& source, const std::string& from, const std::string& to, const bool& recursive = true);
		
		// Return the current state of memory as a string. This is currently implemented only for Windows.
		BCTBX_PUBLIC std::string getMemoryReportAsString();
		
		/**
		 *  The purpose is to replace std::find for search with case insensitive, without having to deal with locale.
		 *  Set 'results' as string results from regex_search.
		 *  
		 *  Return 0 if successful, -1 otherwise. (Linphone convention)
		 */
		BCTBX_PUBLIC int find(std::smatch * results, const std::string &stringWords, const std::string &filter);
	
	}

}

#endif /* BCTBX_UTILS_H */
