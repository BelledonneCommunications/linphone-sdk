/*
 * Copyright (c) 2020 Belledonne Communications SARL.
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

#ifndef _MS_MEDIA_FOUNDATION_CAP_H
#define _MS_MEDIA_FOUNDATION_CAP_H

#ifndef WINVER
 // Set WINVER to get MF_DEVSOURCE attributes
#define WINVER _WIN32_WINNT_WIN7
#endif

#define _WINSOCKAPI_
#include <windows.h>
#include <mfidl.h>	// IMFMediaSource
#include <mfreadwrite.h>	// IMFSourceReaderCallback
#include <mfapi.h>	// MFCreateAttributes
#include <mfobjects.h>	// ActivateObject
#include <stdlib.h>	// wchar => char
#include <stdio.h>

#include <shlwapi.h>	// QITAB

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"

class MSMFoundationCap : public IMFSourceReaderCallback {
	CRITICAL_SECTION mCriticalSection;
	CONDITION_VARIABLE mIsFlushed;
	long mReferenceCount;
	IMFSourceReader* mSourceReader;	// The source
	MSYuvBufAllocator * mAllocator;
	bool_t mRunning;	// The reading process is running or not. Set it to FALSE to stop it.
	float mFps;
	unsigned int mPlaneSize;	// Optimization to avoid to compute it on each frame
	LONG mStride;				// Stride from media type
	mblk_t * mFrameData;		// Frame to send to MS Queue when it request one
	unsigned int mSampleCount, mProcessCount;
public:
	MSMFoundationCap();
	~MSMFoundationCap();

//-----------------------------------------  Getters/Setters

	void setVSize(MSVideoSize vsize);
	void setDeviceName(const std::string &name);
	void setFps(const float &fps);
	float getFps() const;
	int getDeviceOrientation() const;
	void setDeviceOrientation(int orientation);
	void setVideoFormat(const GUID &videoFormat);				// Set videoformat and update MSPixFmt format

//----------------------------------------  Mediastreamer Interface

	void activate();
	void start();
	void feed(MSFilter * filter);
	void stop(const int& pWaitStop);
	void deactivate();

//----------------------------------------  Actions

	HRESULT setCaptureDevice(const std::string& name);	// Set the current Capture device name
	HRESULT setSourceReader(IMFActivate *device);		// Set the reader source
	HRESULT restartWithNewConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float pFPS);
	HRESULT getStride(IMFMediaType *pType, LONG * stride);// Update media type if it hasn't any stride and put it in parameter
	HRESULT setMediaConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float FPS);// Set internal data and do updates
	bool_t isTimeToSend(uint64_t tickerTime);// Wheck if we have to put the frame in ms queue
	bool_t isSupportedFormat(const GUID &videoFormat)const;

//----------------------------------------  Variables

	GUID mVideoFormat;  // MFVideoFormat_NV12 is only supported. Another filter will lead to trying to force the media to MFVideoFormat_NV12
	int mPixelFormat;	// Store MSPixFmt
	int mOrientation;
	UINT32 mWidth;
	UINT32 mHeight;
	std::string mDeviceName;
	MSAverageFPS mAvgFps;
	MSFrameRateController mFramerateController;

//----------------------------------------	Special Microsoft Section

	// the class must implement the methods from IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	//  the class must implement the methods from IMFSourceReaderCallback
	STDMETHODIMP OnReadSample(HRESULT status, DWORD streamIndex, DWORD streamFlags, LONGLONG timeStamp, IMFSample *sample);
	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *);
	STDMETHODIMP OnFlush(DWORD);
	static const char *pixFmtToString(const GUID &fmt);	// Helper to return the name of the format
};

#endif //_MS_MEDIA_FOUNDATION_CAP_H
