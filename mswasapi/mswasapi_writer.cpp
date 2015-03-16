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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "mediastreamer2/mscommon.h"
#include "mswasapi_writer.h"


#define REFTIME_250MS 2500000

#define REPORT_ERROR(msg, result) \
	if (result != S_OK) { \
		ms_error(msg, result); \
		goto error; \
	}
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


bool MSWASAPIWriter::smInstantiated = false;


MSWASAPIWriter::MSWASAPIWriter()
	: mAudioClient(NULL), mAudioRenderClient(NULL), mBufferFrameCount(0), mIsInitialized(false), mIsActivated(false), mIsStarted(false)
{
}

MSWASAPIWriter::~MSWASAPIWriter()
{
	RELEASE_CLIENT(mAudioClient);
#if BUILD_FOR_WINDOWS_PHONE
	FREE_PTR(mRenderId);
#endif
	smInstantiated = false;
}


void MSWASAPIWriter::init(LPCWSTR id) {
	HRESULT result;
	WAVEFORMATEX *pWfx = NULL;

#if BUILD_FOR_WINDOWS_PHONE
	AudioClientProperties properties;
	mRenderId = GetDefaultAudioRenderId(Communications);
	if (mRenderId == NULL) {
		ms_error("Could not get the RenderId of the MSWASAPI audio output interface");
		goto error;
	}

	if (smInstantiated) {
		ms_error("An MSWASAPIWriter is already instantiated. A second one can not be created.");
		goto error;
	}

	result = ActivateAudioInterface(mRenderId, IID_IAudioClient2, (void **)&mAudioClient);
	REPORT_ERROR("Could not activate the MSWASAPI audio output interface [%i]", result);
	properties.cbSize = sizeof AudioClientProperties;
	properties.bIsOffload = false;
	properties.eCategory = AudioCategory_Communications;
	result = mAudioClient->SetClientProperties(&properties);
	REPORT_ERROR("Could not set properties of the MSWASAPI audio output interface [%x]", result);
#else
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDevice *pDevice = NULL;
	CoInitialize(NULL);
	result = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
	REPORT_ERROR("mswasapi: Could not create an instance of the device enumerator", result);
	mRenderId = id;
	result = pEnumerator->GetDevice(mRenderId, &pDevice);
	SAFE_RELEASE(pEnumerator);
	REPORT_ERROR("mswasapi: Could not get the rendering device", result);
	result = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&mAudioClient);
	SAFE_RELEASE(pDevice);
	REPORT_ERROR("mswasapi: Could not activate the rendering device", result);
#endif
	result = mAudioClient->GetMixFormat(&pWfx);
	REPORT_ERROR("Could not get the mix format of the MSWASAPI audio output interface [%x]", result);
	mRate = pWfx->nSamplesPerSec;
	mNChannels = pWfx->nChannels;
	FREE_PTR(pWfx);
	mIsInitialized = true;
	smInstantiated = true;
	return;

error:
	// Initialize the frame rate and the number of channels to prevent configure a resampler with crappy parameters.
	mRate = 8000;
	mNChannels = 1;
	return;
}

