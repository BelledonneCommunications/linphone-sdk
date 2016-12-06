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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msvideo.h"

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifndef NO_FFMPEG
#include "ffmpeg-priv.h"
#else
#define FF_INPUT_BUFFER_PADDING_SIZE 32
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 57

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#endif

#if TARGET_OS_IPHONE
#include <CoreGraphics/CGDataProvider.h>
#include <CoreGraphics/CGImage.h>
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGBitmapContext.h>
#endif

#ifdef _WIN32
#include <fcntl.h>
#include <sys/types.h>
#include <io.h>
#include <stdio.h>
#include <malloc.h>
#endif

mblk_t *jpeg2yuv(uint8_t *jpgbuf, int bufsize, MSVideoSize *reqsize){
#if !defined(NO_FFMPEG) && !TARGET_OS_IPHONE /* this code must never be used for iOS */
	AVCodecContext av_context;
	int got_picture=0;
	mblk_t *ret;
	struct SwsContext *sws_ctx;
	AVPacket pkt;
	MSPicture dest;
	AVCodec *codec=avcodec_find_decoder(CODEC_ID_MJPEG);
	AVFrame* orig = av_frame_alloc();

	if (codec==NULL){
		ms_error("Could not find MJPEG decoder in ffmpeg.");
		return NULL;
	}

	avcodec_get_context_defaults3(&av_context,NULL);
	if (avcodec_open2(&av_context,codec,NULL)<0){
		ms_error("jpeg2yuv: avcodec_open failed");
		return NULL;
	}
	av_init_packet(&pkt);
	pkt.data=jpgbuf;
	pkt.size=bufsize;

	if (avcodec_decode_video2(&av_context,orig,&got_picture,&pkt) < 0) {
		ms_error("jpeg2yuv: avcodec_decode_video failed");
		avcodec_close(&av_context);
		return NULL;
	}
	ret=ms_yuv_buf_alloc(&dest, reqsize->width,reqsize->height);
	/* not using SWS_FAST_BILINEAR because it doesn't play well with
	 * av_context.pix_fmt set to AV_PIX_FMT_YUVJ420P by jpeg decoder */
	sws_ctx=sws_getContext(av_context.width,av_context.height,av_context.pix_fmt,
		reqsize->width,reqsize->height,AV_PIX_FMT_YUV420P,SWS_BILINEAR,
                NULL, NULL, NULL);
	if (sws_ctx==NULL) {
		ms_error("jpeg2yuv: ms_sws_getContext() failed.");
		avcodec_close(&av_context);
		freemsg(ret);
		return NULL;
	}

#if LIBSWSCALE_VERSION_INT >= AV_VERSION_INT(0,9,0)
	if (sws_scale(sws_ctx,(const uint8_t* const *)orig->data,orig->linesize,0,av_context.height,dest.planes,dest.strides)<0){
#else
	if (sws_scale(sws_ctx,(uint8_t**)orig->data,orig->linesize,0,av_context.height,dest.planes,dest.strides)<0){
#endif
		ms_error("jpeg2yuv: ms_sws_scale() failed.");
		sws_freeContext(sws_ctx);
		avcodec_close(&av_context);
		freemsg(ret);
		return NULL;
	}
	sws_freeContext(sws_ctx);
	av_frame_free(&orig);
	avcodec_close(&av_context);
	return ret;
#elif TARGET_OS_IPHONE
	MSPicture dest;
	CGDataProviderRef dataProvider = CGDataProviderCreateWithData(NULL, jpgbuf, bufsize, NULL);
	// use the data provider to get a CGImage; release the data provider
	CGImageRef image = CGImageCreateWithJPEGDataProvider(dataProvider, NULL, FALSE,
						kCGRenderingIntentDefault);
						CGDataProviderRelease(dataProvider);
	reqsize->width = CGImageGetWidth(image);
	reqsize->height = CGImageGetHeight(image);

	uint8_t* tmp = (uint8_t*) malloc(reqsize->width * reqsize->height * 4);
	mblk_t* ret=ms_yuv_buf_alloc(&dest, reqsize->width, reqsize->height);
	CGColorSpaceRef colourSpace = CGColorSpaceCreateDeviceRGB();
	CGContextRef imageContext =
	CGBitmapContextCreate(tmp, reqsize->width, reqsize->height, 8, reqsize->width*4, colourSpace, kCGImageAlphaNoneSkipLast);
	CGColorSpaceRelease(colourSpace);
	// draw the image to the context, release it
	CGContextDrawImage(imageContext, CGRectMake(0, 0, reqsize->width, reqsize->height), image);
	CGImageRelease(image);

	/* convert tmp/RGB -> ret/YUV */
	for(int y=0; y<reqsize->height; y++) {
		for(int x=0; x<reqsize->width; x++) {
			uint8_t r = tmp[y * reqsize->width * 4 + x * 4 + 0];
			uint8_t g = tmp[y * reqsize->width * 4 + x * 4 + 1];
			uint8_t b = tmp[y * reqsize->width * 4 + x * 4 + 2];

			// Y
			*dest.planes[0]++ = (uint8_t)((0.257 * r) + (0.504 * g) + (0.098 * b) + 16);

			// U/V subsampling
			if ((y % 2==0) && (x%2==0)) {
				uint32_t r32=0, g32=0, b32=0;
				for(int i=0; i<2; i++) {
					for(int j=0; j<2; j++) {
						r32 += tmp[(y+i) * reqsize->width * 4 + (x+j) * 4 + 0];
						g32 += tmp[(y+i) * reqsize->width * 4 + (x+j) * 4 + 1];
						b32 += tmp[(y+i) * reqsize->width * 4 + (x+j) * 4 + 2];
					}
				}
				r32 = (uint32_t)(r32 * 0.25f); g32 = (uint32_t)(g32 * 0.25f); b32 = (uint32_t) (b32 * 0.25f);

				// U
				*dest.planes[1]++ = (uint8_t)(-(0.148 * r32) - (0.291 * g32) + (0.439 * b32) + 128);
				// V
				*dest.planes[2]++ = (uint8_t)((0.439 * r32) - (0.368 * g32) - (0.071 * b32) + 128);
			}
		}
	}
	free(tmp);
	return ret;
#else
	return NULL;
#endif
}

#if __clang__
#pragma clang diagnostic pop
#endif
