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

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#include "bctoolbox/charconv.h"
#include "bctoolbox/logging.h"
#include "bctoolbox/port.h"

#include <cstdlib>
#include <string>

namespace {
std::string &defaultEncodingPrivate() {
	static std::string defaultEncoding;
	return defaultEncoding;
}
} // namespace

void bctbx_set_default_encoding(const char *encoding) {
	defaultEncodingPrivate() = encoding;
}

const char *bctbx_get_default_encoding(void) {
	if (!defaultEncodingPrivate().empty()) return defaultEncodingPrivate().c_str();

#if defined(__ANDROID__) || TARGET_OS_IPHONE
	return "UTF-8";
#else
	return "locale";
#endif
}

wchar_t *bctbx_string_to_wide_string(const char *str) {
	wchar_t *wstr;
	size_t sz = mbstowcs(NULL, str, 0);
	if (sz == (size_t)-1) {
		return NULL;
	}
	sz += 1;
	wstr = (wchar_t *)bctbx_malloc(sz * sizeof(wchar_t));
	sz = mbstowcs(wstr, str, sz);
	if (sz == (size_t)-1) {
		bctbx_free(wstr);
		return NULL;
	}
	return wstr;
}

char *bctbx_wide_string_to_string(const wchar_t *wstr) {
	size_t sz;
	char *str;
	sz = wcstombs(NULL, wstr, 0);
	if (sz == (size_t)-1) {
		return NULL;
	}
	sz += 1;
	str = (char *)bctbx_malloc(sz);
	sz = wcstombs(str, wstr, sz);
	if (sz == (size_t)-1) {
		bctbx_free(str);
		return NULL;
	}
	return str;
}
