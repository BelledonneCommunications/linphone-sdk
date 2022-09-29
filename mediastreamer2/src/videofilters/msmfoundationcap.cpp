/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "msmfoundationcap.h"

#include "mediastreamer2/msticker.h"
#include <ortp/ortp.h>

#include <map>
#include <vector>
#include <limits>
#include <mfobjects.h>	// ActivateObject + MFVideoFormat
#include <mfapi.h>	// MFCreateAttributes + MFVideoFormat
#include <Mferror.h>//MF_E_NO_MORE_TYPES

#pragma comment(lib,"mfuuid.lib") // MFVideoFormat symbols

// GUID hack to allow storing it into map
struct GUIDComparer{
    bool operator()(const GUID & Left, const GUID & Right) const{
        return memcmp(&Left , &Right,sizeof(Right)) < 0;
    }
};
template<typename Format>
class ConfigurationManager {
public:
	std::map<UINT32, std::map<UINT32, std::map<int, std::map<GUID, Format, GUIDComparer> > > > mSortedList;
	ConfigurationManager(){}
	
	virtual ~ConfigurationManager(){};
	virtual void clean(){mSortedList.clear();}
	Format getMediaConfiguration(GUID* videoFormat, UINT32* width, UINT32 *height, float * fps){// Return Best Format from parameters
		int roundedFps = (int)(*fps * 100.0);
		if( mSortedList.size() > 0){
			auto mediaWidth = mSortedList.lower_bound(*width);
			if(mediaWidth == mSortedList.end() )
				--mediaWidth;
			auto mediaHeight = mediaWidth->second.lower_bound(*height);
			if(mediaHeight == mediaWidth->second.end() )
				--mediaHeight;
			auto mediaFps = mediaHeight->second.upper_bound(roundedFps);// Try to get more FPS than target
			if(mediaFps != mediaHeight->second.begin() )// fps <= target or fps[0]
				--mediaFps;

			auto mediaVideo = mediaFps->second.find(*videoFormat);
			if( mediaVideo == mediaFps->second.end())
				mediaVideo = mediaFps->second.find(MFVideoFormat_NV12);// Previous format is not found. Try with NV12
			if (mediaVideo == mediaFps->second.end())
				mediaVideo = mediaFps->second.find(MFVideoFormat_MJPG);// Try with MFVideoFormat_MJPG format
			if(mediaVideo == mediaFps->second.end())
				mediaVideo = mediaFps->second.find(MFVideoFormat_YUY2);// Try with MFVideoFormat_YUY2 format
			if(mediaVideo == mediaFps->second.end()){
				return nullptr;
			}else{
				*videoFormat = mediaVideo->first;
				*width = mediaWidth->first;
				*height = mediaHeight->first;
				*fps = mediaFps->first / 100.0f;
				return mediaVideo->second;
			}
		}else
			return nullptr;

	}
	std::string toString(){
		std::string displayStr;
		for(auto width : mSortedList){
			for(auto height : width.second){
				for(auto fps : height.second) {
					for(auto video : fps.second) {
						displayStr += std::to_string(width.first) +" | " + std::to_string(height.first) + " | " + std::to_string(fps.first/100.0)+ std::string("fps | ") + MSMFoundationCap::pixFmtToString(video.first) +std::string("\n");
					}
				}
			}
		}
		return displayStr;
	}
};

MSMFoundationCap::MSMFoundationCap() {
	InitializeCriticalSection(&mCriticalSection);
	mWidth = MS_VIDEO_SIZE_CIF_W;
	mHeight = MS_VIDEO_SIZE_CIF_H;
	setVideoFormat(MFVideoFormat_Base);// Default format
	mAllocator = ms_yuv_buf_allocator_new();
	mFrameData = NULL;
	mFps = 60.0;
	mOrientation = 0;
	mSampleCount= mProcessCount=0;
	mFmtChanged = FALSE;
	mNewFormatTakenAccount = TRUE;
}

MSMFoundationCap::~MSMFoundationCap() {
	EnterCriticalSection(&mCriticalSection);
	if(mFrameData){
		freemsg(mFrameData);
		mFrameData = NULL;
	}
	ms_free(mAllocator);
	LeaveCriticalSection(&mCriticalSection);
	DeleteCriticalSection(&mCriticalSection);
}

void MSMFoundationCap::safeRelease(){
	delete this;
}

void MSMFoundationCap::setVSize(MSVideoSize vsize) {
	setMediaConfiguration(mVideoFormat, vsize.width, vsize.height, mFps);
}

void MSMFoundationCap::setWebCam(MSWebCam* webcam){
	mDeviceName = webcam->name;
}

void MSMFoundationCap::setFps(const float &pFps){
	ms_video_init_framerate_controller(&mFramerateController, pFps);// Set the controller to the target FPS and then try to find a format to fit the configuration
	setMediaConfiguration(mVideoFormat, mWidth, mHeight, pFps);// mFps can change here, but don't use it to the controller
	ms_average_fps_init(&mAvgFps,"[MSMFoundationCap] fps=%f");
}

float MSMFoundationCap::getFps() const { return mFps; }

int MSMFoundationCap::getDeviceOrientation() const{	return mOrientation; }

void MSMFoundationCap::setDeviceOrientation(int orientation){ mOrientation = orientation; }

//----------------------------------------

void MSMFoundationCap::activate() {}

void MSMFoundationCap::feed(MSFilter * filter) {
	if (mFmtChanged) {// Keep this if we want to manage camera behaviors (format changing not coming from Linphone)
		ms_message("[MSMFoundationCap] Camera has changed its own output format. Sending event (%dx%d)", mWidth, mHeight);
		mNewFormatTakenAccount = FALSE;// Reset flag before sending new format event.
		ms_filter_notify_no_arg(filter, MS_FILTER_OUTPUT_FMT_CHANGED);
		mFmtChanged = FALSE;
		return;
	}
	if(!mNewFormatTakenAccount)
		return;// Avoid sending new frames till new format has not been taken account (look for getter of size)
	mblk_t **data = &mFrameData;
	EnterCriticalSection(&mCriticalSection);
	if(mRunning && mFrameData ) {
		if (isTimeToSend(filter->ticker->time) ) {
			++mProcessCount;
			uint32_t timestamp;	
			timestamp = (uint32_t)(filter->ticker->time * 90);// rtp uses a 90000 Hz clockrate for video
			mblk_set_timestamp_info(*data, timestamp);
			ms_queue_put(filter->outputs[0], *data);
			ms_average_fps_update(&mAvgFps,filter->ticker->time);
			*data = NULL;
		}
	}
	LeaveCriticalSection(&mCriticalSection);
}

void MSMFoundationCap::start() {}

void MSMFoundationCap::stop(const int &pWaitStop) {}

void MSMFoundationCap::deactivate() {}

//-------------------------------------------------------------------------------------------------------------

HRESULT MSMFoundationCap::setMediaConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float pFps){return S_OK;}

void MSMFoundationCap::setVideoFormat(const GUID &videoFormat){
	if( videoFormat == MFVideoFormat_Base || !isSupportedFormat(videoFormat)){// Default format
		setVideoFormat(MFVideoFormat_NV12);
	}else{
		mVideoFormat = videoFormat;
		if(mVideoFormat == MFVideoFormat_NV12)
			mPixelFormat = MS_YUV420P;
		else if(mVideoFormat == MFVideoFormat_MJPG)
			mPixelFormat = MS_MJPEG;
		else if(mVideoFormat == MFVideoFormat_YUY2)
			mPixelFormat = MS_YUY2;
	}
}

HRESULT MSMFoundationCap::restartWithNewConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float pFps){	return S_OK;}

//----------------------------------------

bool_t MSMFoundationCap::isTimeToSend(uint64_t tickerTime){
	return ms_video_capture_new_frame(&mFramerateController, tickerTime);
}

//Only support NV12, MJPG and YUY2
bool_t MSMFoundationCap::isSupportedFormat(const GUID &videoFormat){
	return videoFormat == MFVideoFormat_NV12 || videoFormat == MFVideoFormat_MJPG || videoFormat ==  MFVideoFormat_YUY2;
}