int MSWASAPIWriter::activate()
{
	HRESULT result;
	REFERENCE_TIME requestedDuration = REFTIME_250MS;
	WAVEFORMATPCMEX proposedWfx;
	WAVEFORMATEX *pUsedWfx = NULL;
	WAVEFORMATEX *pSupportedWfx = NULL;

	if (!mIsInitialized) goto error;

	proposedWfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	proposedWfx.Format.nChannels = (WORD)mNChannels;
	proposedWfx.Format.nSamplesPerSec = mRate;
	proposedWfx.Format.wBitsPerSample = 16;
	proposedWfx.Format.nAvgBytesPerSec = mRate * mNChannels * proposedWfx.Format.wBitsPerSample / 8;
	proposedWfx.Format.nBlockAlign = (WORD)(proposedWfx.Format.wBitsPerSample * mNChannels / 8);
	proposedWfx.Format.cbSize = 22;
	proposedWfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
	proposedWfx.Samples.wValidBitsPerSample = proposedWfx.Format.wBitsPerSample;
	if (mNChannels == 1) {
		proposedWfx.dwChannelMask = SPEAKER_FRONT_CENTER;
	} else {
		proposedWfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	}
	result = mAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX *)&proposedWfx, &pSupportedWfx);
	if (result == S_OK) {
		pUsedWfx = (WAVEFORMATEX *)&proposedWfx;
	} else if (result == S_FALSE) {
		pUsedWfx = pSupportedWfx;
	} else {
		REPORT_ERROR("Audio format not supported by the MSWASAPI audio output interface [%x]", result);
	}
	result = mAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, requestedDuration, 0, pUsedWfx, NULL);
	REPORT_ERROR("Could not initialize the MSWASAPI audio output interface [%x]", result);
	result = mAudioClient->GetBufferSize(&mBufferFrameCount);
	REPORT_ERROR("Could not get buffer size for the MSWASAPI audio output interface [%x]", result);
	ms_message("MSWASAPI audio output interface buffer size: %i", mBufferFrameCount);
	result = mAudioClient->GetService(IID_IAudioRenderClient, (void **)&mAudioRenderClient);
	REPORT_ERROR("Could not get render service from the MSWASAPI audio output interface [%x]", result);
	mIsActivated = true;
	return 0;

error:
	FREE_PTR(pSupportedWfx);
	return -1;
}

int MSWASAPIWriter::deactivate()
{
	RELEASE_CLIENT(mAudioRenderClient);
	mIsActivated = false;
	return 0;
}

void MSWASAPIWriter::start()
{
	HRESULT result;

	if (!isStarted() && mIsActivated) {
		mIsStarted = true;
		result = mAudioClient->Start();
		if (result != S_OK) {
			ms_error("Could not start playback on the MSWASAPI audio output interface [%x]", result);
		}
	}
}

void MSWASAPIWriter::stop()
{
	HRESULT result;

	if (isStarted() && mIsActivated) {
		mIsStarted = false;
		result = mAudioClient->Stop();
		if (result != S_OK) {
			ms_error("Could not stop playback on the MSWASAPI audio output interface [%x]", result);
		}
	}
}

int MSWASAPIWriter::feed(MSFilter *f)
{
	HRESULT result;
	BYTE *buffer;
	UINT32 numFramesPadding;
	UINT32 numFramesAvailable;
	UINT32 numFramesFed;
	mblk_t *im;
	int msBufferSizeAvailable;
	int msNumFramesAvailable;
	int bytesPerFrame = (16 * mNChannels / 8);

	if (isStarted()) {
		while ((im = ms_queue_get(f->inputs[0])) != NULL) {
			msBufferSizeAvailable = msgdsize(im);
			if (msBufferSizeAvailable < 0) msBufferSizeAvailable = 0;
			msNumFramesAvailable = msBufferSizeAvailable / bytesPerFrame;
			if (msNumFramesAvailable > 0) {
				// Calculate the number of frames to pass to the Audio Render Client
				result = mAudioClient->GetCurrentPadding(&numFramesPadding);
				REPORT_ERROR("Could not get current buffer padding for the MSWASAPI audio output interface [%x]", result);
				numFramesAvailable = mBufferFrameCount - numFramesPadding;
				if ((UINT32)msNumFramesAvailable > numFramesAvailable) {
					// The bufferizer is filled more than the space available in the Audio Render Client.
					// Some frames will be dropped.
					numFramesFed = numFramesAvailable;
				} else {
					numFramesFed = msNumFramesAvailable;
				}

				// Feed the Audio Render Client
				if (numFramesFed > 0) {
					result = mAudioRenderClient->GetBuffer(numFramesFed, &buffer);
					REPORT_ERROR("Could not get buffer from the MSWASAPI audio output interface [%x]", result);
					memcpy(buffer, im->b_rptr, numFramesFed * bytesPerFrame);
					result = mAudioRenderClient->ReleaseBuffer(numFramesFed, 0);
					REPORT_ERROR("Could not release buffer of the MSWASAPI audio output interface [%x]", result);
				}
			}
			freemsg(im);
		}
	} else {
		drop(f);
	}
	return 0;

error:
	return -1;
}

void MSWASAPIWriter::drop(MSFilter *f) {
	mblk_t *im;

	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		freemsg(im);
	}
}
