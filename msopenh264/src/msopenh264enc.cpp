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


#include "msopenh264enc.h"
#include "mediastreamer2/msticker.h"
#include "wels/codec_app_def.h"


static const int RC_MARGIN = 10000; // bits per sec


VideoStarter::VideoStarter()
	: mNextTime(0), mFrameCount(0)
{}

VideoStarter::~VideoStarter()
{}

void VideoStarter::firstFrame(uint64_t curtime)
{
	mNextTime = curtime + 2000;
}

bool VideoStarter::needIFrame(uint64_t curtime)
{
	if (mNextTime == 0) return false;
	if (curtime >= mNextTime) {
		mFrameCount++;
		if (mFrameCount == 1) {
			mNextTime += 2000;
		} else {
			mNextTime = 0;
		}
		return true;
	}
	return false;
}


#define MS_OPENH264_CONF(required_bitrate, bitrate_limit, resolution, fps) \
	{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H }, fps, NULL }


static const MSVideoConfiguration openh264_conf_list[] = {
#if defined(ANDROID) || (TARGET_OS_IPHONE == 1) || defined(__arm__)
	MS_OPENH264_CONF(0, 512000, QVGA, 12)
#else
	MS_OPENH264_CONF(1024000, 1536000, SVGA, 25),
	MS_OPENH264_CONF( 512000, 1024000,  VGA, 25),
	MS_OPENH264_CONF( 256000,  512000,  VGA, 15),
	MS_OPENH264_CONF( 170000,  256000, QVGA, 15),
	MS_OPENH264_CONF( 128000,  170000, QCIF, 10),
	MS_OPENH264_CONF(  64000,  128000, QCIF,  7),
	MS_OPENH264_CONF(      0,   64000, QCIF,  5)
#endif
};

static const MSVideoConfiguration multicore_openh264_conf_list[] = {
#if defined(ANDROID) || (TARGET_OS_IPHONE == 1) || defined(__arm__)
	MS_OPENH264_CONF(2048000, 3072000,       UXGA, 15),
	MS_OPENH264_CONF(1024000, 1536000, SXGA_MINUS, 15),
	MS_OPENH264_CONF( 750000, 1024000,        XGA, 15),
	MS_OPENH264_CONF( 500000,  750000,       SVGA, 15),
	MS_OPENH264_CONF( 300000,  500000,        VGA, 12),
	MS_OPENH264_CONF(      0,  300000,       QVGA, 12)
#else
	MS_OPENH264_CONF(1536000,  2560000, SXGA_MINUS, 15),
	MS_OPENH264_CONF(1536000,  2560000,       720P, 15),
	MS_OPENH264_CONF(1024000,  1536000,        XGA, 15),
	MS_OPENH264_CONF( 512000,  1024000,       SVGA, 15),
	MS_OPENH264_CONF( 256000,   512000,        VGA, 15),
	MS_OPENH264_CONF( 170000,   256000,       QVGA, 15),
	MS_OPENH264_CONF( 128000,   170000,       QCIF, 10),
	MS_OPENH264_CONF(  64000,   128000,       QCIF,  7),
	MS_OPENH264_CONF(      0,    64000,       QCIF,  5)
#endif
};


MSOpenH264Encoder::MSOpenH264Encoder(MSFilter *f)
	: mFilter(f), mPacker(0), mPacketisationMode(0), mVConfList(openh264_conf_list), mFrameCount(0), mInitialized(false)
{
	if (ms_get_cpu_count() > 1) mVConfList = &multicore_openh264_conf_list[0];
	mVConf = ms_video_find_best_configuration_for_bitrate(mVConfList, 384000);

	long ret = WelsCreateSVCEncoder(&mEncoder);
	if (ret != 0) {
		ms_error("OpenH264 encoder: Failed to create encoder: %d", ret);
	}
}

MSOpenH264Encoder::~MSOpenH264Encoder()
{
	if (mEncoder != 0) {
		WelsDestroySVCEncoder(mEncoder);
	}
}

