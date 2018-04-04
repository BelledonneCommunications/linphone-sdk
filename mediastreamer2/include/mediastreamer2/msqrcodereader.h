/*
 * mediastreamer2 library - modular sound and video processing and streaming
 * Copyright (C) 2018  Belledonne Communications SARL (simon.morlat@linphone.org)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef msqrcodereader_h
#define msqrcodereader_h

#include <mediastreamer2/msfilter.h>

#define MS_QRCODE_READER_QRCODE_FOUND	MS_FILTER_EVENT(MS_QRCODE_READER_ID, 0, const char*)
#define MS_QRCODE_READER_RESET_SEARCH	MS_FILTER_METHOD_NO_ARG(MS_QRCODE_READER_ID, 0)
#define MS_QRCODE_READET_SET_DECODER_RECT	MS_FILTER_METHOD(MS_QRCODE_READER_ID, 1, MSRect)

#endif

