/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
 *
 * This file is part of mswasapi library - modular sound and video processing and streaming Windows Audio Session API
 * sound card plugin for mediastreamer2 (see https://gitlab.linphone.org/BC/public/mswasapi).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <bctoolbox/utils.hh>
#include <mediastreamer2/mscommon.h>

#define REPORT_ERROR_NOGOTO(msg, result)                                                                               \
	if (result != S_OK) {                                                                                              \
		ms_error(msg, result);                                                                                         \
		if (result == E_OUTOFMEMORY) ms_error(("mswasapi: " + bctoolbox::Utils::getMemoryReportAsString()).c_str());   \
	}
#define REPORT_ERROR(msg, result)                                                                                      \
	if (result != S_OK) {                                                                                              \
		ms_error(msg, result);                                                                                         \
		if (result == E_OUTOFMEMORY) ms_error(("mswasapi: " + bctoolbox::Utils::getMemoryReportAsString()).c_str());   \
		goto error;                                                                                                    \
	}
#define SAFE_RELEASE(obj)                                                                                              \
	if ((obj) != NULL) {                                                                                               \
		(obj)->Release();                                                                                              \
		(obj) = NULL;                                                                                                  \
	}

#define FREE_PTR(ptr)                                                                                                  \
	if (ptr != NULL) {                                                                                                 \
		CoTaskMemFree((LPVOID)ptr);                                                                                    \
		ptr = NULL;                                                                                                    \
	}
