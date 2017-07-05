/*
mswasapi_writer.h

mediastreamer2 library - modular sound and video processing and streaming
Windows Audio Session API sound card plugin for mediastreamer2
Copyright (C) 2010-2013 Belledonne Communications, Grenoble, France

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


#pragma once


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msqueue.h"

#include "mswasapi.h"


class MSWASAPIWriter
#ifdef MS2_WINDOWS_UNIVERSAL
	: public RuntimeClass< RuntimeClassFlags< ClassicCom >, FtmBase, IActivateAudioInterfaceCompletionHandler >
#endif
{
public:
	MSWASAPIWriter();
	virtual ~MSWASAPIWriter();

	void init(LPCWSTR id, MSFilter *f);
	int activate();
	int deactivate();
	bool isStarted() { return mIsStarted; }
	void start();
	void stop();
	int feed(MSFilter *f);

	int getRate() { return mRate; }
	int getNChannels() { return mNChannels; }
	float getVolumeLevel();
	void setVolumeLevel(float volume);

#ifdef MS2_WINDOWS_UNIVERSAL
	void setAsNotInstantiated() { smInstantiated = false; }

	// IActivateAudioInterfaceCompletionHandler
	STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation *operation);
#endif

private:
	static void * feedThread(void *p);
	void * feedThread();
	void createBufferizer(MSFilter *f);
	void drop(MSFilter *f);
	HRESULT configureAudioClient();

	static bool smInstantiated;
	HANDLE mSamplesRequestedEvent;
	ms_thread_t mThread;
	ms_mutex_t mThreadMutex;
#ifdef MS2_WINDOWS_UNIVERSAL
	Platform::String^ mRenderId;
	HANDLE mActivationEvent;
#else
	LPCWSTR mRenderId;
#endif
#if defined(MS2_WINDOWS_PHONE) || defined(MS2_WINDOWS_UNIVERSAL)
	IAudioClient2 *mAudioClient;
#else
	IAudioClient *mAudioClient;
#endif
	IAudioRenderClient *mAudioRenderClient;
	ISimpleAudioVolume *mVolumeControler;
	MSFlowControlledBufferizer *mBufferizer;
	UINT32 mBufferFrameCount;
	bool mIsInitialized;
	bool mIsActivated;
	bool mIsStarted;
	bool mIsReadyToWrite;
	int mRate;
	int mNChannels;
};


#ifdef MS2_WINDOWS_UNIVERSAL
#define MSWASAPI_WRITER(w) ((MSWASAPIWriterType)((MSWASAPIWriterPtr)(w))->writer)
typedef ComPtr<MSWASAPIWriter> MSWASAPIWriterType;

struct MSWASAPIWriterWrapper
{
	MSWASAPIWriterType writer;
};

typedef struct MSWASAPIWriterWrapper* MSWASAPIWriterPtr;
#else
#define MSWASAPI_WRITER(w) ((MSWASAPIWriterType)(w))
typedef MSWASAPIWriter* MSWASAPIWriterPtr;
typedef MSWASAPIWriter* MSWASAPIWriterType;
#endif

MSWASAPIWriterPtr MSWASAPIWriterNew();
void MSWASAPIWriterDelete(MSWASAPIWriterPtr ptr);