void MSMFoundationCap::processFrame(byte* inputBytes, DWORD inputCapacity, int inputStride ) {
	if (mFrameData) { // Clean memory
		freemsg(mFrameData);
		mFrameData = NULL;
	}
	if (mVideoFormat == MFVideoFormat_NV12) { // Process raw data from NV12
		if( inputCapacity >= mHeight * inputStride)// Ensure to get enough data in frame
			mFrameData = copy_ycbcrbiplanar_to_true_yuv_with_rotation(mAllocator, inputBytes, inputBytes + mHeight * abs(inputStride), mOrientation, mWidth, mHeight, inputStride, inputStride, TRUE);
	} else if (mVideoFormat == MFVideoFormat_MJPG || mVideoFormat == MFVideoFormat_YUY2) { // Process raw data from MJPEG/YUY2
		mFrameData = ms_yuv_allocator_get(mAllocator, inputCapacity, mWidth, mHeight);
		if (mFrameData) {
			memcpy(mFrameData->b_rptr, inputBytes, inputCapacity);
		}
	}
}
/*******************************************************************************
 * Methods to (de)initialize and run the Media Foundation video capture filter *
 ******************************************************************************/

static MSMFoundationCap* ms_mfoundation_new();// Instanciate a Desktop or UWP Object

static void ms_mfoundation_init(MSFilter *filter) {
	MSMFoundationCap *mf = ms_mfoundation_new();
	filter->data = mf;
	// No need to activate, it is done by ms_mfoundationcap_create_reader where it has the information about the webcam.
}

static void ms_mfoundation_preprocess(MSFilter *filter) {
	MSMFoundationCap *mf = static_cast<MSMFoundationCap *>(filter->data);
	mf->start();
}

static void ms_mfoundation_process(MSFilter *filter) {
	MSMFoundationCap *mf = static_cast<MSMFoundationCap *>(filter->data);
	mf->feed(filter);
}

static void ms_mfoundation_postprocess(MSFilter *filter) {
	MSMFoundationCap *mf = static_cast<MSMFoundationCap *>(filter->data);
	mf->stop(5000);// a maximum of 5s should be enough to stop
}

static void ms_mfoundation_uninit(MSFilter *filter) {
	MSMFoundationCap *mf = static_cast<MSMFoundationCap *>(filter->data);
	mf->deactivate();
	mf->safeRelease();
}

static int ms_mfoundation_set_fps(MSFilter *filter, void *arg){
	MSMFoundationCap *mf=(MSMFoundationCap*)filter->data;
	mf->setFps(*(float*)arg);
	return 0;
}

static int ms_mfoundation_get_fps(MSFilter *filter, void *arg){
	MSMFoundationCap *mf=(MSMFoundationCap*)filter->data;
	if (filter->ticker){
		*((float*)arg) = ms_average_fps_get(&mf->mAvgFps);
	} else {
		*((float*)arg) = mf->getFps();
	}
	return 0;
}

static int ms_mfoundation_set_pix_fmt(MSFilter *filter, void *arg) {
	MSMFoundationCap *mf = (MSMFoundationCap*)filter->data;
	mf->mPixelFormat = *((MSPixFmt*)arg);
	return 0;
}

static int ms_mfoundation_get_pix_fmt(MSFilter *filter, void *arg) {
	MSMFoundationCap *mf = (MSMFoundationCap*)filter->data;
	if (mf->mPixelFormat == MS_RGB24)
		*((MSPixFmt*)arg) = (MSPixFmt)MS_RGB24_REV;
	else
		*((MSPixFmt*)arg) = (MSPixFmt)mf->mPixelFormat;
	return 0;
}

static int ms_mfoundation_set_vsize(MSFilter *filter, void *arg) {
	MSMFoundationCap *mf = (MSMFoundationCap*)filter->data;
	mf->setVSize(*((MSVideoSize*)arg));
	return 0;
}

static int ms_mfoundation_get_vsize(MSFilter *filter, void *arg) {
	MSMFoundationCap *mf = (MSMFoundationCap*)filter->data;
	MSVideoSize *vs = (MSVideoSize*)arg;
	vs->height = mf->mHeight;
	vs->width = mf->mWidth;
	mf->mNewFormatTakenAccount = TRUE;// We get a size : we suppose that the new format is taken account.
	return 0;
}

static int ms_mfoundation_set_device_orientation(MSFilter *filter, void *arg) {
	MSMFoundationCap *mf = static_cast<MSMFoundationCap *>(filter->data);
	int orientation = *((int *)arg);
	if (mf->getDeviceOrientation() != orientation) {
		mf->setDeviceOrientation(orientation);
	}
	return 0;
}

extern "C" {

static MSFilterMethod ms_mfoundation_methods[] = {
	{ MS_FILTER_SET_FPS,        ms_mfoundation_set_fps },
	{ MS_FILTER_GET_FPS,        ms_mfoundation_get_fps },
	{ MS_FILTER_SET_PIX_FMT	,	ms_mfoundation_set_pix_fmt },
	{ MS_FILTER_GET_PIX_FMT	,	ms_mfoundation_get_pix_fmt },
	{ MS_FILTER_GET_VIDEO_SIZE,	ms_mfoundation_get_vsize },
	{ MS_FILTER_SET_VIDEO_SIZE,	ms_mfoundation_set_vsize },
	{ MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION,     ms_mfoundation_set_device_orientation }, 
	{ 0,						NULL }
};

MSFilterDesc ms_mfoundation_read_desc = {
	MS_FILTER_PLUGIN_ID,
	"MSMFoundationCap",
	N_("Media Foundation video capture."),
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	ms_mfoundation_init,
	ms_mfoundation_preprocess,
	ms_mfoundation_process,
	ms_mfoundation_postprocess,
	ms_mfoundation_uninit,
	ms_mfoundation_methods
};
}//extern "C"

MS_FILTER_DESC_EXPORT(ms_mfoundation_read_desc)
#ifdef __cplusplus
extern "C"{
#endif
// DETECTION
static MSFilter *ms_mfoundationcap_create_reader(MSWebCam *cam) {
	MSFactory *factory = ms_web_cam_get_factory(cam);
	MSFilter *filter = ms_factory_create_filter_from_desc(factory, &ms_mfoundation_read_desc);// Call init()
	MSMFoundationCap *mf=(MSMFoundationCap*)filter->data;
	ms_message("[MSMFoundationCap] Using %s", cam->name);
	mf->setWebCam(cam);
	mf->activate();
	return filter;
}

static void ms_mfoundationcap_uinit(MSWebCam *cam);
static void ms_mfoundationcap_detect(MSWebCamManager *manager);

MSWebCamDesc ms_mfoundationcap_desc = {
	"MSMFoundationCamera",
	ms_mfoundationcap_detect,
	NULL,
	ms_mfoundationcap_create_reader,
	ms_mfoundationcap_uinit,
	NULL
};
#ifdef __cplusplus
}//extern "C"
#endif

#ifndef IF_EQUAL_RETURN
#define IF_EQUAL_RETURN(param, val) if(val == param) return #val
#endif

