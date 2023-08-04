/*
mswasapi_writer.cpp

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
#include "mediastreamer2/flowcontrol.h"
#include "mswasapi_writer.h"


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


static const int flowControlInterval = 5000; // ms
static const int flowControlThreshold = 40; // ms
static const int minBufferDurationMs = 200; // ms

#ifdef MS2_WINDOWS_UNIVERSAL
bool MSWASAPIWriter::smInstantiated = false;
#endif

MSWASAPIWriter::MSWASAPIWriter()
	:  MSWasapi("output"), mAudioRenderClient(NULL), mVolumeControler(NULL), mBufferFrameCount(0), mIsActivated(false), mIsStarted(false) {
#ifndef MS2_WINDOWS_PHONE
	mActivationEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	if (!mActivationEvent) {
		ms_error("MSWASAPIWriter: Could not create activation event of the MSWASAPI audio output interface [%i]", GetLastError());
		return;
	}
#endif
}

MSWASAPIWriter::~MSWASAPIWriter() {
	RELEASE_CLIENT(mAudioClient);
#ifdef MS2_WINDOWS_PHONE
	FREE_PTR(mRenderId);
#else
	if (mActivationEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(mActivationEvent);
		mActivationEvent = INVALID_HANDLE_VALUE;
	}
#ifdef MS2_WINDOWS_UNIVERSAL
	smInstantiated = false;
#endif
#endif
}

int MSWASAPIWriter::activate() {
	HRESULT result;
	WAVEFORMATPCMEX proposedWfx;
	WAVEFORMATEX *pUsedWfx = NULL;
	WAVEFORMATEX *pSupportedWfx = NULL;
	REFERENCE_TIME devicePeriod = 0;
	REFERENCE_TIME requestedBufferDuration = 0;
	int devicePeriodMs;
	
	if (!mIsInitialized) goto error;
	
	result = mAudioClient->GetDevicePeriod(&devicePeriod, NULL);
	if (result != S_OK) {
		ms_warning("MSWASAPIWriter: GetDevicePeriod() failed.");
	}
	devicePeriodMs = (int)(devicePeriod / 10000);
	
	/*Compute our requested buffer duration.*/
	requestedBufferDuration = minBufferDurationMs * 10000LL; // requestedBufferDuration is expressed in 100-nanoseconds units
	
	proposedWfx = buildFormat();
	
	result = mAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX *)&proposedWfx, &pSupportedWfx);
	if (result == S_OK) {
		pUsedWfx = (WAVEFORMATEX *)&proposedWfx;
	} else if (result == S_FALSE) {
		pUsedWfx = pSupportedWfx;
	} else {
		REPORT_ERROR("Audio format not supported by the MSWASAPI audio output interface [%x]", result);
	}
	// Use best quality intended to be heard by humans.
	result = mAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |  AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY , requestedBufferDuration, 0, pUsedWfx, NULL);
	if ((result != S_OK) && (result != AUDCLNT_E_ALREADY_INITIALIZED)) {
		REPORT_ERROR("Could not initialize the MSWASAPI audio output interface [%x]", result);
	}
	mNBlockAlign = pUsedWfx->nBlockAlign;
	mRate = pUsedWfx->nSamplesPerSec;
	mNChannels = pUsedWfx->nChannels;
	result = mAudioClient->GetBufferSize(&mBufferFrameCount);
	REPORT_ERROR("Could not get buffer size for the MSWASAPI audio output interface [%x]", result);
	result = mAudioClient->GetService(IID_IAudioRenderClient, (void **)&mAudioRenderClient);
	REPORT_ERROR("Could not get render service from the MSWASAPI audio output interface [%x]", result);
	result = mAudioClient->GetService(IID_ISimpleAudioVolume, (void **)&mVolumeControler);
	REPORT_ERROR("Could not get volume control service from the MSWASAPI audio output interface [%x]", result);
	mIsActivated = true;
	
	ms_message("MSWASAPI playback output initialized for [%s] at %i Hz, %i channels, with buffer size %i (%i ms), device period is %i, %i-bit frames are on %i bits", mDeviceName.c_str(), (int)mRate, (int)mNChannels,
			   (int)mBufferFrameCount, (int)1000*mBufferFrameCount/ mRate, devicePeriodMs, (int)pUsedWfx->wBitsPerSample, mNBlockAlign*8);
	FREE_PTR(pSupportedWfx);
	return 0;
	
error:
	FREE_PTR(pSupportedWfx);
	if(mAudioClient) mAudioClient->Reset();
	return -1;
}

int MSWASAPIWriter::deactivate() {
	RELEASE_CLIENT(mAudioRenderClient);
	mIsActivated = false;
	return 0;
}

void MSWASAPIWriter::start() {
	HRESULT result;
	
	if (!isStarted() && mIsActivated) {
		result = mAudioClient->Start();
		if (result != S_OK) {
			ms_error("Could not start playback on the MSWASAPI audio output interface [%x]", result);
			return;
		}
		mIsStarted = true;
	}
}

void MSWASAPIWriter::stop() {
	HRESULT result;
	
	if (isStarted() && mIsActivated) {
		mIsStarted = false;
		result = mAudioClient->Stop();
		if (result != S_OK) {
			ms_error("Could not stop playback on the MSWASAPI audio output interface [%x]", result);
		}
	}
}

