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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#ifndef PRIVATE_H
#define PRIVATE_H

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msvideopresets.h"


#if defined(_WIN32) || defined(_WIN32) || defined(__WIN32) || defined(__WIN32__)
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


#ifdef __cplusplus
extern "C"
{
#endif

MSTickerPrio __ms_get_default_prio(bool_t is_video);

void media_stream_start_ticker(MediaStream *stream);

const char * media_stream_type_str(MediaStream *stream);

void media_stream_free(MediaStream *stream);

/**
 * Ask the video stream to send a Picture Loss Indication.
 * @param[in] stream The videostream object.
 */
MS2_PUBLIC void video_stream_send_pli(VideoStream *stream);

/**
 * Ask the video stream to send a Slice Loss Indication.
 * @param[in] stream The videostream object.
 * @param[in] first The address of the first lost macroblock.
 * @param[in] number The number of lost macroblocks.
 * @param[in] picture_id The six least significant bits of the picture ID.
 */
MS2_PUBLIC void video_stream_send_sli(VideoStream *stream, uint16_t first, uint16_t number, uint8_t picture_id);

/**
 * Ask the video stream to send a Reference Picture Selection Indication.
 * @param[in] stream The videostream object.
 * @param[in] bit_string A pointer to the variable length native RPSI bit string to include in the RTCP FB message.
 * @param[in] bit_string_len The length of the bit_string in bits.
 */
MS2_PUBLIC void video_stream_send_rpsi(VideoStream *stream, uint8_t *bit_string, uint16_t bit_string_len);


void video_stream_open_player(VideoStream *stream, MSFilter *sink);

void video_stream_close_player(VideoStream *stream);

/**
 * Initialise srtp library, shall be called once but multiple call is supported
 * @return 0 on success, error code from srtp/crypto/include/err.h otherwise
 */
MS2_PUBLIC int ms_srtp_init(void);

/**
 * Shutdown the srtp library
 */
MS2_PUBLIC void ms_srtp_shutdown(void);

/**
 * Set the backlink in dtls_context to stream sessions context. Used when reinvite force creation of a new stream with same session data
 * @param[in/out]	dtls_context	Dtls context, contains a link to stream session context needed to access srtp context
 * @param[in]		stream_sessions	Pointer to the new stream session structure
 */
MS2_PUBLIC void ms_dtls_srtp_set_stream_sessions(MSDtlsSrtpContext *dtls_context, MSMediaStreamSessions *stream_sessions);
/**
 * Set the backlink in zrtp_context to stream sessions context. Used when reinvite force creation of a new stream with same session data
 * @param[in/out]	zrtp_context	ZRTP context, contains a link to stream session context needed to access srtp context
 * @param[in]		stream_sessions	Pointer to the new stream session structure
 */
MS2_PUBLIC void ms_zrtp_set_stream_sessions(MSZrtpContext *zrtp_context, MSMediaStreamSessions *stream_sessions);

bool_t ms_media_stream_sessions_secured(const MSMediaStreamSessions *sessions,MediaStreamDir dir);

MSSrtpCtx* ms_srtp_context_new(void);
void ms_srtp_context_delete(MSSrtpCtx *session);


void register_video_preset_high_fps(MSVideoPresetsManager *manager);

MSFilter *_ms_create_av_player(const char *filename, MSFactory* factory);
void video_recorder_handle_event(void *userdata, MSFilter *recorder, unsigned int event, void *event_arg);


#ifdef __cplusplus
}
#endif

#endif /* PRIVATE_H */