const char *MSMFoundationCap::pixFmtToString(const GUID &guid){
	IF_EQUAL_RETURN(guid, MFVideoFormat_AI44); //     FCC('AI44')
	IF_EQUAL_RETURN(guid, MFVideoFormat_ARGB32); //   D3DFMT_A8R8G8B8
	IF_EQUAL_RETURN(guid, MFVideoFormat_AYUV); //     FCC('AYUV')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DV25); //     FCC('dv25')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DV50); //     FCC('dv50')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DVH1); //     FCC('dvh1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DVSD); //     FCC('dvsd')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DVSL); //     FCC('dvsl')
	IF_EQUAL_RETURN(guid, MFVideoFormat_H264); //     FCC('H264')
	IF_EQUAL_RETURN(guid, MFVideoFormat_H265); //     FCC('H265')
	IF_EQUAL_RETURN(guid, MFVideoFormat_HEVC); //     FCC('HEVC')
	IF_EQUAL_RETURN(guid, MFVideoFormat_I420); //     FCC('I420')
	IF_EQUAL_RETURN(guid, MFVideoFormat_IYUV); //     FCC('IYUV')
	IF_EQUAL_RETURN(guid, MFVideoFormat_M4S2); //     FCC('M4S2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MJPG);
	IF_EQUAL_RETURN(guid, MFVideoFormat_MP43); //     FCC('MP43')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MP4S); //     FCC('MP4S')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MP4V); //     FCC('MP4V')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MPG1); //     FCC('MPG1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MSS1); //     FCC('MSS1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MSS2); //     FCC('MSS2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_NV11); //     FCC('NV11')
	IF_EQUAL_RETURN(guid, MFVideoFormat_NV12); //     FCC('NV12')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P010); //     FCC('P010')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P016); //     FCC('P016')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P210); //     FCC('P210')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P216); //     FCC('P216')
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB24); //    D3DFMT_R8G8B8
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB32); //    D3DFMT_X8R8G8B8
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB555); //   D3DFMT_X1R5G5B5
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB565); //   D3DFMT_R5G6B5
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB8);
	IF_EQUAL_RETURN(guid, MFVideoFormat_UYVY); //     FCC('UYVY')
	IF_EQUAL_RETURN(guid, MFVideoFormat_v210); //     FCC('v210')
	IF_EQUAL_RETURN(guid, MFVideoFormat_v410); //     FCC('v410')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WMV1); //     FCC('WMV1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WMV2); //     FCC('WMV2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WMV3); //     FCC('WMV3')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WVC1); //     FCC('WVC1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y210); //     FCC('Y210')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y216); //     FCC('Y216')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y410); //     FCC('Y410')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y416); //     FCC('Y416')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y41P);
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y41T);
	IF_EQUAL_RETURN(guid, MFVideoFormat_YUY2); //     FCC('YUY2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_YV12); //     FCC('YV12')
	IF_EQUAL_RETURN(guid, MFVideoFormat_YVYU);

	return "Bad format";
}
#ifndef IF_EQUAL_GUID_RETURN
#define IF_EQUAL_GUID_RETURN(guid, sub, type) if(type == std::string(MSMFoundationCap::pixFmtToString(guid)) || type == sub) return guid
#endif

const GUID& MSMFoundationCap::pixStringToGuid(const std::string& type){
	IF_EQUAL_GUID_RETURN(MFVideoFormat_AI44, "AI44", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_ARGB32, "ARGB32", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_AYUV, "AYUV", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_DV25, "DV25", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_DV50, "DV50", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_DVH1, "DVH1", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_DVSD, "DVSD", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_DVSL, "DVSL", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_H264, "H264", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_H265, "H265", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_HEVC, "HEVC", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_I420, "I420", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_IYUV, "IYUV", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_M4S2, "M4S2", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_MJPG, "MJPG", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_MP43, "MP43", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_MP4S, "MP4S", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_MP4V, "MP4V", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_MPG1, "MPG1", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_MSS1, "MSS1", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_MSS2, "MSS2", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_NV11, "NV11", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_NV12, "NV12", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_P010, "P010", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_P016, "P016", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_P210, "P210", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_P216, "P216", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_RGB24, "RGB24", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_RGB32, "RGB32", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_RGB555, "RGB555", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_RGB565, "RGB565", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_RGB8, "RGB8", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_UYVY, "UYVY", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_v210, "v210", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_v410, "v410", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_WMV1, "WMV1", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_WMV2, "WMV2", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_WMV3, "WMV3", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_WVC1, "WVC1", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_Y210, "Y210", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_Y216, "Y216", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_Y410, "Y410", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_Y416, "Y416", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_Y41P, "Y41P", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_Y41T, "Y41T", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_YUY2, "YUY2", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_YV12, "YV12", type);
	IF_EQUAL_GUID_RETURN(MFVideoFormat_YVYU, "YVYU", type);
	
	return MFVideoFormat_Base;
}

#if defined( MS2_WINDOWS_UWP )

#include <agile.h>
#include <algorithm>
#include <mfidl.h>
#include <windows.h>
#include <Memorybuffer.h>
#include <collection.h>
#include <windows.foundation.h> //TypedEventHandler
#include <windows.media.capture.frames.h> //MediaFrameFormat
#include <windows.media.capture.h> //MediaStreamType

using namespace Platform;
using namespace Windows::Media::Capture; //MediaStreamType
using namespace Windows::Foundation; //TypedEventHandler
using namespace Windows::Devices::Enumeration; //DeviceInformationCollection
using namespace concurrency;
using namespace Windows::Graphics::Imaging; //SoftwareBitmap 
using namespace Windows::Media::Capture::Frames; // MediaFrameFormat

//------------------------------------ Hack for TypedEventHandler
static MSMFoundationUwpImpl *gCurrentCap = nullptr;
[Windows::Foundation::Metadata::WebHostHiddenAttribute] ref class App sealed {
  public:
	App() {
	}
	void reader_FrameArrived(Windows::Media::Capture::Frames::MediaFrameReader ^ reader,
							 Windows::Media::Capture::Frames::MediaFrameArrivedEventArgs ^ args) {
		if (gCurrentCap)
			gCurrentCap->reader_FrameArrived(reader, args);
	}
};
static App ^ gApp = nullptr;
//-----------------------------------------------------------------

std::string make_string(const std::wstring& wstring)
{
  auto wideData = wstring.c_str();
  int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideData, -1, nullptr, 0, NULL, NULL);
  auto utf8 = std::make_unique<char[]>(bufferSize);
  if (0 == WideCharToMultiByte(CP_UTF8, 0, wideData, -1, utf8.get(), bufferSize, NULL, NULL))
    throw std::exception("Can't convert string to UTF8");

  return std::string(utf8.get());
}
// Convert a reference pointer to a specific ComPtr.
template <typename T> Microsoft::WRL::ComPtr<T> AsComPtr(Platform::Object ^ object) {
	Microsoft::WRL::ComPtr<T> p;
	reinterpret_cast<IUnknown *>(object)->QueryInterface(IID_PPV_ARGS(&p));
	return p;
}

class ConfigurationManagerUwp : public ConfigurationManager<MediaFrameFormat^>{
public:
	ConfigurationManagerUwp(){}
	virtual ~ConfigurationManagerUwp(){};

	void setMediaTypes(Windows::Media::Capture::Frames::MediaFrameSource^ source){
		for (MediaFrameFormat^ format : source->SupportedFormats){
			// Limit ourselves to formats that we can render.
			if( format->VideoFormat){
				UINT32 width = format->VideoFormat->Width;
				UINT32 height = format->VideoFormat->Height;
				int framerate = (int)((format->FrameRate ? static_cast<float>(format->FrameRate->Numerator / format->FrameRate->Denominator) : 0.0f) * 100.0f);
			
				std::wstring wsstr(format->Subtype->Data());
				const GUID& videoType = MSMFoundationCap::pixStringToGuid(make_string(wsstr));
				if(MSMFoundationCap::isSupportedFormat(videoType))
					mSortedList[width][height][framerate][videoType] = format;
			}
		}
	}
};

MSMFoundationUwpImpl::MSMFoundationUwpImpl() : MSMFoundationCap() {
	mCameraSharingMode = MediaCaptureSharingMode::ExclusiveControl;
}

MSMFoundationUwpImpl::~MSMFoundationUwpImpl() {
	gCurrentCap = nullptr;
	disposeMediaCapture();
	this->mStreaming = FALSE;	// Just in case of concurrency
}

void MSMFoundationUwpImpl::safeRelease(){
	mCurrentTask = mCurrentTask.then([this]() {
		mCurrentTask = concurrency::task_from_result();
		delete this;
	});
}

void MSMFoundationUwpImpl::setWebCam(MSWebCam* webcam){
	mDeviceName = webcam->name;
	std::string id((char*)webcam->data);
	std::wstring w_str = std::wstring(id.begin(), id.end());
	const wchar_t *w_chars = w_str.c_str();
	mId = ref new Platform::String(w_chars, (unsigned int)w_str.length());
}

