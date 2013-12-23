/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#ifndef FFMPEG_PRIV_H
#define FFMPEG_PRIV_H

#ifdef __cplusplus
/*see http://linux.die.net/man/3/uint64_c */
#define __STDC_CONSTANT_MACROS 1
#endif

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include <ortp/port.h>

#if defined(HAVE_LIBAVCODEC_AVCODEC_H)
/* new layout */
# include <libavcodec/avcodec.h>
# include <libavutil/avutil.h>
# include <libavutil/mem.h>
#endif

#if defined(HAVE_LIBSWSCALE_SWSCALE_H)
/* new layout */
#  include <libswscale/swscale.h>
#endif

#if HAVE_LIBAVCODEC_AVCODEC_H
#if LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(52,24,0)
/*should work as long as nobody uses avformat.h*/
typedef struct AVPacket{
	uint8_t *data;
	int size;
}AVPacket;

static inline void av_init_packet(AVPacket *pkt){
	
}
static inline int avcodec_decode_video2(AVCodecContext *avctx, AVFrame *picture,
                         int *got_picture_ptr,
                         AVPacket *avpkt){
	return avcodec_decode_video(avctx,picture, got_picture_ptr,avpkt->data,avpkt->size);
}
#endif

#if (LIBAVCODEC_VERSION_MAJOR >= 56)
#include <libavcodec/old_codec_ids.h>
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54,25,0)
#define CodecID AVCodecID
#endif

#ifndef HAVE_FUN_avcodec_encode_video2
int avcodec_encode_video2 (AVCodecContext *avctx, AVPacket *avpkt, const AVFrame *frame, int *got_packet_ptr);
#endif

#ifndef HAVE_FUN_avcodec_get_context_defaults3 /**/
int avcodec_get_context_defaults3 (AVCodecContext *s, const AVCodec *codec);
AVCodecContext *avcodec_alloc_context3(AVCodec *codec);
#endif

#ifndef HAVE_FUN_avcodec_open2 /**/
int avcodec_open2 (AVCodecContext *avctx, const AVCodec *codec, /*AVDictionary*/ void **options);
#endif

#endif /*iHAVE_LIBAVCODEC_AVCODEC_H*/
#endif /* FFMPEG_PRIV_H */
