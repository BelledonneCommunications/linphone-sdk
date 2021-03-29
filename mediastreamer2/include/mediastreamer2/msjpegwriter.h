/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
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

#ifndef msjpegwriter_h
#define msjpegwriter_h

#include <mediastreamer2/msfilter.h>

/*#if defined(__APPLE__)
#include <sys/syslimits.h>
#define MAX_PATH PATH_MAX
#elif defined(__unix__)
#include <limits.h>
#define MAX_PATH PATH_MAX
#else
#include <limits.h>
#endif*/

#define MAX_PATH_TEMP 255

typedef struct _MSJpegWriteEventData {
	char filePath[MAX_PATH_TEMP];
} MSJpegWriteEventData;

#define MS_JPEG_WRITER_TAKE_SNAPSHOT	MS_FILTER_METHOD(MS_JPEG_WRITER_ID, 0, const char)
#define MS_JPEG_WRITER_SNAPSHOT_TAKEN 	MS_FILTER_EVENT(MS_JPEG_WRITER_ID, 0, MSJpegWriteEventData)

#endif