void MSMFoundationUwpImpl::setFrameSource(Windows::Media::Capture::Frames::MediaFrameSourceGroup ^ sourceGroup) {
	EnterCriticalSection(&mCriticalSection);
	mSourceGroup = sourceGroup;
	mId = sourceGroup->Id;
	mDisplayName = sourceGroup->DisplayName;
	LeaveCriticalSection(&mCriticalSection);
}

void MSMFoundationUwpImpl::updateFrameSource() {
	if (mMediaCapture != nullptr && mSource == nullptr && mSourceGroup !=nullptr) {
		for(auto info : mSourceGroup->SourceInfos ) {
			if(info->MediaStreamType == MediaStreamType::VideoRecord ) {
				mSource = mMediaCapture->FrameSources->Lookup(info->Id);
				break;
			}
		}
		if(mSource == nullptr) {
			mSource = mMediaCapture->FrameSources->First()->Current->Value;
		}
		setMediaConfiguration(mVideoFormat,mWidth, mHeight,mFps );
	}
}

task<bool> MSMFoundationUwpImpl::tryInitializeCaptureAsync() {
	if (mMediaCapture != nullptr) { // Check if Media Capture has already been initialized
		return task_from_result<bool>(true);
	}
	if (mSourceGroup == nullptr) {
		return task_from_result<bool>(false);
	}
	mMediaCapture = ref new MediaCapture(); // Create a new media capture object.
	auto settings = ref new MediaCaptureInitializationSettings();
	settings->SourceGroup = mSourceGroup; // Select the source we will be reading from.
	settings->SharingMode = mCameraSharingMode; // This media capture has exclusive control of the source.
	settings->MemoryPreference = MediaCaptureMemoryPreference::Cpu;// Set to CPU to ensure frames always contain CPU SoftwareBitmap images,instead of preferring GPU D3DSurface images.
	settings->StreamingCaptureMode = StreamingCaptureMode::Video;// Capture only video. Audio device will not be initialized.
	try{
		create_task(mMediaCapture->InitializeAsync(settings)).wait();
		ms_message("[MSMFoundationCapUwp] Successfully initialized MediaCapture");
		return task_from_result<bool>(true);
	}catch(Platform::Exception ^ e){
		if(  e->HResult == 0xc00d3704 && mCameraSharingMode == MediaCaptureSharingMode::ExclusiveControl){
			mCameraSharingMode = MediaCaptureSharingMode::SharedReadOnly; // This media capture has shared control of the source.
			disposeMediaCapture();
			return tryInitializeCaptureAsync();
		}else{
			ms_warning("[MSMFoundationCapUwp] 1. Failed to initialize media capture");
			return task_from_result<bool>(false);
		}
	}
}

task<void> MSMFoundationUwpImpl::createReaderAsync() {
	bool initialized =  tryInitializeCaptureAsync().get();
	task<void> taskResult = task_from_result();
	if( initialized){
		updateFrameSource(); // Create Source from MediaCapture
		if (mSource != nullptr) {
			bool needToCreateReader = false;// Need for freeing critical section.
			EnterCriticalSection(&mCriticalSection);
			try{
				MediaFrameReader ^ reader = create_task(mMediaCapture->CreateFrameReaderAsync(mSource, mSource->CurrentFormat->Subtype)).get();
				mReader = reader;
				gCurrentCap = this;// Hack to allow callback to use native data
				if(!gApp)
					gApp = ref new App();
				mFrameArrivedToken = mReader->FrameArrived += ref new TypedEventHandler<MediaFrameReader ^, MediaFrameArrivedEventArgs ^>( gApp, &App::reader_FrameArrived, CallbackContext::Any);
			}catch(Platform::Exception ^ e){//CreateFrameReaderAsync
				if( e->HResult == 0xc00d3704 && mCameraSharingMode == MediaCaptureSharingMode::ExclusiveControl){
					mCameraSharingMode = MediaCaptureSharingMode::SharedReadOnly; // This media capture has shared control of the source.
					disposeMediaCapture();
					ms_warning("[MSMFoundationCapUwp] Cannot create Reader on source, retry on SharedReadOnly mode.");
					needToCreateReader = true;
				}
			}
			LeaveCriticalSection(&mCriticalSection);
			if(needToCreateReader)
				taskResult = createReaderAsync();
		}else {
			ms_error("[MSMFoundationCapUwp] Cannot create source");
		}
	}
	return taskResult;
}

task<void> MSMFoundationUwpImpl::stopReaderAsync() {
	task<void> currentTask = task_from_result();
	mStreaming = false;
	if (mReader != nullptr) {
		// Stop streaming from reader.
		currentTask = currentTask.then([this]() { return mReader->StopAsync(); }).then([this]() {
			EnterCriticalSection(&mCriticalSection);
			if( mReader){
				mReader->FrameArrived -= mFrameArrivedToken;
				mReader = nullptr;
				gCurrentCap = nullptr;
			}
			LeaveCriticalSection(&mCriticalSection);
			ms_message("[MSMFoundationCapUwp] Reader stopped");
		});
	}
	
	return currentTask;
}

task<void> MSMFoundationUwpImpl::startReaderAsync() {
	createReaderAsync().wait();
	if (mReader != nullptr && !mStreaming) {
		EnterCriticalSection(&mCriticalSection);
		try{
			auto result = create_task(mReader->StartAsync()).get();			
			if (result == MediaFrameReaderStartStatus::Success) {
				mStreaming = true;
				ms_message("[MSMFoundationCapUwp] Start reader");
			}else
				ms_warning("[MSMFoundationCapUwp] Cannot start Reader. Status is %X", result);
		}catch(Platform::Exception ^ e){
			std::wstring wsstrResult(e->Message->Data());
			ms_warning("[MSMFoundationCapUwp] Exception on Reader StartAsync. Reader will not stream : %s [%X]", make_string(wsstrResult).c_str(), e->HResult);
		}
		LeaveCriticalSection(&mCriticalSection);
		return task_from_result();
	}
	return task_from_result();
}

void MSMFoundationUwpImpl::activate() {
	mCurrentTask = mCurrentTask
	.then([this]() {
		try{
			MediaFrameSourceGroup ^ group = create_task(MediaFrameSourceGroup::FromIdAsync(mId)).get();// TODO Can raise exception if no more available
			setFrameSource(group);
		}catch(Platform::Exception^ e){
			std::wstring wsstr(mId->Data());
			std::wstring wsstrResult(e->Message->Data());
			ms_error("[MSMFoundationCapUwp] Cannot get Frame Source from %s : %s [%X]", make_string(wsstr).c_str(), make_string(wsstrResult).c_str(), e->HResult);
		}
		return task_from_result();
	})
	.then([this]() {
		if (mMediaCapture != nullptr && mSource != nullptr) {
			for (MediaFrameFormat^ format : mSource->SupportedFormats) {
				// Limit ourselves to formats that we can render.
				String^ width = format->VideoFormat ? format->VideoFormat->Width.ToString() : "?";
				String^ height = format->VideoFormat ? format->VideoFormat->Height.ToString() : "?";
				String^ framerate = format->FrameRate ? round(format->FrameRate->Numerator / format->FrameRate->Denominator).ToString() : "?";
				String^ DisplayName = format->MajorType + " | " + format->Subtype + " | " + width + " x " + height + " | " + framerate + "fps";
				std::wstring wsstr(DisplayName->Data());
				ms_message("[MSMFoundationCapUwp] %s",make_string(wsstr).c_str()); 
			}
		}
		return task_from_result();
	});//, task_continuation_context::get_current_winrt_context());
}

void MSMFoundationUwpImpl::disposeMediaCapture() {
	mSource = nullptr;
	if(mMediaCapture != nullptr){
		delete mMediaCapture.Get();
		mMediaCapture = nullptr;
	}
}

