/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msvideo.h"

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "turbojpeg.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <math.h>

#define free_and_reset_msg(m) \
	freemsg(m); \
	m = NULL

mblk_t *jpeg2yuv_details(uint8_t *jpgbuf, int bufsize, MSVideoSize *reqsize, tjhandle turbojpegDec, tjhandle yuvEncoder, MSYuvBufAllocator * allocator, uint8_t **gRgbBuf, size_t *gRgbBufLen) {
	MSPicture dest;
	mblk_t *m = NULL;
	uint8_t *rgbBuf = NULL;
	int imgsrch, imgsrcw, imgsrcsubsamp, imgsrccolor, sfnum, i;
	int scaledw = 0, scaledh = 0;
	int requestw, requesth;
	tjscalingfactor* sf = NULL;
	bool_t haveDecoder = (turbojpegDec != NULL);
	bool_t haveEncoder = (yuvEncoder != NULL);
	if( gRgbBuf != NULL)
		rgbBuf = *gRgbBuf;

	if(!haveDecoder)
		turbojpegDec = tjInitDecompress();
	if (turbojpegDec == NULL) {
		ms_error("tjInitDecompress error: %s", tjGetErrorStr());
		return NULL;
	}

	if (tjDecompressHeader3(
		turbojpegDec,
		jpgbuf,
		bufsize,
		&imgsrcw,
		&imgsrch,
		&imgsrcsubsamp,
		&imgsrccolor
		) != 0) {
		ms_error("tjDecompressHeader3() failed, error: %s", tjGetErrorStr());
	}

	if( reqsize->width == MS_VIDEO_SIZE_UNKNOWN_W)
		requestw = imgsrcw;
	else
		requestw = reqsize->width;
	if( reqsize->height == MS_VIDEO_SIZE_UNKNOWN_H)
		requesth = imgsrch;
	else
		requesth = reqsize->height;

	sf = tjGetScalingFactors(&sfnum);

	// Prevision of height and width
	for(i = 0; i < sfnum; i++) {
		scaledw = TJSCALED(imgsrcw, sf[i]);
		scaledh = TJSCALED(imgsrch, sf[i]);
		if (scaledw <= requestw && scaledh <= requesth)
			break;
	}

	if (scaledw <= 0 && scaledh <= 0) {
		ms_error("No resolution size found for (%ix%i)", requestw, requesth);
		goto clean;
	}
	if(allocator != NULL){
		m = ms_yuv_buf_allocator_get(allocator, &dest, scaledw, scaledh);
	}else
		m = ms_yuv_buf_alloc(&dest, scaledw, scaledh);
	if (m == NULL) goto clean;

	if (imgsrccolor == TJCS_YCbCr && imgsrcsubsamp == TJSAMP_420) {
		if (tjDecompressToYUVPlanes(
			turbojpegDec,
			jpgbuf,
			bufsize,
			dest.planes,
			dest.w,
			dest.strides,
			dest.h,
			0
			) <0 && tjGetErrorCode(turbojpegDec) != TJERR_WARNING) {
			ms_error("tjDecompressToYUVPlanes() failed, error: %s", tjGetErrorStr());
			free_and_reset_msg(m);
			goto clean;
		}
	} else {
		size_t pitch = scaledw * tjPixelSize[TJPF_RGB];
		size_t rgbBufSize = pitch * scaledh;
		if(!haveEncoder)
			yuvEncoder = tjInitCompress();
		if (yuvEncoder == NULL) {
			ms_error("tjInitCompress() failed, error: %s", tjGetErrorStr());
			free_and_reset_msg(m);
			goto clean;
		}
		if(gRgbBufLen != NULL) {
			if(*gRgbBufLen < rgbBufSize){
				bctbx_free(rgbBuf);
				rgbBuf = bctbx_new(uint8_t, rgbBufSize);
				*gRgbBufLen = rgbBufSize;
				*gRgbBuf = rgbBuf;
			}
		}else
			rgbBuf = bctbx_new(uint8_t, rgbBufSize);
		if (tjDecompress2(
			turbojpegDec,
			jpgbuf,
			bufsize,
			rgbBuf,
			scaledw,
			(int)pitch,
			scaledh,
			TJPF_RGB,
			0
		) <0 && tjGetErrorCode(turbojpegDec) != TJERR_WARNING) {
			ms_error("tjDecompress2() failed, error: %s", tjGetErrorStr());
			free_and_reset_msg(m);
			goto clean;
		}
		
		if (tjEncodeYUVPlanes(
			yuvEncoder,
			rgbBuf,
			scaledw,
			(int)pitch,
			scaledh,
			TJPF_RGB,
			dest.planes,
			dest.strides,
			TJSAMP_420,
			0
		) <0 && tjGetErrorCode(yuvEncoder) != TJERR_WARNING) {
			ms_error("tjEncodeYUVPlanes() failed, error: %s", tjGetErrorStr());
			free_and_reset_msg(m);
			goto clean;
		}
	}

	reqsize->width = scaledw;
	reqsize->height = scaledh;

	clean:

	if (!haveEncoder && yuvEncoder != NULL) {
		if (tjDestroy(yuvEncoder) != 0) {
			ms_error("YUV encoder destroying failed: %s", tjGetErrorStr());
		}
	}

	if (!haveDecoder && turbojpegDec != NULL) {
		if (tjDestroy(turbojpegDec) != 0)
			ms_error("tjDestroy decompress error: %s", tjGetErrorStr());
	}

	if (gRgbBufLen == NULL && rgbBuf != NULL) bctbx_free(rgbBuf);
	return m;
}

mblk_t *jpeg2yuv(uint8_t *jpgbuf, int bufsize, MSVideoSize *reqsize) {
	return jpeg2yuv_details(jpgbuf,bufsize,reqsize, NULL, NULL, NULL, NULL, NULL);
}
#if __clang__
#pragma clang diagnostic pop
#endif
