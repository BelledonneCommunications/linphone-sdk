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

	}

}

#endif /* BCTBX_UTILS_H */