void MSMFoundationUwpImpl::processFrame(Windows::Media::Capture::Frames::MediaFrameReference ^ frame) {
	if (frame == nullptr) return;
	EnterCriticalSection(&mCriticalSection);
	SoftwareBitmap ^ inputBitmap = frame->VideoMediaFrame->GetVideoFrame()->SoftwareBitmap;
	BitmapBuffer ^ input = inputBitmap->LockBuffer(BitmapBufferAccessMode::Read);
	int inputStride = input->GetPlaneDescription(0).Stride;	 // Get stride values to calculate buffer position for a given pixel x and y position.
	IMemoryBufferReference ^ inputReference = input->CreateReference();
	byte *inputBytes;
	UINT32 inputCapacity;
	mWidth = inputBitmap->PixelWidth;
	mHeight = inputBitmap->PixelHeight;
	AsComPtr<IMemoryBufferByteAccess>(inputReference)->GetBuffer(&inputBytes, &inputCapacity);

	MSMFoundationCap::processFrame(inputBytes, (DWORD)inputCapacity, inputStride);

	// Close objects that need closing.
	delete inputReference;
	delete input;
	delete inputBitmap;
	LeaveCriticalSection(&mCriticalSection);
}

void MSMFoundationUwpImpl::reader_FrameArrived(MediaFrameReader ^ reader, MediaFrameArrivedEventArgs ^ args) {
	mCurrentTask = mCurrentTask.then([this, reader, args]() {
		if(reader){
			try {
				MediaFrameReference^ frame = reader->TryAcquireLatestFrame();
				if(frame)
					processFrame(frame);
			}catch (Platform::Exception^ e) {
				std::wstring wsstrResult(e->Message->Data());
				ms_error("[MSMFoundationCapUwp] Cannot acquire last frame : %s [%X]", make_string(wsstrResult).c_str(), e->HResult);
			}
		}
		return task_from_result();
	});
}
task<void> MSMFoundationUwpImpl::startAsync() {
	startReaderAsync().wait();
	mRunning = TRUE;
	return task_from_result();
}
void MSMFoundationUwpImpl::start() {
	mCurrentTask = mCurrentTask.then([this]() {
		return startAsync();
	});
}

task<void> MSMFoundationUwpImpl::stopAsync() {
	if (mFrameData) {
		freemsg(mFrameData);
		mFrameData = NULL;
	}
	mRunning = FALSE;
	return stopReaderAsync();
}
void MSMFoundationUwpImpl::stop(const int &pWaitStop) {
	mCurrentTask = mCurrentTask.then([this]() {
		return stopAsync();
	});
}

void MSMFoundationUwpImpl::deactivate() {
	mCurrentTask = mCurrentTask.then([this]() {
		ms_message("[MSMFoundationCapUwp] Frames count : %d samples, %d processed", mSampleCount, mProcessCount);
		mSampleCount = mProcessCount = 0;
		return task_from_result();
	});
}
void MSMFoundationUwpImpl::setInternalFormat(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float pFps){
	setVideoFormat(videoFormat);
	mFps = pFps;
	bool_t formatChanged = FALSE;
	if (frameWidth != mWidth || frameHeight != mHeight) {
		if (mFrameData) {
			freemsg(mFrameData);
			mFrameData = NULL;
		}
		mWidth = frameWidth;
		mHeight = frameHeight;
		formatChanged = TRUE;
	}
	if (mSource)
		ms_message("[MSMFoundationCapUwp] %s the video format : %dx%d : %s, %f fps", (formatChanged ? "Changed" : "Keep"), mWidth, mHeight, pixFmtToString(mVideoFormat), mFps);
	else
		ms_message("[MSMFoundationCapUwp] %s the video format without Reader : %dx%d : %s, %f fps", (formatChanged ? "Changed" : "Keep"), mWidth, mHeight, pixFmtToString(mVideoFormat), mFps);
}

HRESULT MSMFoundationUwpImpl::setMediaConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float pFps){
	HRESULT hr = S_OK;
	MediaFrameFormat^ mediaFormat;
	ConfigurationManagerUwp configs;
	bool_t doSet = TRUE;
	GUID requestedVideoFormat = videoFormat;
	UINT32 requestedFrameWidth = frameWidth;
	UINT32 requestedFrameHeight = frameHeight;

	EnterCriticalSection(&mCriticalSection);
	if( mSource){
		configs.setMediaTypes(mSource);
		if ( !isSupportedFormat(videoFormat)){
			ms_error("[MSMFoundationCapUwp] The Video format is not supported by the filter : %s. Trying to force to MFVideoFormat_NV12", pixFmtToString(videoFormat));
			videoFormat = MFVideoFormat_NV12;
		}
		mediaFormat = configs.getMediaConfiguration(&videoFormat, &frameWidth, &frameHeight,&pFps );
		if(mediaFormat){
			MediaFrameFormat^ currentMediaFormat = mSource->CurrentFormat;
			if( currentMediaFormat){
			   doSet = !(mediaFormat->MajorType == currentMediaFormat->MajorType &&
				   mediaFormat->Subtype == currentMediaFormat->Subtype &&
				   mediaFormat->FrameRate->Numerator == currentMediaFormat->FrameRate->Numerator &&
				   mediaFormat->FrameRate->Denominator == currentMediaFormat->FrameRate->Denominator &&
				   ((mediaFormat->VideoFormat == nullptr && currentMediaFormat->VideoFormat == nullptr) ||
					(mediaFormat->VideoFormat != nullptr && currentMediaFormat->VideoFormat != nullptr &&
					 mediaFormat->VideoFormat->Width == currentMediaFormat->VideoFormat->Width &&
					mediaFormat->VideoFormat->Height == currentMediaFormat->VideoFormat->Height)));
			}
		}else
			hr = -1;
	}
	auto configsStr = configs.toString();
	if(doSet && SUCCEEDED(hr)) {
		mCurrentTask = mCurrentTask.then([this, mediaFormat, videoFormat, frameWidth, frameHeight, pFps, configsStr
											, requestedVideoFormat, requestedFrameWidth, requestedFrameHeight]() {
			bool_t restartCamera = mRunning && mSource;
			if (restartCamera)
				create_task(stopAsync()).wait();
			EnterCriticalSection(&mCriticalSection);
			try {
				if(mSource)
					create_task(mSource->SetFormatAsync(mediaFormat)).wait();
			}
			catch (Platform::Exception^ e) {
				std::wstring wsstrResult(e->Message->Data());
				ms_warning("[MSMFoundationCapUwp] SetFormatAsync failed : %s [%X]", make_string(wsstrResult).c_str(), e->HResult);
				ms_warning("%s", configsStr.c_str());
				mFmtChanged = TRUE;
			}
			setInternalFormat(videoFormat, frameWidth, frameHeight, pFps);
			LeaveCriticalSection(&mCriticalSection);
			if (restartCamera)
				create_task(startAsync()).wait();
			mFmtChanged = mFmtChanged || (videoFormat != requestedVideoFormat || requestedFrameWidth != frameWidth || requestedFrameHeight != frameHeight);
			mNewFormatTakenAccount = !mFmtChanged;// Block new frames
		});
	}else if(!SUCCEEDED(hr)) {
		ms_error("[MSMFoundationCapUwp] Cannot set the video format : %dx%d : %s, %f fps. [%X]", frameWidth, frameHeight, pixFmtToString(videoFormat), pFps, hr);
		ms_error("%s", configsStr.c_str());
	}
	LeaveCriticalSection(&mCriticalSection);
	configs.clean();
	return hr;
}

