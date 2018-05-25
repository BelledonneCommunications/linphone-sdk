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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bctoolbox/logging.h"
#include "bctoolbox/port.h"
#include "bctoolbox/charconv.h"

extern "C" char *bctbx_locale_to_utf8 (const char *str) {
	// TODO remove this part when the NDK will contain a usable iconv
	return bctbx_strdup(str);
}

extern "C" char *bctbx_utf8_to_locale (const char *str) {
	// TODO remove this part when the NDK will contain a usable iconv
	return bctbx_strdup(str);
}

extern "C" char *bctbx_convert_any_to_utf8 (const char *str, const char *encoding) {
	if (!encoding)
		return NULL;

	// TODO change this part when the NDK will contain a usable iconv
	return bctbx_strdup(str);
}
