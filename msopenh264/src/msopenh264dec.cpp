/*
H.264 encoder/decoder plugin for mediastreamer2 based on the openh264 library.
Copyright (C) 2006-2012 Belledonne Communications, Grenoble

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


#include "msopenh264dec.h"


MSOpenH264Decoder::MSOpenH264Decoder()
	: mDecoder(0), mInitialized(false), mFirstImageDecoded(false)
{
	long ret = WelsCreateDecoder(&mDecoder);
	if (ret != 0) {
		ms_error("%s: Failed creating openh264 decoder: %d", __FUNCTION__, ret);
	}
}

MSOpenH264Decoder::~MSOpenH264Decoder()
{
	if (mDecoder != 0) {
		WelsDestroyDecoder(mDecoder);
	}
}

void MSOpenH264Decoder::initialize()
{
	mFirstImageDecoded = false;
	rfc3984_init(&mUnpacker);
	if (mDecoder != 0) {
		SDecodingParam params = { 0 };
		params.iOutputColorFormat = videoFormatI420;
		params.uiTargetDqLayer = (unsigned char) -1;
		params.uiEcActiveFlag = 1;
		params.sVideoProperty.size = sizeof(params.sVideoProperty);
		params.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
		long ret = mDecoder->Initialize(&params);
		if (ret != 0) {
			ms_error("%s: Failed to initialize openh264 decoder: %d", __FUNCTION__, ret);
		} else {
			mInitialized = true;
		}
	}
}

void MSOpenH264Decoder::feed(MSFilter *f)
{
	if (!isInitialized()){
		ms_queue_flush(f->inputs[0]);
		return;
	}

	MSQueue nalus;
	ms_queue_init(&nalus);

	mblk_t *im;
	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		rfc3984_unpack(&mUnpacker, im, &nalus);
		if (!ms_queue_empty(&nalus)) {
			ms_message("nalus");
// 			void * pData[3] = { 0 };
// 			SBufferInfo sDstBufInfo = { 0 };
// 			mDecoder->DecodeFrame2 (pBuf + iBufPos, iSliceSize, pData, &sDstBufInfo);
// 			if (sDstBufInfo.iBufferStatus == 1) {
// 				uint8_t * pDst[3] = { 0 };
// 				pDst[0] = (uint8_t *)pData[0];
// 				pDst[1] = (uint8_t *)pData[1];
// 				pDst[2] = (uint8_t *)pData[2];
// 			}
		}
	}
}

void MSOpenH264Decoder::uninitialize()
{
	if (mDecoder != 0) {
		mDecoder->Uninitialize();
	}
	mInitialized = false;
}