static MSMFoundationCap* ms_mfoundation_new(){
	return new MSMFoundationUwpImpl();
}
static void ms_mfoundationcap_detect(MSWebCamManager *manager) {
	HANDLE eventCompleted = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	if (!eventCompleted) {
		ms_error("[MSMFoundationCapUwp] Could not create camera detection event [%X]", GetLastError());
		return;
	}
	IAsyncOperation<DeviceInformationCollection ^> ^ enumOperation =
		DeviceInformation::FindAllAsync(DeviceClass::VideoCapture);
	enumOperation->Completed = ref new AsyncOperationCompletedHandler<DeviceInformationCollection ^>(
		[manager, eventCompleted](IAsyncOperation<DeviceInformationCollection ^> ^ asyncOperation,
								  Windows::Foundation::AsyncStatus asyncStatus) {
			if (asyncStatus == Windows::Foundation::AsyncStatus::Completed) {
				DeviceInformationCollection ^ DeviceInfoCollection = asyncOperation->GetResults();
				if ((DeviceInfoCollection == nullptr) || (DeviceInfoCollection->Size == 0)) {
					ms_error("[MSMFoundationCapUwp] No webcam found");
				} else {
					try {
						for (unsigned int i = 0; i < DeviceInfoCollection->Size; i++) {
							DeviceInformation ^ DeviceInfo = DeviceInfoCollection->GetAt(i);
							char *idStr = NULL;
							char *nameStr = NULL;
							size_t returnlen;
							size_t inputlen = wcslen(DeviceInfo->Name->Data()) + 1;
							nameStr = (char *)ms_malloc(inputlen);
							if (!nameStr ||
								wcstombs_s(&returnlen, nameStr, inputlen, DeviceInfo->Name->Data(), inputlen) != 0) {
								ms_error("[MSMFoundationCapUwp] Cannot convert webcam name to multi-byte string.");
								goto deviceError;
							}
							const wchar_t *id = DeviceInfo->Id->Data();
							inputlen = wcslen(id) + 1;
							idStr = (char *)ms_malloc(inputlen);
							if (!idStr ||
								wcstombs_s(&returnlen, idStr, inputlen, DeviceInfo->Id->Data(), inputlen) != 0) {
								ms_error("[MSMFoundationCapUwp] Cannot convert webcam id to multi-byte string.");
								goto deviceError;
							}
							char *name = bctbx_strdup_printf("%s--%s", nameStr, idStr);
							char *camId = ms_strdup(idStr);
							MSWebCam *cam = ms_web_cam_new(&ms_mfoundationcap_desc);
							cam->name = name;
							cam->data = camId;
							ms_web_cam_manager_add_cam(manager, cam);
							deviceError:
							if(nameStr) ms_free(nameStr);
							if(idStr) ms_free(idStr);
						}
					} catch (Platform::Exception ^ e) {
						ms_error("[MSMFoundationCapUwp] Error of webcam detection");
					}
				}
			} else {
				ms_error("[MSMFoundationCapUwp] Cannot enumerate webcams");
			}
			SetEvent(eventCompleted);
		});
	WaitForSingleObjectEx(eventCompleted, INFINITE, FALSE);
	CloseHandle(eventCompleted);
}
static void ms_mfoundationcap_uinit(MSWebCam *cam){
	if(cam->data)
		ms_free(cam->data);
}
#else

#include <shlwapi.h>	// QITAB
#pragma comment(lib,"Mfplat.lib")	//MFCreateAttributes symbols
#pragma comment(lib,"Mf.lib")	//MFEnumDeviceSources symbols
#pragma comment(lib,"Mfreadwrite.lib")	//MFCreateSourceReaderFromMediaSource symbols
#pragma comment(lib,"shlwapi.lib")	//QISearch symbols

class MFDevices {// Store device description with helper
public:
	IMFActivate ** mDevices;
	IMFAttributes * mAttributes;
	UINT32 mDevicesCount;

	//IReadOnlyDictionary<string, MediaFrameSource> mFrameSources;// UWP

	MFDevices(){
		mDevices = NULL;
		mAttributes = NULL;
		mDevicesCount = 0;
	}
	void clean(){
		if (mAttributes) {
			mAttributes->Release();
			mAttributes = NULL;
		}
		for (DWORD i = 0; i < mDevicesCount; i++) {
			if (&mDevices[i]) {
				mDevices[i]->Release();
				mDevices[i] = NULL;
			}
		}
		CoTaskMemFree(mDevices);
		mDevicesCount = 0;
	}
	HRESULT getDevices();
	
};
HRESULT MFDevices::getDevices(){
		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		UINT32 count = 0;
		if (FAILED(hr)) {
			ms_error("[MSMFoundationCap] Cannot get devices because of failed CoInitialize [%X]", hr);
			return hr;
		}
		// Create an attribute store to specify enumeration parameters.
		
		hr = MFCreateAttributes(&mAttributes, 1);
		if (FAILED(hr)) {
			ms_error("[MSMFoundationCap] Cannot get devices due to create enumeration attributes [%X]", hr);
			clean();
			return hr;
		}
		//The attribute to be requested is devices that can capture video
		hr = mAttributes->SetGUID( MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID );
		if (FAILED(hr)) {
			ms_error("[MSMFoundationCap] Cannot get devices due to capture attribute [%X]", hr);
			clean();
			return hr;
		}
		//Enummerate the video capture devices
		hr =  MFEnumDeviceSources(mAttributes, &mDevices, &mDevicesCount);//[desktop apps only]
		if (FAILED(hr)) {
			ms_error("[MSMFoundationCap] Cannot enumerate capture devices from MFEnumDeviceSources [%X]", hr);
			clean();
		}
		return hr;
	}
class ConfigurationManagerDesktop : public ConfigurationManager<IMFMediaType *>{
public:
	std::vector<IMFMediaType *> mMediaTypes;// Used to cleanup
	ConfigurationManagerDesktop(){}
	~ConfigurationManagerDesktop(){};

	void clean(){
		for(size_t i = 0 ; i < mMediaTypes.size() ; ++i)
			mMediaTypes[i]->Release();
	}
	void setMediaTypes(IMFSourceReader* source){
		IMFMediaType * mediaType = NULL;
		HRESULT hr;
		UINT32 width, height, fpsNumerator, fpsDenominator;
		GUID videoType;
		int mediaCount = 0, acceptableMediaCount = 0;
		std::vector<std::string> traces;
		for(DWORD i = 0 ; source->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &mediaType) != MF_E_NO_MORE_TYPES ; ++i ) {	
			++mediaCount;
			if(mediaType) {
				++acceptableMediaCount;
				hr = mediaType->GetGUID(MF_MT_SUBTYPE, &videoType);
				if(SUCCEEDED(hr) && MSMFoundationCap::isSupportedFormat(videoType)){// Get frame size from media type
					traces.push_back("Got a good format : " +std::string(MSMFoundationCap::pixFmtToString(videoType)));
					hr = MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &width, &height);
				}else{
					if(SUCCEEDED(hr))
						traces.push_back("Bad Format : " +std::string(MSMFoundationCap::pixFmtToString(videoType)));
					else
						traces.push_back("Cannot get Format from MF_MT_SUBTYPE: [" +std::to_string(hr)+"]");
					hr = -1;
				}
				if (SUCCEEDED(hr)){
					traces.push_back("Attributes could be retrieved: "+std::to_string(width) +"/"+std::to_string(height) );
					hr = MFGetAttributeRatio(mediaType, MF_MT_FRAME_RATE , &fpsNumerator, &fpsDenominator);
				}else{
					traces.push_back("Cannot get attributes (o previous error) [" +std::to_string(hr)+"]");
				}
				if (SUCCEEDED(hr)) {
					mSortedList[width][height][(int)((static_cast<float>(fpsNumerator)/fpsDenominator) * 100.0f)][videoType] = mediaType;
					mMediaTypes.push_back(mediaType);
				}else {
					traces.push_back("Cannot get ratio(or previous error) [" +std::to_string(hr)+"]");
					mediaType->Release();
					mediaType = NULL;
				}
			}
		}
		// Debug mode
		if( mSortedList.size() == 0)
		{
			ms_error("There are no available configurations. Media:%d UsableMedia:%d. Traces:", mediaCount, acceptableMediaCount);
			for(size_t i = 0 ; i < traces.size() ; ++i)
				ms_error("[MSMFoundationCap] %s", traces[i].c_str());
		}
	}
};

MSMFoundationDesktopImpl::MSMFoundationDesktopImpl() : MSMFoundationCap() {
	InitializeConditionVariable (&mIsFlushed);
	mStride = mWidth;
	mReferenceCount = 1;
	mSourceReader = NULL;
	mRunning = FALSE;
	mPlaneSize =  mHeight * abs(mStride);
}
MSMFoundationDesktopImpl::~MSMFoundationDesktopImpl() {
	stop(2000);
	if (mSourceReader) {
		mSourceReader->Release();
		mSourceReader = NULL;
	}
}

void MSMFoundationDesktopImpl::activate() {
}

