/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2013 Belledonne Communications, Grenoble

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#ifndef PRIVATE_H
#define PRIVATE_H

#include "mediastreamer2/mediastream.h"


#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(__WIN32__)
#ifdef MEDIASTREAMER2_INTERNAL_EXPORTS
#define MEDIASTREAMER2_INTERNAL_EXPORT __declspec(dllexport)
#define MEDIASTREAMER2_INTERNAL_VAR_EXPORT __declspec(dllexport)
#else
#define MEDIASTREAMER2_INTERNAL_EXPORT
#define MEDIASTREAMER2_INTERNAL_VAR_EXPORT extern __declspec(dllimport)
#endif
#else
#define MEDIASTREAMER2_INTERNAL_EXPORT extern
#define MEDIASTREAMER2_INTERNAL_VAR_EXPORT extern
#endif


#define MAX_RTP_SIZE	UDP_MAX_SIZE


MSTickerPrio __ms_get_default_prio(bool_t is_video);

MEDIASTREAMER2_INTERNAL_EXPORT RtpSession * create_duplex_rtpsession(int loc_rtp_port, int loc_rtcp_port, bool_t ipv6);

void start_ticker(MediaStream *stream);

void mediastream_payload_type_changed(RtpSession *session, unsigned long data);

const char * media_stream_type_str(MediaStream *stream);

void media_stream_free(MediaStream *stream);

#endif /* PRIVATE_H */