void MSOpenH264Encoder::initialize()
{
	mFrameCount = 0;
	mPacker = rfc3984_new();
	rfc3984_set_mode(mPacker, mPacketisationMode);
	rfc3984_enable_stap_a(mPacker, FALSE);
	if (mEncoder != 0) {
		SEncParamExt params;
		int ret = mEncoder->GetDefaultParams(&params);
		if (ret != 0) {
			ms_error("OpenH264 encoder: Failed getting default params: %d", ret);
		} else {
			int targetBitrate, maxBitrate;
			calcBitrates(targetBitrate, maxBitrate);
			params.iUsageType = CAMERA_VIDEO_REAL_TIME;
			params.iInputCsp = videoFormatI420;
			params.iPicWidth = mVConf.vsize.width;
			params.iPicHeight = mVConf.vsize.height;
			params.iTargetBitrate = targetBitrate;
			params.iMaxBitrate = maxBitrate;
			params.iRCMode = RC_BITRATE_MODE;
			params.fMaxFrameRate = mVConf.fps;
			params.bEnableRc = true;
			params.bEnableFrameSkip = true;
			params.uiMaxNalSize = ms_get_mtu();
			params.iMultipleThreadIdc = ms_get_cpu_count();
			params.bEnableDenoise = false;
			params.bEnableBackgroundDetection = true;
			params.bEnableAdaptiveQuant = true;
			params.bEnableSceneChangeDetect = false;
			params.bEnableLongTermReference  = false;
			params.sSpatialLayers[0].iVideoWidth = mVConf.vsize.width;
			params.sSpatialLayers[0].iVideoHeight = mVConf.vsize.height;
			params.sSpatialLayers[0].fFrameRate = mVConf.fps;
			params.sSpatialLayers[0].iSpatialBitrate = mVConf.required_bitrate;
			params.sSpatialLayers[0].sSliceCfg.uiSliceMode = SM_DYN_SLICE;
			params.sSpatialLayers[0].sSliceCfg.sSliceArgument.uiSliceSizeConstraint = ms_get_mtu();
			ret = mEncoder->InitializeExt(&params);
			if (ret != 0) {
				ms_error("OpenH264 encoder: Failed to initialize: %d", ret);
			} else {
				mInitialized = true;
			}
		}
	}
}

void MSOpenH264Encoder::feed()
{
	if (!isInitialized()){
		ms_queue_flush(mFilter->inputs[0]);
		return;
	}

	mblk_t *im;
	MSQueue nalus;
	ms_queue_init(&nalus);
	long long int ts = mFilter->ticker->time * 90LL;

	// Send I frame 2 seconds and 4 seconds after the beginning
	if (mVideoStarter.needIFrame(mFilter->ticker->time)) {
		generateKeyframe();
	}

	while ((im = ms_queue_get(mFilter->inputs[0])) != NULL) {
		MSPicture pic;
		if (ms_yuv_buf_init_from_mblk(&pic, im) == 0) {
			SFrameBSInfo sFbi = { 0 };
			SSourcePicture srcPic = { 0 };
			srcPic.iColorFormat = videoFormatI420;
			srcPic.iPicWidth = pic.w;
			srcPic.iPicHeight = pic.h;
			for (int i = 0; i < 3; i++) {
				srcPic.iStride[i] = pic.strides[i];
				srcPic.pData[i] = pic.planes[i];
			}
			srcPic.uiTimeStamp = ts;
			int ret = mEncoder->EncodeFrame(&srcPic, &sFbi);
			if (ret == cmResultSuccess) {
				if ((sFbi.eOutputFrameType != videoFrameTypeSkip) && (sFbi.eOutputFrameType != videoFrameTypeInvalid)) {
					if (mFrameCount == 0) {
						mVideoStarter.firstFrame(mFilter->ticker->time);
					}
					mFrameCount++;
					fillNalusQueue(sFbi, &nalus);
					rfc3984_pack(mPacker, &nalus, mFilter->outputs[0], sFbi.uiTimeStamp);
				}
			} else {
				ms_error("OpenH264 encoder: Frame encoding failed: %d", ret);
			}
		}
		freemsg(im);
	}
}