void MSMFoundationDesktopImpl::start() {
	MFDevices devices;
	HRESULT hr = devices.getDevices();
	UINT32 currentDeviceIndex = 0;
	bool_t found = FALSE;

	if( FAILED(hr)) return;
	while(!found && currentDeviceIndex < devices.mDevicesCount) {
		WCHAR *nameString = NULL;		
		UINT32 cchName; 
		hr = devices.mDevices[currentDeviceIndex]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &nameString, &cchName);
		if (SUCCEEDED(hr)) {
			// We don't need null-terminated character because we are using std::string for convertion.
			size_t inputlen = wcslen(nameString);
			UINT currentCodePage = GetACP();
			int sizeNeeded = WideCharToMultiByte(currentCodePage, 0, nameString, (int)inputlen, NULL, 0, NULL, NULL);
			std::string strConversion( sizeNeeded, '\0');
			if(WideCharToMultiByte(currentCodePage, 0, nameString, (int)inputlen, &strConversion[0], (int)sizeNeeded, NULL, NULL) && mDeviceName == strConversion) {
				found = TRUE;
			}else
				++currentDeviceIndex;
		}else
			++currentDeviceIndex;
		CoTaskMemFree(nameString);
	}
	if(found){
		if(!mSourceReader){
			hr = setSourceReader(devices.mDevices[currentDeviceIndex]);
			ms_average_fps_init(&mAvgFps,"[MSMFoundationCap] fps=%f");
		}
	}else{
		ms_error("[MSMFoundationCap] Device cannot be activated because friendly name has not been found : '%s' in the current device list:", mDeviceName.c_str());
		for(UINT32 i = 0 ; i <  devices.mDevicesCount ; ++i){
			WCHAR *nameString = NULL;		
			UINT32 cchName; 
			hr = devices.mDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &nameString, &cchName);
			if (SUCCEEDED(hr)) {
				// We don't need null-terminated character because we are using std::string for convertion.
				size_t inputlen = wcslen(nameString);
				UINT currentCodePage = GetACP();
				int sizeNeeded = WideCharToMultiByte(currentCodePage, 0, nameString, (int)inputlen, NULL, 0, NULL, NULL);
				std::string strConversion( sizeNeeded, 0 );
				if(WideCharToMultiByte(currentCodePage, 0, nameString, (int)inputlen, &strConversion[0], (int)sizeNeeded, NULL, NULL))
					ms_error("[MSMFoundationCap] %s", strConversion.c_str());
			}
		}
	}
	devices.clean();
	if(mSourceReader && !mRunning) {
		EnterCriticalSection(&mCriticalSection);
		HRESULT hr = mSourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, NULL, NULL, NULL);  // Ask for the first sample.
		if (SUCCEEDED(hr)){
			mRunning = TRUE;// Inside critical section to ensure to not miss the first frame
		}else {
			ms_error("[MSMFoundationCap] Cannot start reading from Camera : %X", hr);
		}
		LeaveCriticalSection(&mCriticalSection);
	}else
		ms_error("[MSMFoundationCap] Cannot start reading from Camera because of source reader not activated.");
}

void MSMFoundationDesktopImpl::stop(const int& pWaitStop) {
	HRESULT hr;
	if(mSourceReader && mRunning){
		EnterCriticalSection(&mCriticalSection);
		mRunning = FALSE;
		hr = mSourceReader->Flush(MF_SOURCE_READER_FIRST_VIDEO_STREAM);// Flush the video stream
		if( SUCCEEDED(hr))
			SleepConditionVariableCS (&mIsFlushed, &mCriticalSection, pWaitStop);// wait for emptying queue. This is done asynchrounsly as the Callback on flush has been implemented
		else
			ms_error("[MSMFoundationCap] Cannot flush device, %X", hr);
		LeaveCriticalSection(&mCriticalSection);
	}
	if (mFrameData) {
		freemsg(mFrameData);
		mFrameData = NULL;
	}
}

void MSMFoundationDesktopImpl::deactivate() {
	stop(1000);
	EnterCriticalSection(&mCriticalSection);
	if (mSourceReader) {
		mSourceReader->Release();
		mSourceReader = NULL;
	}
	
	LeaveCriticalSection(&mCriticalSection);
	ms_message("[MSMFoundationCap] Frames count : %d samples, %d processed", mSampleCount, mProcessCount);
	mSampleCount = mProcessCount = 0;
}

//-------------------------------------------------------------------------------------------------------------

//From IUnknown
STDMETHODIMP MSMFoundationDesktopImpl::QueryInterface(REFIID riid, void** ppvObject) {
#ifndef MS2_WINDOWS_UWP	
	static const QITAB qit[] = { QITABENT(MSMFoundationDesktopImpl, IMFSourceReaderCallback),{ 0 }, };
	return QISearch(this, qit, riid, ppvObject);
#else
	return S_OK;
#endif
}
//From IUnknown

ULONG MSMFoundationDesktopImpl::Release() {
	ULONG count = InterlockedDecrement(&mReferenceCount);
	if (count == 0)
		delete this;
	// For thread safety
	return count;
}
//From IUnknown
ULONG MSMFoundationDesktopImpl::AddRef() { return InterlockedIncrement(&mReferenceCount); }

//Method from IMFSourceReaderCallback
STDMETHODIMP MSMFoundationDesktopImpl::OnEvent(DWORD, IMFMediaEvent * mediaEvent) { return S_OK; }
//Method from IMFSourceReaderCallback
STDMETHODIMP MSMFoundationDesktopImpl::OnFlush(DWORD) {
	WakeConditionVariable (&mIsFlushed);// Wakeup threads that are waiting for the flush
	return S_OK; 
}

HRESULT MSMFoundationDesktopImpl::setMediaConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float pFps){
	HRESULT hr = S_OK;
	IMFMediaType *mediaType= NULL;
	ConfigurationManagerDesktop configs;
	LONG stride = frameWidth; // Set stride to width as default
	bool_t doSet = TRUE;
	GUID requestedVideoFormat = videoFormat;
	UINT32 requestedFrameWidth = frameWidth;
	UINT32 requestedFrameHeight = frameHeight;

	EnterCriticalSection(&mCriticalSection);
	if( mSourceReader){
		configs.setMediaTypes(mSourceReader);
		if ( !isSupportedFormat(videoFormat)){
			ms_error("[MSMFoundationCap] The Video format is not supported by the filter : %s. Trying to force to MFVideoFormat_NV12", pixFmtToString(videoFormat));
			videoFormat = MFVideoFormat_NV12;
		}
		mediaType = configs.getMediaConfiguration(&videoFormat, &frameWidth, &frameHeight,&pFps );
		if(mediaType){
			IMFMediaType * currentMediaType = NULL;
			DWORD equalFlags = 0;
			HRESULT hrCurrentMedia = mSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &currentMediaType);
			if( SUCCEEDED(hrCurrentMedia) && currentMediaType){
				currentMediaType->IsEqual(mediaType, &equalFlags);// This is the same media type. Don't need to change it.
				doSet = (equalFlags != 0);
				currentMediaType->Release();
			}
			if( doSet ){
				hr = mSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, mediaType);
				if( hr == MF_E_INVALIDREQUEST){//One or more sample requests are still pending. Flush the device, restart it and try setting format 
					ms_message("[MSMFoundationCap] Restarting device with a new configuration : %dx%d : %s, %f fps", frameWidth, frameHeight, pixFmtToString(videoFormat), pFps);
					stop(2000);
					hr = mSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, mediaType);
					if (SUCCEEDED(hr)) {
						start();
					}else
						ms_error("[MSMFoundationCap] Cannot restart device with the new configuration [%X]", hr);
				}
				if(SUCCEEDED(hr)) getStride(mediaType, &stride);
			}
		}else{
			hr = -1;
			ms_warning("[MSMFoundationCap] No available configuration have been found from this list : \n%s", configs.toString().c_str());			
		}
	}
	if(doSet && SUCCEEDED(hr)) {
		bool_t formatChanged = FALSE;
		setVideoFormat(videoFormat);
		mFps = pFps;
		if(frameWidth != mWidth || frameHeight != mHeight || stride != mStride){
			if(mFrameData){
				freemsg(mFrameData);
				mFrameData = NULL;
			}
			mWidth = frameWidth;
			mHeight = frameHeight;
			mStride = stride;
			mPlaneSize = mHeight * abs(mStride);// Details : mWidth * mHeight * abs(mStride) / mWidth;
			formatChanged = TRUE;
			mFmtChanged = videoFormat != requestedVideoFormat || requestedFrameWidth != frameWidth || requestedFrameHeight != frameHeight;
			mNewFormatTakenAccount = FALSE;// Block new frames
		}
		if(mSourceReader)
			ms_message("[MSMFoundationCap] %s the video format : %dx%d : %s, %f fps", (formatChanged ? "Changed" : "Keep"), mWidth, mHeight, pixFmtToString(mVideoFormat), mFps);
		else
			ms_message("[MSMFoundationCap] %s the video format without Reader : %dx%d : %s, %f fps", (formatChanged ? "Changed" : "Keep"), mWidth, mHeight, pixFmtToString(mVideoFormat), mFps);
		if(mFmtChanged)
			ms_message("%s", configs.toString().c_str());
	}else if(!SUCCEEDED(hr) ){
		ms_error("[MSMFoundationCap] Cannot set the video format : %dx%d : %s, %f fps. [%X]", frameWidth, frameHeight, pixFmtToString(videoFormat), pFps, hr);
		ms_message("%s", configs.toString().c_str());
	}
	LeaveCriticalSection(&mCriticalSection);
	configs.clean();
	return hr;
}

