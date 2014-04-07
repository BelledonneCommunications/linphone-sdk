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
#include "mediastreamer2/msticker.h"
#include "ortp/b64.h"


MSOpenH264Decoder::MSOpenH264Decoder(MSFilter *f)
	: mFilter(f), mDecoder(0), mInitialized(false), mSPS(0), mPPS(0), mYUVMsg(0), mLastErrorReportTime(0), mPacketCount(0),
	mWidth(MS_VIDEO_SIZE_UNKNOWN_W), mHeight(MS_VIDEO_SIZE_UNKNOWN_H), mFirstImageDecoded(false)
{
	long ret = WelsCreateDecoder(&mDecoder);
	if (ret != 0) {
		ms_error("OpenH264 decoder: Failed to create decoder: %l", ret);
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
	mPacketCount = 0;
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
			ms_error("OpenH264 decoder: Failed to initialize: %l", ret);
		} else {
			ms_video_init_average_fps(&mFPS, "OpenH264 decoder: FPS=%f");
			mInitialized = true;
		}
	}
}

void MSOpenH264Decoder::feed()
{
	if (!isInitialized()){
		ms_queue_flush(mFilter->inputs[0]);
		return;
	}

	MSQueue nalus;
	ms_queue_init(&nalus);

	mblk_t *im;
	while ((im = ms_queue_get(mFilter->inputs[0])) != NULL) {
		if ((mPacketCount == 0) && (mSPS != 0) && (mPPS != 0)) {
			// Push the sps/pps given in sprop-parameter-sets if any
			mblk_set_timestamp_info(mSPS, mblk_get_timestamp_info(im));
			mblk_set_timestamp_info(mPPS, mblk_get_timestamp_info(im));
			rfc3984_unpack(&mUnpacker, mSPS, &nalus);
			rfc3984_unpack(&mUnpacker, mPPS, &nalus);
			mSPS = 0;
			mPPS = 0;
		}
		rfc3984_unpack(&mUnpacker, im, &nalus);
		mblk_t *nal;
		while ((nal = ms_queue_get(&nalus)) != NULL) {
			mblk_t *fullNal = addNalMarker(nal);
			void * pData[3] = { 0 };
			SBufferInfo sDstBufInfo = { 0 };
			int len = fullNal->b_wptr - fullNal->b_rptr;
			DECODING_STATE state = mDecoder->DecodeFrame2(fullNal->b_rptr, len, pData, &sDstBufInfo);
			if (state != dsErrorFree) {
				ms_error("OpenH264 decoder: DecodeFrame2 failed: 0x%x", state);
				if (((mFilter->ticker->time - mLastErrorReportTime) > 5000) || (mLastErrorReportTime == 0)) {
					mLastErrorReportTime = mFilter->ticker->time;
					ms_filter_notify_no_arg(mFilter, MS_VIDEO_DECODER_DECODING_ERRORS);
				}
			}
			if (sDstBufInfo.iBufferStatus == 1) {
				uint8_t * pDst[3] = { 0 };
				pDst[0] = (uint8_t *)pData[0];
				pDst[1] = (uint8_t *)pData[1];
				pDst[2] = (uint8_t *)pData[2];

				// Update video size and (re)allocate YUV buffer if needed
				if ((mWidth != sDstBufInfo.UsrData.sSystemBuffer.iWidth)
					|| (mHeight != sDstBufInfo.UsrData.sSystemBuffer.iHeight)) {
					if (mYUVMsg) {
						freemsg(mYUVMsg);
					}
					mWidth = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
					mHeight = sDstBufInfo.UsrData.sSystemBuffer.iHeight;
					mYUVMsg = ms_yuv_buf_alloc(&mOutbuf, mWidth, mHeight);
				}

				// Scale/copy frame to destination mblk_t
				for (int i = 0; i < 3; i++) {
					uint8_t *dst = mOutbuf.planes[i];
					uint8_t *src = pDst[i];
					int h = mHeight >> (( i > 0) ? 1 : 0);

					for(int j = 0; j < h; j++) {
						memcpy(dst, src, mOutbuf.strides[i]);
						dst += mOutbuf.strides[i];
						src += sDstBufInfo.UsrData.sSystemBuffer.iStride[(i == 0) ? 0 : 1];
					}
				}
				ms_queue_put(mFilter->outputs[0], dupmsg(mYUVMsg));

				// Update average FPS
				if (ms_video_update_average_fps(&mFPS, mFilter->ticker->time)) {
					ms_message("OpenH264 decoder: Frame size: %dx%d", mWidth, mHeight);
				}

				// Notify first decoded image
				if (!mFirstImageDecoded) {
					mFirstImageDecoded = true;
					ms_filter_notify_no_arg(mFilter, MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
				}
			}
			freemsg(nal);
			freemsg(fullNal);
		}
		mPacketCount++;
	}
}

void MSOpenH264Decoder::uninitialize()
{
	if (mSPS != 0) {
		freemsg(mSPS);
	}
	if (mPPS != 0) {
		freemsg(mPPS);
	}
	if (mYUVMsg != 0) {
		freemsg(mYUVMsg);
	}
	if (mDecoder != 0) {
		mDecoder->Uninitialize();
	}
	mInitialized = false;
}

void MSOpenH264Decoder::provideSpropParameterSets(char *value, int valueSize)
{
	char *b64_sps = value;
	char *b64_pps = strchr(value, ',');
	if (b64_pps) {
		*b64_pps = '\0';
		++b64_pps;
		ms_message("OpenH264 decoder: Got sprop-parameter-sets sps=%s, pps=%s", b64_sps, b64_pps);
		mSPS = allocb(valueSize, 0);
		mSPS->b_wptr += b64::b64_decode(b64_sps, strlen(b64_sps), mSPS->b_wptr, valueSize);
		mPPS = allocb(valueSize, 0);
		mPPS->b_wptr += b64::b64_decode(b64_pps, strlen(b64_pps), mPPS->b_wptr, valueSize);
	}
}

void MSOpenH264Decoder::resetFirstImageDecoded()
{
	mFirstImageDecoded = false;
	mWidth = MS_VIDEO_SIZE_UNKNOWN_W;
	mHeight = MS_VIDEO_SIZE_UNKNOWN_H;
}

MSVideoSize MSOpenH264Decoder::getSize() const
{
	MSVideoSize size;
	size.width = mWidth;
	size.height = mHeight;
	return size;
}

mblk_t * MSOpenH264Decoder::addNalMarker(mblk_t* im)
{
	uint8_t *src = im->b_rptr;
	if ((src[0] == 0) && (src[1] == 0) && (src[2] == 0) && (src[3] == 1)) {
		// Workaround for stupid RTP H264 sender that includes nal markers
		return copymsg(im);
	} else {
		int len = im->b_wptr - im->b_rptr;
		uint8_t naluType = (*src) & ((1 << 5) - 1);

		// Prepend nal marker
		mblk_t *om = allocb(len * 2, 0);
		uint8_t *dst = om->b_wptr;
		*dst++ = 0;
		*dst++ = 0;
		*dst++ = 1;
		*dst++ = *src++;
		while (src < (im->b_wptr - 3)) {
			if ((src[0] == 0) && (src[1] == 0) && (src[2] < 3)) {
				*dst++ = 0;
				*dst++ = 0;
				*dst++ = 3;
				src += 2;
			}
			*dst++ = *src++;
		}
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		om->b_wptr = dst;

		return om;
	}
}
