/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef msjpegwriter_h
#define msjpegwriter_h

#include <mediastreamer2/msfilter.h>

#define MS_JPEG_WRITER_TAKE_SNAPSHOT	MS_FILTER_METHOD(MS_JPEG_WRITER_ID, 0, const char)
#define MS_JPEG_WRITER_SNAPSHOT_TAKEN 	MS_FILTER_EVENT(MS_JPEG_WRITER_ID, 0, const char)

#endif