HRESULT MSMFoundationDesktopImpl::setSourceReader(IMFActivate *device) {
	HRESULT hr = S_OK;
	IMFMediaSource *source = NULL;
	IMFAttributes *attributes = NULL;

	EnterCriticalSection(&mCriticalSection);
	hr = device->ActivateObject(__uuidof(IMFMediaSource), (void**)&source);
	if (SUCCEEDED(hr)) //Allocate attributes
		hr = MFCreateAttributes(&attributes, 2);
	else
		ms_error("[MSMFoundationCap] Cannot create source reader because of failing attributes allocation [%X]", hr);
	if (SUCCEEDED(hr)) //get attributes
		hr = attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);
	else
		ms_error("[MSMFoundationCap] Cannot create source reader because of failing attributes setting [%X]", hr);	
	if (SUCCEEDED(hr)) // Set the callback pointer.
		hr = attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);	
	else
		ms_error("[MSMFoundationCap] Cannot create source reader because of failing callback initialization [%X]", hr);
	if (SUCCEEDED(hr)) //Create the source reader
		hr = MFCreateSourceReaderFromMediaSource(source, attributes, &mSourceReader);
	else
		ms_error("[MSMFoundationCap] Cannot create source reader from media source [%X]", hr);
	if (SUCCEEDED(hr)){  // Try to find a suitable output type.
		hr = setMediaConfiguration(mVideoFormat, mWidth, mHeight, mFps);
	}
	if( FAILED(hr) ) {
		if( mSourceReader ) {
			mSourceReader->Release();
			mSourceReader = NULL;
		}
	}
// Cleanup
	if (source) { source->Release(); source = NULL; }
	if (attributes) { attributes->Release(); attributes = NULL; }
	LeaveCriticalSection(&mCriticalSection);

	return hr;
}
HRESULT MSMFoundationDesktopImpl::restartWithNewConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float pFps){
	ms_message("[MSMFoundationCap] Restarting device with a new configuration : %dx%d : %s, %f fps", frameWidth, frameHeight, pixFmtToString(videoFormat), pFps);
	HRESULT hr = S_OK;
	stop(2000);
	hr = setMediaConfiguration(videoFormat,frameWidth, frameHeight,pFps );
	if( SUCCEEDED(hr)) {
		start();
	}else
		ms_error("[MSMFoundationCap] Cannot restart device with the new configuration [%X]", hr);
	return hr;
}

HRESULT MSMFoundationDesktopImpl::getStride(IMFMediaType *pType, LONG * stride){
	HRESULT hr = S_OK;
	LONG tempStride = 0;

	//Get the stride for this format so we can calculate the number of bytes per pixel
	hr = pType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&tempStride); // Try to get the default stride from the media type.
	if (FAILED(hr)) {
		UINT32 width, height;
		//Setting this atribute to NULL we can obtain the default stride
		GUID subtype = GUID_NULL;
		// Obtain the subtype
		hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
		//obtain the width and height
		if (SUCCEEDED(hr))
			hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
#ifndef MS2_WINDOWS_UWP		
		//Calculate the stride based on the subtype and width
		if (SUCCEEDED(hr))
			hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, width, &tempStride); //[desktop apps only]
		// set the attribute so it can be read
		if (SUCCEEDED(hr))
			(void)pType->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(tempStride));
#endif		
	}
	if( SUCCEEDED(hr) && stride)
		*stride = tempStride;
	return hr;
}

//Method from IMFSourceReaderCallback
HRESULT MSMFoundationDesktopImpl::OnReadSample(HRESULT status, DWORD streamIndex, DWORD streamFlags, LONGLONG timeStamp, IMFSample *sample) {
	HRESULT hr = status;
	IMFMediaBuffer *mediaBuffer = NULL;

	EnterCriticalSection(&mCriticalSection);
	if(mRunning) {
		if (SUCCEEDED(hr)) {
			if (sample) {// Get the video frame buffer from the sample.
				hr = sample->GetBufferByIndex(0, &mediaBuffer);
				if (SUCCEEDED(hr)) {
					++mSampleCount;
					BYTE* data;
					DWORD length = 0;
					try{
						mediaBuffer->Lock(&data, NULL, &length);
						processFrame(data, length, mStride);
					}catch(...){

					}
				}
			}
		}	
		if (SUCCEEDED(hr) && mSourceReader){ // Request the next frame.
			hr = mSourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, NULL, NULL, NULL);
		}
		if (FAILED(hr))
			ms_error("[MSMFoundationCap] Cannot read sample : %X", hr);
		if (mediaBuffer) { mediaBuffer->Release(); mediaBuffer = NULL; }
	}
	LeaveCriticalSection(&mCriticalSection);
	
	return hr;
}

void MSMFoundationDesktopImpl::safeRelease(){
	Release();
}
static MSMFoundationCap* ms_mfoundation_new(){
	return new MSMFoundationDesktopImpl();
}
static void ms_mfoundationcap_detect(MSWebCamManager *manager) {
	MFDevices devices;
	if (FAILED(devices.getDevices())) return;
	for (UINT32 i = 0; i < devices.mDevicesCount; ++i) { // Get the human-friendly name of the device
		WCHAR *nameString = NULL;		
		UINT32 cchName; 
		HRESULT hr = devices.mDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME , &nameString, &cchName);
		if (SUCCEEDED(hr)) {
			// We don't need null-terminated character because we are using std::string for convertion.
			size_t inputlen = wcslen(nameString);
			UINT currentCodePage = GetACP();
			int sizeNeeded = WideCharToMultiByte(currentCodePage, 0, nameString, (int)inputlen, NULL, 0, NULL, NULL);
			std::string strConversion( sizeNeeded, 0 );
			char *nameStr = NULL;
			if(WideCharToMultiByte(currentCodePage, 0, nameString, (int)inputlen, &strConversion[0], sizeNeeded, NULL, NULL)){
				nameStr = (char *)ms_malloc(strConversion.length()+1 );
				strcpy(nameStr, strConversion.c_str());
				nameStr[strConversion.length()] = '\0';
			}
			if(!nameStr){
				ms_error("[MSMFoundationCap] Cannot convert webcam name to multi-byte string.");
				ms_free(nameStr);
			}else {
				MSWebCam *cam = ms_web_cam_new(&ms_mfoundationcap_desc);
				cam->name = nameStr;
				ms_web_cam_manager_add_cam(manager, cam);
			}
		}
		CoTaskMemFree(nameString);
	}
	devices.clean();
}

static void ms_mfoundationcap_uinit(MSWebCam *cam){
}
#endif	// MS2_WINDOWS_UWP
