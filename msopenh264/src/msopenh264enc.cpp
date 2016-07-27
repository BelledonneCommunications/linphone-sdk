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


#include "msopenh264enc.h"
#include "mediastreamer2/msticker.h"
#include "wels/codec_app_def.h"
#include "wels/codec_ver.h"

//static const int RC_MARGIN = 10000; // bits per sec
#if MSOPENH264_DEBUG
static int debugLevel = 4;
#else
static int debugLevel = 1;
#endif



#if defined(ANDROID) || (TARGET_OS_IPHONE == 1) || defined(__arm__)
	#define MS_OPENH264_CONF(required_bitrate, bitrate_limit, resolution, fps_pc, cpus_pc, fps_mobile, cpus_mobile) \
		{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H },fps_mobile, cpus_mobile, NULL }
#else
	#define MS_OPENH264_CONF(required_bitrate, bitrate_limit, resolution, fps_pc, cpus_pc, fps_mobile, cpus_mobile) \
		{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H },fps_pc, cpus_pc, NULL }
#endif

static const MSVideoConfiguration openh264_conf_list[] = {
	MS_OPENH264_CONF(2048000, 	1000000,            UXGA, 25,  4, 12, 2), /*1200p*/

	MS_OPENH264_CONF(1024000, 	5000000, 	  SXGA_MINUS, 25,  4, 12, 2),

	MS_OPENH264_CONF(1024000,  	5000000,   			720P, 25,  4, 12, 2),

	MS_OPENH264_CONF( 750000, 	2048000,             XGA, 20,  4, 12, 2),

	MS_OPENH264_CONF( 500000,  	1024000,            SVGA, 20,  2, 12, 2),

	MS_OPENH264_CONF( 256000,  	 800000,             VGA, 15,  2, 12, 2), /*480p*/

	MS_OPENH264_CONF( 128000,  	 512000,             CIF, 15,  1, 12, 2),

	MS_OPENH264_CONF( 100000,  	 380000,            QVGA, 15,  1, 15, 2), /*240p*/
	MS_OPENH264_CONF( 100000,  	 380000,            QVGA, 15,  1, 12, 1),

	MS_OPENH264_CONF( 128000,    170000,            QCIF, 10,  1, 10, 1),
	MS_OPENH264_CONF(  64000,    128000,            QCIF, 10,  1, 7, 1),
	MS_OPENH264_CONF(      0,     64000,            QCIF, 10,  1, 5, 1)
};