/* We go here everytick, as the filter is declared with the IS_PUMP flag.*/
int MSWASAPIWriter::feed(MSFilter *f) {
	HRESULT result;
	UINT32 numFramesPadding = 0;
	UINT32 numFramesWritable;        
	mblk_t *im;
	
	if (!isStarted()) goto error;
	
	result = mAudioClient->GetCurrentPadding(&numFramesPadding);
	REPORT_ERROR("Could not get current buffer padding for the MSWASAPI audio output interface [%x]", result);
	numFramesWritable = mBufferFrameCount - numFramesPadding;
	REPORT_ERROR("Could not get the frame size for the MSWASAPI audio output interface [%x]", result);
	
	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		int inputFrames = (int)(msgdsize(im) / mNBlockAlign);
		int writtenFrames = inputFrames;
		if( msgdsize(im)%8 != 0 )// Get one more space to put unexpected data. This is a workaround to a bug from special audio driver that could write/use outside the requested buffer and where it is not on fully 8 octets
			++inputFrames;
		msgpullup(im, -1);
		if (inputFrames > (int)numFramesWritable) {
			/*This case should not happen because of upstream flow control, except in rare disaster cases.*/
			if(mMinFrameCount != -1)
				ms_error("MSWASAPIWriter: cannot write output buffer of %i samples, not enough space [%i=%i-%i].", inputFrames, numFramesWritable,mBufferFrameCount,numFramesPadding);
		}else {
			BYTE *buffer;
			result = mAudioRenderClient->GetBuffer(inputFrames, &buffer);
			if (result == S_OK) {
				memcpy(buffer, im->b_rptr, im->b_wptr - im->b_rptr);
				result = mAudioRenderClient->ReleaseBuffer(writtenFrames, 0);	//Use only the needed frame
			}else {
				ms_error("Could not get buffer from the MSWASAPI audio output interface [%x]", result);
			}
		}
		freemsg(im);
	}
	/* Compute the minimum number of queued samples during a 5 seconds period.*/
	if (mMinFrameCount == -1) mMinFrameCount = numFramesPadding;
	if ((int)numFramesPadding < mMinFrameCount) mMinFrameCount = (int)numFramesPadding;
	if (f->ticker->time % flowControlInterval == 0) {
		int minExcessMs = (mMinFrameCount * 1000) / mRate;
		if (minExcessMs >= flowControlThreshold) {
			/* Send a notification to request the flow controller to drop samples in excess.*/
			MSAudioFlowControlDropEvent ev;
			ev.flow_control_interval_ms = flowControlInterval;
			ev.drop_ms = minExcessMs / 2;
			if (ev.drop_ms > 0) {
				ms_warning("MSWASAPIWrite: output buffer was filled with at least %i ms in the last %i ms, asking to drop.", (int)ev.drop_ms, flowControlInterval);
				ms_filter_notify(f, MS_AUDIO_FLOW_CONTROL_DROP_EVENT, &ev);
			}
		}else {
			ms_debug("MSWASAPIWrite: buffer was filled with a minmum of %i ms in the last %i ms, all is good.", (int)((mMinFrameCount * 1000) / mRate), flowControlInterval);
		}
		mMinFrameCount = -1;
	}
	
error:
	ms_queue_flush(f->inputs[0]);
	return 0;
}

float MSWASAPIWriter::getVolumeLevel() {
	HRESULT result;
	float volume;
	
	if (!mIsActivated) {
		ms_error("MSWASAPIWriter::getVolumeLevel(): the MSWASAPIWriter instance is not started");
		goto error;
	}
	result = mVolumeControler->GetMasterVolume(&volume);
	REPORT_ERROR("MSWASAPIWriter::getVolumeLevel(): could not get the master volume [%x]", result);
	return volume;
	
error:
	return -1.0f;
}

void MSWASAPIWriter::setVolumeLevel(float volume) {
	HRESULT result;
	
	if (!mIsActivated) {
		ms_error("MSWASAPIWriter::setVolumeLevel(): the MSWASAPIWriter instance is not started");
		goto error;
	}
	result = mVolumeControler->SetMasterVolume(volume, NULL);
	REPORT_ERROR("MSWASAPIWriter::setVolumeLevel(): could not set the master volume [%x]", result);
	
error:
	return;
}

void MSWASAPIWriter::setNChannels(int channels) {
	if(mNChannels != channels) {
		ms_warning("MSWASAPIWriter::setNChannels(): trying to change channel to %d but it is not implemented. Keep %d", channels, mNChannels);
	}
}

#ifndef MS2_WINDOWS_PHONE
// Called from main mswasapi.
MSWASAPIWriterPtr MSWASAPIWriterNew() {
	MSWASAPIWriterPtr w = new MSWASAPIWriterWrapper();
#ifdef MS2_WINDOWS_UNIVERSAL
	w->writer = Make<MSWASAPIWriter>();
#endif
	w->writer = new MSWASAPIWriter();
	w->writer->AddRef();
	return w;
}
void MSWASAPIWriterDelete(MSWASAPIWriterPtr ptr) {
#ifdef MS2_WINDOWS_UNIVERSAL
	ptr->writer->setInstantiated(false);
#endif
	ptr->writer->Release();
	ptr->writer = nullptr;
	delete ptr;
}
#else
MSWASAPIWriterPtr MSWASAPIWriterNew() {
	return (MSWASAPIWriterPtr) new MSWASAPIWriter();
}

void MSWASAPIWriterDelete(MSWASAPIWriterPtr ptr) {
	delete ptr;
}
#endif
