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
#include <stdlib.h>	// wchar => char
#include <stdio.h>

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/mswebcam.h"

class MSMFoundationCap{
protected:	
	CRITICAL_SECTION mCriticalSection;
	MSYuvBufAllocator * mAllocator;
	mblk_t * mFrameData;		// Frame to send to MS Queue when it request one
	bool_t mRunning;	// The reading process is running or not. Set it to FALSE to stop it.
	float mFps;
	unsigned int mSampleCount, mProcessCount;
	bool_t mFmtChanged;

public:
	MSMFoundationCap();
	virtual ~MSMFoundationCap();
	virtual void safeRelease();
//-----------------------------------------  Getters/Setters
	
	void setVSize(MSVideoSize vsize);
	virtual void setWebCam(MSWebCam* webcam);
	void setFps(const float &fps);
	float getFps() const;
	int getDeviceOrientation() const;
	void setDeviceOrientation(int orientation);
	void setVideoFormat(const GUID &videoFormat);				// Set videoformat and update MSPixFmt format
	
//----------------------------------------  Mediastreamer Interface
	
	virtual void activate();
	virtual void start();
	virtual void feed(MSFilter * filter);
	virtual void stop(const int& pWaitStop);
	virtual void deactivate();
	
//----------------------------------------  Actions
	
	virtual HRESULT setMediaConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float FPS);// Set internal data and do updates
	virtual HRESULT restartWithNewConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float pFps);
	bool_t isTimeToSend(uint64_t tickerTime);// Wheck if we have to put the frame in ms queue
	static bool_t isSupportedFormat(const GUID &videoFormat);
	void processFrame(byte* inputBytes, DWORD inputCapacity, int inputStride );// Read inputs and store data into frameData
	
//----------------------------------------  Variables
	
	GUID mVideoFormat;  // MFVideoFormat_NV12 is only supported. Another filter will lead to trying to force the media to MFVideoFormat_NV12
	int mPixelFormat;	// Store MSPixFmt
	int mOrientation;
	UINT32 mWidth;
	UINT32 mHeight;
	std::string mDeviceName;
	MSAverageFPS mAvgFps;
	MSFrameRateController mFramerateController;	
	
//----------------------------------------  Tools
	static const char *pixFmtToString(const GUID &fmt);	// Helper to return the name of the format
	static const GUID& pixStringToGuid(const std::string& type);// Helper to convert GUID
};
#if defined( MS2_WINDOWS_UWP )

#include <ppltasks.h>

class MSMFoundationUwpImpl : public MSMFoundationCap{
	
//----------------------------------------  Variables
	
		bool mStreaming = false;
		Windows::Media::Capture::MediaCaptureSharingMode mCameraSharingMode;
		Platform::Agile<Windows::Media::Capture::MediaCapture^> mMediaCapture;
		Windows::Media::Capture::Frames::MediaFrameSource^ mSource;
		Windows::Media::Capture::Frames::MediaFrameReader^ mReader;
		Windows::Foundation::EventRegistrationToken mFrameArrivedToken;
// Frame Source	
		Platform::String^ mId;
		Platform::String^ mDisplayName;
		Windows::Media::Capture::Frames::MediaFrameSourceGroup^ mSourceGroup;// Store available formats
//--------------------------------------------------------		
public:
	MSMFoundationUwpImpl();
	~MSMFoundationUwpImpl();
	virtual void safeRelease();
	
//-----------------------------------------  Getters/Setters
	
	virtual void setWebCam(MSWebCam* webcam);
	
//----------------------------------------  Mediastreamer Interface	
	virtual void activate();// Set the current Capture device
	virtual void start();
	virtual void stop(const int& pWaitStop);
	virtual void deactivate();

//----------------------------------------  Actions
	
	HRESULT setMediaConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float pFps);
	concurrency::task<void> startReaderAsync(); // Starts reading frames from the current reader. Must be called from the UI thread.
	concurrency::task<void> stopReaderAsync(); // Stops reading from the frame reader and disposes of the reader
	void disposeMediaCapture(); // Closes the MediaCapture object. Must be called from the UI thread.
	concurrency::task<void> createReaderAsync(); //Creates a reader from the current frame source and registers to handle its frame events. Must be called from the UI thread.
	concurrency::task<bool> tryInitializeCaptureAsync(); // Initialize the media capture object. Must be called from the UI thread.
	void updateFrameSource();// Updates the current frame source to the one corresponding to the current selection.
	void processFrame(Windows::Media::Capture::Frames::MediaFrameReference^ frame);// Frame processing
	void setFrameSource(Windows::Media::Capture::Frames::MediaFrameSourceGroup^ sourceGroup);
	void reader_FrameArrived(Windows::Media::Capture::Frames::MediaFrameReader^ reader, Windows::Media::Capture::Frames::MediaFrameArrivedEventArgs^ args);// Handles the frame arrived event by converting the frame to a dislayable format and rendering it to the screen.

//----------------------------------------	Special Microsoft Section
	
	concurrency::task<void> mCurrentTask = concurrency::task_from_result();
	
};
#else
#include <mfidl.h>	// IMFMediaSource (here for mfreadwrite.h)
#include <mfreadwrite.h>	// IMFSourceReaderCallback

class MSMFoundationDesktopImpl : public MSMFoundationCap, public IMFSourceReaderCallback {
//----------------------------------------  Variables
	CONDITION_VARIABLE mIsFlushed;
	long mReferenceCount;
	IMFSourceReader* mSourceReader;	// The source
	unsigned int mPlaneSize;	// Optimization to avoid to compute it on each frame
	LONG mStride;				// Stride from media type
public:
	MSMFoundationDesktopImpl();
	~MSMFoundationDesktopImpl();
	virtual void safeRelease();
	
//----------------------------------------  Mediastreamer Interface		
	
	virtual void start();
	virtual void stop(const int& pWaitStop);
	virtual void activate();
	virtual void deactivate();
	
//----------------------------------------  Actions
	
	HRESULT setMediaConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float FPS);// Set internal data and do updates
	HRESULT setCaptureDevice(const std::string& name);	// Set the current Capture device name
	HRESULT setSourceReader(IMFActivate *device);		// Set the reader source
	HRESULT restartWithNewConfiguration(GUID videoFormat, UINT32 frameWidth, UINT32 frameHeight, float pFPS);
	HRESULT getStride(IMFMediaType *pType, LONG * stride);// Update media type if it hasn't any stride and put it in parameter
	bool_t isTimeToSend(uint64_t tickerTime);// Wheck if we have to put the frame in ms queue

//----------------------------------------	Special Microsoft Section

	// the class must implement the methods from IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	//  the class must implement the methods from IMFSourceReaderCallback
	STDMETHODIMP OnReadSample(HRESULT status, DWORD streamIndex, DWORD streamFlags, LONGLONG timeStamp, IMFSample *sample);
	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *);
	STDMETHODIMP OnFlush(DWORD);
};
#endif//MS2_WINDOWS_UWP
#endif //_MS_MEDIA_FOUNDATION_CAP_H
