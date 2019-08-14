/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
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


#pragma once


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"

#include "mswasapi.h"


class MSWASAPIReader
#ifdef MS2_WINDOWS_UNIVERSAL
	: public RuntimeClass< RuntimeClassFlags< ClassicCom >, FtmBase, IActivateAudioInterfaceCompletionHandler >
#endif
{
public:
	MSWASAPIReader(MSFilter *filter);
	virtual ~MSWASAPIReader();

	void init(LPCWSTR id);
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
	void silence(MSFilter *f);

	static bool smInstantiated;
#ifdef MS2_WINDOWS_UNIVERSAL
	Platform::String^ mCaptureId;
	HANDLE mActivationEvent;
#else
	LPCWSTR mCaptureId;
#endif
#if defined(MS2_WINDOWS_PHONE) || defined(MS2_WINDOWS_UNIVERSAL)
	IAudioClient2 *mAudioClient;
#else
	IAudioClient *mAudioClient;
#endif
	IAudioCaptureClient *mAudioCaptureClient;
	ISimpleAudioVolume *mVolumeControler;
	UINT32 mBufferFrameCount;
	bool mIsInitialized;
	bool mIsActivated;
	bool mIsStarted;
	int mRate;
	int mNChannels;
	MSFilter *mFilter;
	MSTickerSynchronizer *mTickerSynchronizer;
	uint64_t mReadFrames;
};


#ifdef MS2_WINDOWS_UNIVERSAL
#define MSWASAPI_READER(w) ((MSWASAPIReaderType)((MSWASAPIReaderPtr)(w))->reader)
typedef ComPtr<MSWASAPIReader> MSWASAPIReaderType;

struct MSWASAPIReaderWrapper
{
	MSWASAPIReaderType reader;
};

typedef struct MSWASAPIReaderWrapper* MSWASAPIReaderPtr;
#else
#define MSWASAPI_READER(w) ((MSWASAPIReaderType)(w))
typedef MSWASAPIReader* MSWASAPIReaderPtr;
typedef MSWASAPIReader* MSWASAPIReaderType;
#endif

MSWASAPIReaderPtr MSWASAPIReaderNew(MSFilter *f);
void MSWASAPIReaderDelete(MSWASAPIReaderPtr ptr);