MSOpenH264Encoder::MSOpenH264Encoder(MSFilter *f)
	: mFilter(f), mPacker(0), mPacketisationMode(0), mVConfList(openh264_conf_list), mFrameCount(0),
		mInitialized(false), mPacketisationModeSet(false), mAVPFEnabled(false)
{
	long ret = WelsCreateSVCEncoder(&mEncoder);
	if (ret != 0) {
		ms_error("OpenH264 encoder: Failed to create encoder: %li", ret);
	}
	setConfigurationList(NULL);
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
	if (mPacketisationModeSet)
		rfc3984_set_mode(mPacker, mPacketisationMode);
	else rfc3984_set_mode(mPacker, 1); // in absence of explicit directive from the other end, allow mode 1 because it allows to send big slices which is necessary for large images
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
			//params.iInputCsp = videoFormatI420;
			params.iPicWidth = mVConf.vsize.width;
			params.iPicHeight = mVConf.vsize.height;
			params.iTargetBitrate = targetBitrate;
			params.iMaxBitrate = maxBitrate;
			params.iRCMode = RC_BITRATE_MODE;
			params.fMaxFrameRate = mVConf.fps;
			params.uiIntraPeriod=mVConf.fps*10;
			//params.bEnableRc = true;
			params.bEnableFrameSkip = true;
			params.bPrefixNalAddingCtrl = false;
			params.uiMaxNalSize = ms_factory_get_payload_max_size(mFilter->factory);
			params.iMultipleThreadIdc = ms_factory_get_cpu_count(mFilter->factory);
			params.bEnableDenoise = false;
			params.bEnableBackgroundDetection = true;
			params.bEnableAdaptiveQuant = false;
			params.bEnableSceneChangeDetect = false;
			params.bEnableLongTermReference  = false;
			params.iSpatialLayerNum=1;
			params.eSpsPpsIdStrategy = CONSTANT_ID;

			params.sSpatialLayers[0].iVideoWidth = mVConf.vsize.width;
			params.sSpatialLayers[0].iVideoHeight = mVConf.vsize.height;
			params.sSpatialLayers[0].fFrameRate = mVConf.fps;
			params.sSpatialLayers[0].iSpatialBitrate = targetBitrate;
			params.sSpatialLayers[0].iMaxSpatialBitrate = maxBitrate;

#if (OPENH264_MAJOR == 1) && (OPENH264_MINOR >=6)
			params.sSpatialLayers[0].sSliceArgument.uiSliceMode = SM_SIZELIMITED_SLICE;
			params.sSpatialLayers[0].sSliceArgument.uiSliceSizeConstraint = ms_factory_get_payload_max_size(mFilter->factory);
#else
			params.sSpatialLayers[0].sSliceCfg.uiSliceMode = SM_DYN_SLICE;
			params.sSpatialLayers[0].sSliceCfg.sSliceArgument.uiSliceSizeConstraint = ms_factory_get_payload_max_size(mFilter->factory);
#endif
			//params.sSpatialLayers[0].sSliceCfg.uiSliceMode = SM_AUTO_SLICE;
			ret = mEncoder->InitializeExt(&params);
			if (ret != 0) {
				ms_error("OpenH264 encoder: Failed to initialize: %d", ret);
			} else {
				ret = mEncoder->SetOption(ENCODER_OPTION_TRACE_LEVEL, &debugLevel);
				if (ret != 0) {
					ms_error("OpenH264 encoder: Failed setting trace level: %d", ret);
				}
				mInitialized = true;
			}
		}
	}
	if (!mAVPFEnabled && (mFrameCount == 0)) {
		ms_video_starter_init(&mVideoStarter);
	}
	ms_iframe_requests_limiter_init(&mIFrameLimiter, 1000);
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


	if ((im = ms_queue_peek_last(mFilter->inputs[0])) != NULL) {
		MSPicture pic;
		if (ms_yuv_buf_init_from_mblk(&pic, im) == 0) {
			bool generateKeyFrame = false;
			
			SFrameBSInfo sFbi = { 0 };
			SSourcePicture srcPic = { 0 };
			srcPic.iColorFormat = videoFormatI420;
			srcPic.iPicWidth = pic.w;
			srcPic.iPicHeight = pic.h;
			for (int i = 0; i < 3; i++) {
				srcPic.iStride[i] = pic.strides[i];
				srcPic.pData[i] = pic.planes[i];
			}
			srcPic.uiTimeStamp = mFilter->ticker->time;
			if (!mAVPFEnabled && ms_video_starter_need_i_frame(&mVideoStarter, mFilter->ticker->time)) {
				generateKeyFrame = true;
			}

			if (ms_iframe_requests_limiter_iframe_requested(&mIFrameLimiter, mFilter->ticker->time)){
				generateKeyFrame = true;
			}
			if (generateKeyFrame){
				generateKeyframe();
			}
			
			int ret = mEncoder->EncodeFrame(&srcPic, &sFbi);
			if (ret == cmResultSuccess) {
				if ((sFbi.eFrameType != videoFrameTypeSkip) && (sFbi.eFrameType != videoFrameTypeInvalid)) {
					if (sFbi.eFrameType == videoFrameTypeIDR) {
						ms_iframe_requests_limiter_notify_iframe_sent(&mIFrameLimiter, mFilter->ticker->time);
						ms_message("MSOpenH264Encoder: sending IDR");
					}
					mFrameCount++;
					if (!mAVPFEnabled && (mFrameCount == 1)) {
						ms_video_starter_first_frame(&mVideoStarter, mFilter->ticker->time);
					}
					fillNalusQueue(sFbi, &nalus);
					rfc3984_pack(mPacker, &nalus, mFilter->outputs[0], ts);
				}
			} else {
				ms_error("OpenH264 encoder: Frame encoding failed: %d", ret);
			}
		}
	}
	ms_queue_flush(mFilter->inputs[0]);
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
		MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_bitrate(mVConfList, bitrate, ms_factory_get_cpu_count(mFilter->factory));
		setConfiguration(best_vconf);
	}
}

