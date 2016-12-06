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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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

#ifdef __clang__
/*in case of compile with -g static inline can produce this type of warning*/
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
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

#if defined(HAVE_LIBAVCODEC_AVCODEC_H)
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
#if HAVE_AVCODEC_OLD_CODEC_IDS
#include <libavcodec/old_codec_ids.h>
#endif


#if LIBAVUTIL_VERSION_MAJOR <= 51
#define AVPixelFormat PixelFormat
#define AV_PIX_FMT_YUVJ420P PIX_FMT_YUVJ420P
#define AV_PIX_FMT_RGBA PIX_FMT_RGBA
#define AV_PIX_FMT_RGB24 PIX_FMT_RGB24
#define AV_PIX_FMT_BGR24 PIX_FMT_BGR24
#define AV_PIX_FMT_YUV420P PIX_FMT_YUV420P
#define AV_PIX_FMT_YUYV422 PIX_FMT_YUYV422
#define AV_PIX_FMT_UYVY422 PIX_FMT_UYVY422
#define AV_PIX_FMT_YUYV422 PIX_FMT_YUYV422
#define AV_PIX_FMT_RGB565 PIX_FMT_RGB565

#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54,25,0)
/*CODEC_ID_* and CodecID have been deprecated for a long time and this release removes it altogether. Please use AV_CODEC_ID_* and AVCodecID instead.*/
#define CodecID AVCodecID
#ifndef HAVE_AVCODEC_OLD_CODEC_IDS
	#define CODEC_ID_H264 AV_CODEC_ID_H264
	#define CODEC_ID_H263 AV_CODEC_ID_H263
	#define CODEC_ID_H263P AV_CODEC_ID_H263P
	#define CODEC_ID_MPEG4 AV_CODEC_ID_MPEG4
	#define CODEC_ID_MJPEG AV_CODEC_ID_MJPEG
#endif
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

#ifndef HAVE_FUN_av_frame_alloc
AVFrame* av_frame_alloc (void);
#elif LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,45,101)
#define av_frame_alloc avcodec_alloc_frame
/*http://git.videolan.org/?p=ffmpeg.git;a=blob;f=doc/APIchanges
 2013-12-11 - 29c83d2 / b9fb59d,409a143 / 9431356,44967ab / d7b3ee9 - lavc 55.45.101 / 55.28.1 - avcodec.h
   av_frame_alloc(), av_frame_unref() and av_frame_free() now can and should be
   used instead of avcodec_alloc_frame(), avcodec_get_frame_defaults() and
   avcodec_free_frame() respectively. The latter three functions are deprecated.
*jehan: previous version (55.39.100 at least) might be buggy */
#endif

#ifndef HAVE_FUN_av_frame_free
void av_frame_free (AVFrame** frame);
#elif LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,45,101)
#define av_frame_free avcodec_free_frame
/*http://git.videolan.org/?p=ffmpeg.git;a=blob;f=doc/APIchanges
 2013-12-11 - 29c83d2 / b9fb59d,409a143 / 9431356,44967ab / d7b3ee9 - lavc 55.45.101 / 55.28.1 - avcodec.h
   av_frame_alloc(), av_frame_unref() and av_frame_free() now can and should be
   used instead of avcodec_alloc_frame(), avcodec_get_frame_defaults() and
   avcodec_free_frame() respectively. The latter three functions are deprecated.
*jehan: previous version (55.39.100 at least) might be buggy */
#endif

#ifndef HAVE_FUN_av_frame_unref
void av_frame_unref (AVFrame *frame);
#elif LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,45,101)
#define av_frame_unref avcodec_get_frame_defaults
/*http://git.videolan.org/?p=ffmpeg.git;a=blob;f=doc/APIchanges
 2013-12-11 - 29c83d2 / b9fb59d,409a143 / 9431356,44967ab / d7b3ee9 - lavc 55.45.101 / 55.28.1 - avcodec.h
   av_frame_alloc(), av_frame_unref() and av_frame_free() now can and should be
   used instead of avcodec_alloc_frame(), avcodec_get_frame_defaults() and
   avcodec_free_frame() respectively. The latter three functions are deprecated.
 *jehan: previous version (55.39.100 at least) might be buggy */
#endif


#endif /*iHAVE_LIBAVCODEC_AVCODEC_H*/

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif /* FFMPEG_PRIV_H */
