/*
	lime_log.hpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2018  Belledonne Communications SARL

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
#define lime_log_hpp

#include <iomanip>
#include <sstream>
#include <memory>


using namespace::std;

namespace lime {
	void hexStr(std::ostringstream &os, const uint8_t *data, size_t len, size_t digestSize) {
		os << std::dec<<"("<<len<<"B) "<<std::hex;
		if (len > 0) {;
			os << std::setw(2) << std::setfill('0') << (int)data[0];
			if (digestSize>0 && len>digestSize*2 ) {
				for( size_t i(1) ; i < digestSize; ++i ) {
					os << ", "<<std::setw(2) << std::setfill('0') << (int)data[i];
				}
				os << " .. "<<std::setw(2) << std::setfill('0') << (int)data[len-digestSize];
				for( size_t i(len -digestSize +1) ; i < len; ++i ) {
					os << ", "<<std::setw(2) << std::setfill('0') << (int)data[i];
				}
			} else {
				for( size_t i(1) ; i < len; ++i ) {
					os << ", "<<std::setw(2) << std::setfill('0') << (int)data[i];
				}
			}
		}
	}
} // namespace lime