void MSOpenH264Encoder::uninitialize()
{
	if (mPacker != 0) {
		rfc3984_destroy(mPacker);
		mPacker = 0;
	}
	if (mEncoder != 0) {
		mEncoder->Uninitialize();
	}
	mInitialized = false;
}

void MSOpenH264Encoder::setFPS(float fps)
{
	mVConf.fps = fps;
	setConfiguration(mVConf);
}

void MSOpenH264Encoder::setBitrate(int bitrate)
{
	if (isInitialized()) {
		// Encoding is already ongoing, do not change video size, only bitrate.
		mVConf.required_bitrate = bitrate;
		setConfiguration(mVConf);
	} else {
		MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_bitrate(mVConfList, bitrate);
		setConfiguration(best_vconf);
	}
}

void MSOpenH264Encoder::setSize(MSVideoSize size)
{
	MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_size(mVConfList, size);
	mVConf.vsize = size;
	mVConf.fps = best_vconf.fps;
	mVConf.bitrate_limit = best_vconf.bitrate_limit;
	setConfiguration(mVConf);
}

void MSOpenH264Encoder::addFmtp(const char *fmtp)
{
	char value[12];
	if (fmtp_get_value(fmtp, "packetization-mode", value, sizeof(value))) {
		mPacketisationMode = atoi(value);
		ms_message("packetization-mode set to %i", mPacketisationMode);
	}
}

void MSOpenH264Encoder::generateKeyframe()
{
	if (isInitialized()) {
		ms_filter_lock(mFilter);
		int ret = mEncoder->ForceIntraFrame(true);
		ms_filter_unlock(mFilter);
		if (ret != 0) {
			ms_error("OpenH264 encoder: Failed forcing intra-frame: %d", ret);
		}
	}
}

void MSOpenH264Encoder::setConfiguration(MSVideoConfiguration conf)
{
	mVConf = conf;
	if (mVConf.required_bitrate > mVConf.bitrate_limit)
		mVConf.required_bitrate = mVConf.bitrate_limit;
	if (isInitialized()) {
		ms_filter_lock(mFilter);
		applyBitrate();
		ms_filter_unlock(mFilter);
		return;
	}

	ms_message("OpenH264 encoder: Video configuration set: bitrate=%dbits/s, fps=%f, vsize=%dx%d",
		mVConf.required_bitrate, mVConf.fps, mVConf.vsize.width, mVConf.vsize.height);
}

void MSOpenH264Encoder::fillNalusQueue(SFrameBSInfo &sFbi, MSQueue *nalus)
{
	for (int i = 0; i < sFbi.iLayerNum; i++) {
		SLayerBSInfo *layerBsInfo = &sFbi.sLayerInfo[i];
		if (layerBsInfo != NULL) {
			mblk_t *m;
			unsigned char *ptr = layerBsInfo->pBsBuf;
			for (int j = 0; j < layerBsInfo->iNalCount; j++) {
				m = allocb(layerBsInfo->iNalLengthInByte[j], 0);
				memcpy(m->b_wptr, ptr, layerBsInfo->iNalLengthInByte[j]);
				m->b_wptr += layerBsInfo->iNalLengthInByte[j];
				ptr += layerBsInfo->iNalLengthInByte[j];
				ms_queue_put(nalus, m);
			}
		}
	}
}

void MSOpenH264Encoder::calcBitrates(int &targetBitrate, int &maxBitrate) const
{
	targetBitrate = mVConf.required_bitrate * 0.92;
	if (targetBitrate > RC_MARGIN) targetBitrate - RC_MARGIN;
	maxBitrate = targetBitrate + RC_MARGIN / 2;
}

void MSOpenH264Encoder::applyBitrate()
{
	int targetBitrate, maxBitrate;
	calcBitrates(targetBitrate, maxBitrate);
	int ret = mEncoder->SetOption(ENCODER_OPTION_BITRATE, &targetBitrate);
	if (ret != 0) {
		ms_error("OpenH264 encoder: Failed setting bitrate: %d", ret);
	}
	ret = mEncoder->SetOption(ENCODER_OPTION_MAX_BITRATE, &maxBitrate);
	if (ret != 0) {
		ms_error("OpenH264 encoder: Failed setting maximum bitrate: %d", ret);
	}
}
