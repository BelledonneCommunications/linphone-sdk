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
	std::string hexStr(const uint8_t *data, int len) {
		std::stringstream ss;
		ss << "("<<len<<") "<<std::hex;
		for( int i(0) ; i < len; ++i )
			ss << std::setw(2) << std::setfill('0') << (int)data[i];
		return ss.str();
	}

	void hexStr(std::ostringstream &os, const uint8_t *data, int len) {
		os <<std::hex;
		if (len > 0) {;
			os << std::setw(2) << std::setfill('0') << (int)data[0];
			for( int i(1) ; i < len; ++i ) {
				os << ", "<<std::setw(2) << std::setfill('0') << (int)data[i];
			}
		}
	}
} // namespace lime

