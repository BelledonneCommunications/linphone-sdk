/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2016  Belledonne Communications

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

#include "turbojpeg.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <math.h>

mblk_t *jpeg2yuv(uint8_t *jpgbuf, int bufsize, MSVideoSize *reqsize) {
	MSPicture dest;
	mblk_t *m = NULL;
	int imgsrch, imgsrcw, imgsrcsubsamp, imgsrccolor, sfnum, i;
	int scaledw = 0, scaledh = 0;
	tjscalingfactor* sf = NULL;
	tjhandle turbojpegDec = tjInitDecompress();

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

	sf = tjGetScalingFactors(&sfnum);

	// Prevision of height and width
	for(i = 0; i < sfnum; i++) {
		scaledw = TJSCALED(imgsrcw, sf[i]);
		scaledh = TJSCALED(imgsrch, sf[i]);
		if (scaledw <= reqsize->width && scaledh <= reqsize->height)
			break;
	}

	if (scaledw <= 0 && scaledh <= 0) {
		ms_error("No resolution size found for (%ix%i)", reqsize->width, reqsize->height);
		goto clean;
	}

	m = ms_yuv_buf_alloc(&dest, scaledw, scaledh);
	if (m == NULL) goto clean;

	if (tjDecompressToYUVPlanes(
		turbojpegDec,
		jpgbuf,
		bufsize,
		dest.planes,
		dest.w,
		dest.strides,
		dest.h,
		0
		) != 0) {
		ms_error("tjDecompressToYUVPlanes() failed, error: %s", tjGetErrorStr());
		goto clean;
	}

	reqsize->width = scaledw;
	reqsize->height = scaledh;

	clean:
	if (turbojpegDec != NULL) {
		if (tjDestroy(turbojpegDec) != 0)
			ms_error("tjDestroy decompress error: %s", tjGetErrorStr());
	}
	return m;
}

#if __clang__
#pragma clang diagnostic pop
#endif
