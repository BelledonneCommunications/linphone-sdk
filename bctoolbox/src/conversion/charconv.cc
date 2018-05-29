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

#ifdef __APPLE__
   #include "TargetConditionals.h"
#endif

#include "bctoolbox/logging.h"
#include "bctoolbox/port.h"
#include "bctoolbox/charconv.h"

namespace {
	std::string defaultEncoding = "";
}

void bctbx_set_default_encoding (const char *encoding) {
	defaultEncoding = std::string(encoding);
}

const char *bctbx_get_default_encoding () {
	if (!defaultEncoding.empty())
		return defaultEncoding.c_str();

#if defined(__ANDROID__) || TARGET_OS_IPHONE
	return "UTF-8";
#else
	return "locale";
#endif
}
