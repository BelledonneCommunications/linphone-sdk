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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "msopenh264dec.h"

#include "bctoolbox/crypto.h"
#include "mediastreamer2/msticker.h"
#include "wels/codec_ver.h"

static void decoder_log(void *context, int level, const char *message) {
  ms_message("OpenH264 decoder: %s", message);
}

MSOpenH264Decoder::MSOpenH264Decoder(MSFilter *f)
    : mFilter(f), mDecoder(0), mUnpacker(0), mSPS(0), mPPS(0), mYUVMsg(0),
      mBitstream(0), mBitstreamSize(65536), mLastErrorReportTime(0),
      mWidth(MS_VIDEO_SIZE_UNKNOWN_W), mHeight(MS_VIDEO_SIZE_UNKNOWN_H),
      mInitialized(false), mFirstImageDecoded(false) {
  long ret = WelsCreateDecoder(&mDecoder);
  if (ret != 0) {
    ms_error("OpenH264 decoder: Failed to create decoder: %li", ret);
  } else {
    mBitstream = static_cast<uint8_t *>(ms_malloc0(mBitstreamSize));
    WelsTraceCallback cb = &decoder_log;
    mDecoder->SetOption(DECODER_OPTION_TRACE_CALLBACK, (void *)&cb);
    int logLevel = WELS_LOG_WARNING;
    mDecoder->SetOption(DECODER_OPTION_TRACE_LEVEL, &logLevel);
  }
}

MSOpenH264Decoder::~MSOpenH264Decoder() {
  if (mBitstream != 0) {
    ms_free(mBitstream);
  }
  if (mDecoder != 0) {
    WelsDestroyDecoder(mDecoder);
  }
}

void MSOpenH264Decoder::initialize() {
  if (!mInitialized) {
    mFirstImageDecoded = false;
    mUnpacker = rfc3984_new_with_factory(mFilter->factory);
    if (mDecoder != 0) {
      SDecodingParam params = {0};
#if (OPENH264_MAJOR == 1) && (OPENH264_MINOR < 6)
      params.eOutputColorFormat = videoFormatI420;
#endif
      params.uiTargetDqLayer = (unsigned char)-1;
      params.eEcActiveIdc = ERROR_CON_FRAME_COPY_CROSS_IDR;
      params.sVideoProperty.size = sizeof(params.sVideoProperty);
      params.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
      long ret = mDecoder->Initialize(&params);
      if (ret != 0) {
        ms_error("OpenH264 decoder: Failed to initialize: %li", ret);
      } else {
        ms_average_fps_init(&mFPS, "OpenH264 decoder: FPS=%f");
        mInitialized = true;
      }
    }
  }
}

