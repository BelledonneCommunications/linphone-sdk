/*
 * utils.cpp
 * Copyright (C) 2010-2017 Belledonne Communications SARL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "core/platform-helpers/platform-helpers.h"
#include "paths.h"

#ifdef __APPLE__
#include "paths-apple.h"
#elif defined(__ANDROID__)
#include "paths-android.h"
#elif defined(_WIN32)
#include "paths-windows.h"
#elif defined(__linux)
#include "paths-linux.h"
#else
#error "Unsupported system"
#endif

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

const string &Paths::getPath (Paths::Type type, PlatformHelpers *platformHelper) {
	switch (type) {
		case Data:
			return SysPaths::getDataPath(platformHelper);
		case Config:
		default:
			return SysPaths::getConfigPath(platformHelper);
	}
}

LINPHONE_END_NAMESPACE
