/*
mswasapi_reader.cpp

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

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msticker.h"
#include "mswasapi_reader.h"

#define REFTIME_250MS 2500000

#define RELEASE_CLIENT(client) \
	if (client != NULL) { \
		client->Release(); \
		client = NULL; \
	}
#define FREE_PTR(ptr) \
	if (ptr != NULL) { \
		CoTaskMemFree((LPVOID)ptr); \
		ptr = NULL; \
	}


#ifdef MS2_WINDOWS_UNIVERSAL
bool MSWASAPIReader::smInstantiated = false;
#endif




MSWASAPIReader::MSWASAPIReader(MSFilter *filter)
	: MSWasapi("input"), mAudioCaptureClient(NULL), mVolumeControler(NULL), mBufferFrameCount(0), mIsActivated(false), mIsStarted(false), mFilter(filter) {
	mTickerSynchronizer = ms_ticker_synchronizer_new();
#ifndef MS2_WINDOWS_PHONE
	mActivationEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	if (!mActivationEvent) {
		ms_error("MSWASAPIReader: Could not create activation event of the MSWASAPI audio input interface [%i]", GetLastError());
		return;
	}
#endif
}

MSWASAPIReader::~MSWASAPIReader() {
	RELEASE_CLIENT(mAudioClient);
	
#ifdef MS2_WINDOWS_PHONE
	FREE_PTR(mCaptureId);
#else
	if (mActivationEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(mActivationEvent);
		mActivationEvent = INVALID_HANDLE_VALUE;
	}
#ifdef MS2_WINDOWS_UNIVERSAL
	setInstantiated(false);
#endif
#endif
	ms_ticker_synchronizer_destroy(mTickerSynchronizer);
}

int MSWASAPIReader::activate() {
	HRESULT result;
	REFERENCE_TIME requestedDuration = REFTIME_250MS;
	WAVEFORMATPCMEX proposedWfx;
	WAVEFORMATEX *pUsedWfx = NULL;
	WAVEFORMATEX *pSupportedWfx = NULL;
	DWORD flags = 0;

	if (!mIsInitialized) goto error;

#if defined( MS2_WINDOWS_PHONE )
	flags = AUDCLNT_SESSIONFLAGS_EXPIREWHENUNOWNED | AUDCLNT_SESSIONFLAGS_DISPLAY_HIDE | AUDCLNT_SESSIONFLAGS_DISPLAY_HIDEWHENEXPIRED;
#else
	flags = AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |  AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
#endif

	proposedWfx = buildFormat();
	
	result = mAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX *)&proposedWfx, &pSupportedWfx);
	if (result == S_OK) {
		pUsedWfx = (WAVEFORMATEX *)&proposedWfx;
	} else if (result == S_FALSE) {
		pUsedWfx = pSupportedWfx;
	} else {
		REPORT_ERROR("Audio format not supported by the MSWASAPI audio input interface [%x]", result);
	}
	
	result = mAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, requestedDuration, 0, pUsedWfx, NULL);
	if ((result != S_OK) && (result != AUDCLNT_E_ALREADY_INITIALIZED)) {
		REPORT_ERROR("Could not initialize the MSWASAPI audio input interface [%x]", result);
	}
	mNBlockAlign = pUsedWfx->nBlockAlign;
	mRate = pUsedWfx->nSamplesPerSec;
	mNChannels = pUsedWfx->nChannels;
	result = mAudioClient->GetBufferSize(&mBufferFrameCount);
	REPORT_ERROR("Could not get buffer size for the MSWASAPI audio input interface [%x]", result);
	ms_message("MSWASAPI audio input interface buffer size: %i", mBufferFrameCount);
	result = mAudioClient->GetService(IID_IAudioCaptureClient, (void **)&mAudioCaptureClient);
	REPORT_ERROR("Could not get render service from the MSWASAPI audio input interface [%x]", result);
	result = mAudioClient->GetService(IID_ISimpleAudioVolume, (void **)&mVolumeControler);
	REPORT_ERROR("Could not get volume control service from the MSWASAPI audio input interface [%x]", result);
	mIsActivated = true;
	ms_message("MSWASAPI capture initialized for [%s] at %i Hz, %i channels, with buffer size %i (%i ms), %i-bit frames are on %i bits", mDeviceName.c_str(), (int)mRate, (int)mNChannels,
		(int)mBufferFrameCount, (int)1000*mBufferFrameCount/ mRate, (int)pUsedWfx->wBitsPerSample, mNBlockAlign*8);
	FREE_PTR(pSupportedWfx);
	return 0;

error:
	FREE_PTR(pSupportedWfx);
	if(mAudioClient) mAudioClient->Reset();
	return -1;
}

int MSWASAPIReader::deactivate() {
	RELEASE_CLIENT(mAudioCaptureClient);
	RELEASE_CLIENT(mVolumeControler);
	
	mIsActivated = false;
	return 0;
}

void MSWASAPIReader::start() {
	HRESULT result;

	if (!isStarted() && mIsActivated) {
		mIsStarted = true;
		result = mAudioClient->Start();
		if (result != S_OK) {
			ms_error("Could not start capture on the MSWASAPI audio input interface [%x]", result);
		}
	}
	ms_ticker_set_synchronizer(mFilter->ticker, mTickerSynchronizer);
}

void MSWASAPIReader::stop() {
	HRESULT result;

	if (isStarted() && mIsActivated) {
		mIsStarted = false;
		result = mAudioClient->Stop();
		if (result != S_OK) {
			ms_error("Could not stop capture on the MSWASAPI audio input interface [%x]", result);
		}
	}
	ms_ticker_set_synchronizer(mFilter->ticker, nullptr);
}

int MSWASAPIReader::feed(MSFilter *f) {
	HRESULT result;
	DWORD flags;
	BYTE *pData;


	UINT32 numFramesAvailable;
	UINT32 numFramesInNextPacket = 0;
	UINT64 devicePosition;
	mblk_t *m;
	int bytesPerFrame = mNBlockAlign;

	if (isStarted()) {
		result = mAudioCaptureClient->GetNextPacketSize(&numFramesInNextPacket);
		while (numFramesInNextPacket != 0) {
			REPORT_ERROR("Could not get next packet size for the MSWASAPI audio input interface [%x]", result);

			result = mAudioCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, &devicePosition, NULL);
			REPORT_ERROR("Could not get buffer from the MSWASAPI audio input interface [%x]", result);
			if (numFramesAvailable > 0) {
				m = allocb(numFramesAvailable * bytesPerFrame, 0);
				if (m == NULL) {
					ms_error("Could not allocate memory for the captured data from the MSWASAPI audio input interface");
					goto error;
				}
				
				if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
					memset(m->b_wptr, 0, numFramesAvailable * bytesPerFrame);
				} else {
					memcpy(m->b_wptr, pData, numFramesAvailable * bytesPerFrame);
				}
				result = mAudioCaptureClient->ReleaseBuffer(numFramesAvailable);
				REPORT_ERROR("Could not release buffer of the MSWASAPI audio input interface [%x]", result);

				m->b_wptr += numFramesAvailable * bytesPerFrame;
				ms_ticker_synchronizer_update(mTickerSynchronizer, devicePosition, (unsigned int)mRate);

				ms_queue_put(f->outputs[0], m);
				result = mAudioCaptureClient->GetNextPacketSize(&numFramesInNextPacket);
			}
			
		} 
	} else {
		silence(f);
	}
	return 0;

error:
	return -1;
}

float MSWASAPIReader::getVolumeLevel() {
	HRESULT result;
	float volume;

	if (!mIsActivated) {
		ms_error("MSWASAPIReader::getVolumeLevel(): the MSWASAPIReader instance is not started");
		goto error;
	}
	result = mVolumeControler->GetMasterVolume(&volume);
	REPORT_ERROR("MSWASAPIReader::getVolumeLevel(): could not get the master volume [%x]", result);
	return volume;

error:
	return -1.0f;
}

void MSWASAPIReader::setVolumeLevel(float volume) {
	HRESULT result;

	if (!mIsActivated) {
		ms_error("MSWASAPIReader::setVolumeLevel(): the MSWASAPIReader instance is not started");
		goto error;
	}
	result = mVolumeControler->SetMasterVolume(volume, NULL);
	REPORT_ERROR("MSWASAPIReader::setVolumeLevel(): could not set the master volume [%x]", result);

error:
	return;
}

void MSWASAPIReader::setNChannels(int channels) {
	if(mNChannels != channels) {
		ms_warning("MSWASAPIReader::setNChannels(): trying to change channel to %d but it is not implemented. Keep %d", channels, mNChannels);
	}
}

void MSWASAPIReader::silence(MSFilter *f)
{
	mblk_t *om;
	unsigned int bufsize;
	unsigned int nsamples;

	nsamples = (f->ticker->interval * mRate) / 1000;
	bufsize = nsamples * mNChannels * 2;
	om = allocb(bufsize, 0);
	memset(om->b_wptr, 0, bufsize);
	om->b_wptr += bufsize;
	ms_queue_put(f->outputs[0], om);
}

#ifndef MS2_WINDOWS_PHONE
// Called from main mswasapi.
MSWASAPIReaderPtr MSWASAPIReaderNew(MSFilter *f) {
	MSWASAPIReaderPtr w = new MSWASAPIReaderWrapper();
#ifdef MS2_WINDOWS_UNIVERSAL
	w->reader = Make<MSWASAPIReader>(f);
#endif
	w->reader = new MSWASAPIReader(f);
	w->reader->AddRef();
	return w;
}
void MSWASAPIReaderDelete(MSWASAPIReaderPtr ptr) {
#ifdef MS2_WINDOWS_UNIVERSAL
	ptr->reader->setInstantiated(false);
#endif
	ptr->reader->Release();
	ptr->reader = nullptr;
	delete ptr;
}
#else
MSWASAPIReaderPtr MSWASAPIReaderNew(MSFilter *f) {
	return (MSWASAPIReaderPtr) new MSWASAPIReader(f);
}

void MSWASAPIReaderDelete(MSWASAPIReaderPtr ptr) {
	delete ptr;
}
#endif