void MSOpenH264Decoder::feed() {
  if (!isInitialized()) {
    ms_error("MSOpenH264Decoder::feed(): not initialized");
    ms_queue_flush(mFilter->inputs[0]);
    return;
  }

  MSQueue nalus;
  ms_queue_init(&nalus);

  mblk_t *im;
  bool requestPLI = false;
  while ((im = ms_queue_get(mFilter->inputs[0])) != NULL) {
    unsigned int ret;
    if ((getIDRPicId() == 0) && (mSPS != 0) && (mPPS != 0)) {
      // Push the sps/pps given in sprop-parameter-sets if any
      rfc3984_unpack_out_of_band_sps_pps(mUnpacker, mSPS, mPPS);
      mSPS = NULL;
      mPPS = NULL;
    }
    ret = rfc3984_unpack2(mUnpacker, im, &nalus);
    if (ret & Rfc3984FrameAvailable) {
      void *pData[3] = {0};
      SBufferInfo sDstBufInfo = {0};
      int len = nalusToFrame(&nalus);

      if (ret & Rfc3984FrameCorrupted)
        requestPLI = true;

      DECODING_STATE state = mDecoder->DecodeFrame2(
          mBitstream, len, (uint8_t **)pData, &sDstBufInfo);
      if (state != dsErrorFree) {
        ms_error("OpenH264 decoder: DecodeFrame2 failed: 0x%x", (int)state);
        requestPLI = true;
      }
      if (sDstBufInfo.iBufferStatus == 1) {
        uint8_t *pDst[3] = {0};
        pDst[0] = (uint8_t *)pData[0];
        pDst[1] = (uint8_t *)pData[1];
        pDst[2] = (uint8_t *)pData[2];

        // Update video size and (re)allocate YUV buffer if needed
        if ((mWidth != sDstBufInfo.UsrData.sSystemBuffer.iWidth) ||
            (mHeight != sDstBufInfo.UsrData.sSystemBuffer.iHeight)) {
          if (mYUVMsg) {
            freemsg(mYUVMsg);
          }
          mWidth = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
          mHeight = sDstBufInfo.UsrData.sSystemBuffer.iHeight;
          mYUVMsg = ms_yuv_buf_alloc(&mOutbuf, mWidth, mHeight);
          ms_filter_notify_no_arg(mFilter, MS_FILTER_OUTPUT_FMT_CHANGED);
        }

        // Scale/copy frame to destination mblk_t
        for (int i = 0; i < 3; i++) {
          uint8_t *dst = mOutbuf.planes[i];
          uint8_t *src = pDst[i];
          int h = mHeight >> ((i > 0) ? 1 : 0);

          for (int j = 0; j < h; j++) {
            memcpy(dst, src, mOutbuf.strides[i]);
            dst += mOutbuf.strides[i];
            src += sDstBufInfo.UsrData.sSystemBuffer.iStride[(i == 0) ? 0 : 1];
          }
        }
        ms_queue_put(mFilter->outputs[0], dupmsg(mYUVMsg));

        // Update average FPS
        if (ms_average_fps_activity(&mFPS, mFilter->ticker->time, TRUE)) {
          ms_message("OpenH264 decoder: Frame size: %dx%d", mWidth, mHeight);
        }

        // Notify first decoded image
        if (!mFirstImageDecoded) {
          mFirstImageDecoded = true;
          ms_filter_notify_no_arg(mFilter,
                                  MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
        }

#if MSOPENH264_DEBUG
        ms_message("OpenH264 decoder: IDR pic id: %d, Frame num: %d, Temporal "
                   "id: %d, VCL NAL: %d",
                   getIDRPicId(), getFrameNum(), getTemporalId(), getVCLNal());
#endif
      }
    }
  }

  if (requestPLI) {
    if (mAVPFEnabled) {
      ms_filter_notify_no_arg(mFilter, MS_VIDEO_DECODER_SEND_PLI);
    } else if (((mFilter->ticker->time - mLastErrorReportTime) > 5000) ||
               (mLastErrorReportTime == 0)) {
      mLastErrorReportTime = mFilter->ticker->time;
      ms_filter_notify_no_arg(mFilter, MS_VIDEO_DECODER_DECODING_ERRORS);
    }
  }
}

void MSOpenH264Decoder::uninitialize() {
  if (mSPS != 0) {
    freemsg(mSPS);
    mSPS = NULL;
  }
  if (mPPS != 0) {
    freemsg(mPPS);
    mPPS = NULL;
  }
  if (mYUVMsg != 0) {
    freemsg(mYUVMsg);
    mYUVMsg = NULL;
  }
  if (mDecoder != 0) {
    mDecoder->Uninitialize();
  }
  if (mUnpacker) {
    rfc3984_destroy(mUnpacker);
    mUnpacker = NULL;
  }
  mInitialized = false;
}

void MSOpenH264Decoder::provideSpropParameterSets(char *value,
                                                  size_t valueSize) {
  char *b64_sps = value;
  char *b64_pps = strchr(value, ',');
  if (b64_pps) {
    *b64_pps = '\0';
    ++b64_pps;
    ms_message("OpenH264 decoder: Got sprop-parameter-sets sps=%s, pps=%s",
               b64_sps, b64_pps);
    mSPS = allocb(valueSize, 0);
    bctbx_base64_decode(mSPS->b_wptr, &valueSize, (unsigned char *)b64_sps,
                        strlen(b64_sps));
    mSPS->b_wptr += valueSize;
    mPPS = allocb(valueSize, 0);
    bctbx_base64_decode(mPPS->b_wptr, &valueSize, (unsigned char *)b64_pps,
                        strlen(b64_pps));
    mPPS->b_wptr += valueSize;
  }
}

void MSOpenH264Decoder::resetFirstImageDecoded() {
  mFirstImageDecoded = false;
  mWidth = MS_VIDEO_SIZE_UNKNOWN_W;
  mHeight = MS_VIDEO_SIZE_UNKNOWN_H;
}

MSVideoSize MSOpenH264Decoder::getSize() const {
  MSVideoSize size;
  size.width = mWidth;
  size.height = mHeight;
  return size;
}

float MSOpenH264Decoder::getFps() const { return ms_average_fps_get(&mFPS); }

const MSFmtDescriptor *MSOpenH264Decoder::getOutFmt() const {
  MSVideoSize vsize = {mWidth, mHeight};
  return ms_factory_get_video_format(mFilter->factory, "YUV420P", vsize, 0,
                                     NULL);
}

int MSOpenH264Decoder::nalusToFrame(MSQueue *nalus) {
	mblk_t *im;
	uint8_t *dst = mBitstream;
	uint8_t *end = mBitstream + mBitstreamSize;
	bool startPicture = true;

	while ((im = ms_queue_get(nalus)) != NULL) {
		uint8_t *src = im->b_rptr;
		int nalLen = im->b_wptr - src;
		if (nalLen == 0) {
			freemsg(im);
			continue;
		}
		// Already have a NAL: Annex-B/EBSP. No need to convert.
		if ((nalLen >= 4 && src[0] == 0 && src[1] == 0 && src[2] == 0 && src[3] == 1)
			|| nalLen >= 3 && src[0] == 0 && src[1] == 0 && src[2] == 1
		) {
			if ((dst + nalLen ) > end) {
				int pos = dst - mBitstream;
				enlargeBitstream(mBitstreamSize + nalLen );
				dst = mBitstream + pos;
				end = mBitstream + mBitstreamSize;
			}
#if MSOPENH264_DEBUG
			ms_warning("OpenH264 decoder: RTP H264 encoder");
#endif
			memcpy(dst, src, nalLen);
			dst += nalLen;
		} else {
			uint8_t naluType = *src & 0x1f;
#if MSOPENH264_DEBUG
			if ((naluType != 1) && (naluType != 7) && (naluType != 8)) {
				ms_message("OpenH264 decoder: naluType=%d", naluType);
			}
			if (naluType == 7) {
				ms_message("OpenH264 decoder: Got SPS");
			}
			if (naluType == 8) {
				ms_message("OpenH264 decoder: Got PPS");
			}
#endif
			// Without NAL, we have RBSP : add a start code and escape.
			// Destination size need enough space : Each pair of 00 can lead to add another byte for escaping
			size_t maxNewSize = nalLen + nalLen / 2 + 4;//  CurrentSize + Escaping + StartCode

			if ((dst + maxNewSize ) > end) {
				int pos = dst - mBitstream;
				enlargeBitstream(mBitstreamSize + maxNewSize );
				dst = mBitstream + pos;
				end = mBitstream + mBitstreamSize;
			}

			if (startPicture || (naluType == 6)              // SEI
					|| (naluType == 7)                           // SPS
					|| (naluType == 8)                           // PPS
					|| ((naluType >= 14) && (naluType <= 18))) { // Reserved
				*dst++ = 0;// 4 bytes
				startPicture = false;
			}

			// Annex-B start code.
			*dst++ = 0x00;
			*dst++ = 0x00;
			*dst++ = 0x01;
			// NAL header. Do not encode it.
			*dst++ = *src++;

			// RBSP -> EBSP = Insert 03 for escaping if a pair of 0 and a value lesser than 3
			int zeroCount = 0;
			while (src < im->b_wptr) {
				uint8_t value = *src++;
				if (zeroCount == 2 && value <= 3) {
					*dst++ = 3;
					zeroCount = 0;
				}
				*dst++ = value;
				if (value == 0)
					++zeroCount;
				else
					zeroCount = 0;
			}

		}
		freemsg(im);
	}
	return dst - mBitstream;
}


void MSOpenH264Decoder::enlargeBitstream(int newSize) {
  mBitstreamSize = newSize;
  mBitstream = static_cast<uint8_t *>(ms_realloc(mBitstream, mBitstreamSize));
}

int32_t MSOpenH264Decoder::getFrameNum() {
  int32_t frameNum = -1;
  int ret = mDecoder->GetOption(DECODER_OPTION_FRAME_NUM, &frameNum);
  if (ret != 0) {
    ms_error("OpenH264 decoder: Failed getting frame number: %d", ret);
  }
  return frameNum;
}

int32_t MSOpenH264Decoder::getIDRPicId() {
  int32_t IDRPicId = -1;
  int ret = mDecoder->GetOption(DECODER_OPTION_IDR_PIC_ID, &IDRPicId);
  if (ret != 0) {
    ms_error("OpenH264 decoder: Failed getting IDR pic id: %d", ret);
  }
  return IDRPicId;
}

int32_t MSOpenH264Decoder::getTemporalId() {
  int32_t temporalId = -1;
  int ret = mDecoder->GetOption(DECODER_OPTION_TEMPORAL_ID, &temporalId);
  if (ret != 0) {
    ms_error("OpenH264 decoder: Failed getting temporal id: %d", ret);
  }
  return temporalId;
}

int32_t MSOpenH264Decoder::getVCLNal() {
  int32_t vclNal = -1;
  int ret = mDecoder->GetOption(DECODER_OPTION_VCL_NAL, &vclNal);
  if (ret != 0) {
    ms_error("OpenH264 decoder: Failed getting VCL NAL: %d", ret);
  }
  return vclNal;
}
