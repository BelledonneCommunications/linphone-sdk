/*
	bctoolbox
	Copyright (C) 2016  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "bctoolbox/logging.h"
#include "bctoolbox/regex.h"
#include <regex>

extern "C" bool_t bctbx_is_matching_regex(const char *entry, const char* regex) {
	try {
		std::regex entry_regex(regex, std::regex_constants::extended | std::regex_constants::nosubs);
		std::cmatch m;
		return std::regex_match(entry, m, entry_regex);
	}
	catch (const std::regex_error& e) {
		bctbx_error("Could not compile regex '%s': %s", regex, e.what());
		return FALSE;
	}
}