void MSOpenH264Encoder::setSize(MSVideoSize size)
{
	MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_size(mVConfList, size, ms_factory_get_cpu_count(mFilter->factory));
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
		mPacketisationModeSet = true;
		ms_message("packetization-mode set to %i", mPacketisationMode);
	}
}

void MSOpenH264Encoder::setConfigurationList(const MSVideoConfiguration *confList) {
	MSVideoSize vsize;
	if (confList == NULL) {
		mVConfList = openh264_conf_list;
	} else {
		mVConfList = confList;
	}
	MS_VIDEO_SIZE_ASSIGN(vsize, CIF);
	mVConf = ms_video_find_best_configuration_for_size(mVConfList, vsize, ms_factory_get_cpu_count(mFilter->factory));
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

void MSOpenH264Encoder::requestVFU()
{
	// If we receive a VFU request, stop the video starter
	ms_video_starter_deactivate(&mVideoStarter);
	ms_filter_lock(mFilter);
	ms_iframe_requests_limiter_request_iframe(&mIFrameLimiter);
	ms_filter_unlock(mFilter);
}

void MSOpenH264Encoder::notifyPLI()
{
	ms_message("OpenH264: PLI requested");
	requestVFU();
}

void MSOpenH264Encoder::notifyFIR(uint8_t seqnr)
{
	requestVFU();
}

void MSOpenH264Encoder::generateKeyframe()
{
	int ret=0;
	if (mFrameCount>0){
		ret = mEncoder->ForceIntraFrame(true);
	}else ms_message("ForceIntraFrame() ignored since no frame has been generated yet.");

	if (ret != 0) {
		ms_error("OpenH264 encoder: Failed forcing intra-frame: %d", ret);
	}
}

void MSOpenH264Encoder::fillNalusQueue(SFrameBSInfo &sFbi, MSQueue *nalus)
{
	for (int i = 0; i < sFbi.iLayerNum; i++) {
		SLayerBSInfo *layerBsInfo = &sFbi.sLayerInfo[i];
		if (layerBsInfo != NULL) {
			mblk_t *m;
			unsigned char *ptr = layerBsInfo->pBsBuf;
			for (int j = 0; j < layerBsInfo->iNalCount; j++) {
				// Skip the NAL markers (first 4 bytes)
				int len = layerBsInfo->pNalLengthInByte[j] - 4;
				m = allocb(len, 0);
				memcpy(m->b_wptr, ptr + 4, len);
				m->b_wptr += len;
				ptr += layerBsInfo->pNalLengthInByte[j];
				ms_queue_put(nalus, m);
			}
		}
	}
}

void MSOpenH264Encoder::calcBitrates(int &targetBitrate, int &maxBitrate) const
{
	/*
	targetBitrate = mVConf.required_bitrate * 0.92;
	if (targetBitrate > RC_MARGIN) targetBitrate = targetBitrate - RC_MARGIN;
	maxBitrate = targetBitrate + RC_MARGIN / 2;
	*/
	targetBitrate = mVConf.required_bitrate*0.95;
	maxBitrate = mVConf.required_bitrate;
}

void MSOpenH264Encoder::applyBitrate()
{
	int targetBitrate, maxBitrate;
	calcBitrates(targetBitrate, maxBitrate);
	SBitrateInfo targetBitrateInfo, maxBitrateInfo;
	targetBitrateInfo.iLayer = maxBitrateInfo.iLayer = SPATIAL_LAYER_0;
	targetBitrateInfo.iBitrate = targetBitrate;
	maxBitrateInfo.iBitrate = maxBitrate;
	int ret = mEncoder->SetOption(ENCODER_OPTION_BITRATE, &targetBitrateInfo);
	if (ret != 0) {
		ms_error("OpenH264 encoder: Failed setting bitrate: %d", ret);
	}
	ret = mEncoder->SetOption(ENCODER_OPTION_MAX_BITRATE, &maxBitrateInfo);
	if (ret != 0) {
		ms_error("OpenH264 encoder: Failed setting maximum bitrate: %d", ret);
	}
	float frameRate = mVConf.fps;
	ret = mEncoder->SetOption(ENCODER_OPTION_FRAME_RATE, &frameRate);
	if (ret != 0) {
		ms_error("OpenH264 encoder: failed setting frame rate %d", ret);
	}
	ms_message("OpenH264 encoder applyBitrate done");
}
