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
#ifndef lime_log_hpp
#define lime_log_hpp

#include <string>

#define BCTBX_LOG_DOMAIN "lime"
#include <bctoolbox/logging.h>

#define LIME_LOGD BCTBX_SLOGD
#define LIME_LOGI BCTBX_SLOGI
#define LIME_LOGW BCTBX_SLOGW
#define LIME_LOGE BCTBX_SLOGE

namespace lime {
	/**
	 * convert a byte buffer into hexadecimal string
	 * if digest is > 0, only print the first and last digest bytes
	 */
	void hexStr(std::ostringstream &os, const uint8_t *data, size_t len, size_t digest=0);
}

#endif //lime_log_hpp
