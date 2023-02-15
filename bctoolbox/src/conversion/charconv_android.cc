/*
 * Copyright (c) 2016-2020 Belledonne Communications SARL.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bctoolbox/charconv.h"
#include "bctoolbox/defs.h"
#include "bctoolbox/logging.h"
#include "bctoolbox/port.h"

char *bctbx_locale_to_utf8(const char *str) {
	// TODO remove this part when the NDK will contain a usable iconv
	return bctbx_strdup(str);
}

char *bctbx_utf8_to_locale(const char *str) {
	// TODO remove this part when the NDK will contain a usable iconv
	return bctbx_strdup(str);
}

char *bctbx_convert_any_to_utf8(const char *str, BCTBX_UNUSED(const char *encoding)) {
	// TODO change this part when the NDK will contain a usable iconv
	return bctbx_strdup(str);
}

char *bctbx_convert_utf8_to_any(const char *str, BCTBX_UNUSED(const char *encoding)) {
	// TODO change this part when the NDK will contain a usable iconv
	return bctbx_strdup(str);
}

char *
bctbx_convert_string(const char *str, BCTBX_UNUSED(const char *from_encoding), BCTBX_UNUSED(const char *to_encoding)) {
	// TODO change this part when the NDK will contain a usable iconv
	return bctbx_strdup(str);
}

wchar_t *bctbx_string_to_wide_string(BCTBX_UNUSED(const char *str)) {
	// TODO
	bctbx_error("Conversion from string to wide string is not implemented");
	return NULL;
}

unsigned int bctbx_get_code_page(BCTBX_UNUSED(const char *encoding)) {
	bctbx_error("Getting code page is not implemented");
	return 0;
}
