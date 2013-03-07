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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "mediastreamer2/mscommon.h"
#include "mswasapi_reader.h"


#define REFTIMES_PER_SEC 10000000

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


MSWASAPIReader::MSWASAPIReader()
	: mAudioClient(NULL), mAudioCaptureClient(NULL), mBufferFrameCount(0), mIsInitialized(false), mIsActivated(false), mIsStarted(false)
{
	HRESULT result;
	WAVEFORMATEX *pWfx = NULL;

	mCaptureId = GetDefaultAudioCaptureId(Communications);
	if (mCaptureId == NULL) {
		ms_error("Could not get the CaptureId of the MSWASAPI audio input interface");
		goto error;
	}
	result = ActivateAudioInterface(mCaptureId, IID_IAudioClient, (void **)&mAudioClient);
	REPORT_ERROR("Could not activate the MSWASAPI audio input interface [%i]", result);
	result = mAudioClient->GetMixFormat(&pWfx);
	REPORT_ERROR("Could not get the mix format of the MSWASAPI audio input interface [%i]", result);
	mRate = pWfx->nSamplesPerSec;
	mNChannels = pWfx->nChannels;
	FREE_PTR(pWfx);
	mIsInitialized = true;

error:
	return;
}

MSWASAPIReader::~MSWASAPIReader()
{
	RELEASE_CLIENT(mAudioClient);
	FREE_PTR(mCaptureId);
}


int MSWASAPIReader::activate()
{
	HRESULT result;
	REFERENCE_TIME requestedDuration = REFTIMES_PER_SEC;
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
		REPORT_ERROR("Audio format not supported by the MSWASAPI audio input interface [%i]", result);
	}
	result = mAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, requestedDuration, 0, pUsedWfx, NULL);
	REPORT_ERROR("Could not initialize the MSWASAPI audio input interface [%i]", result);
	result = mAudioClient->GetBufferSize(&mBufferFrameCount);
	REPORT_ERROR("Could not get buffer size for the MSWASAPI audio input interface [%i]", result);
	ms_message("MSWASAPI audio input interface buffer size: %i", mBufferFrameCount);
	result = mAudioClient->GetService(IID_IAudioCaptureClient, (void **)&mAudioCaptureClient);
	REPORT_ERROR("Could not get render service from the MSWASAPI audio input interface [%i]", result);
	mIsActivated = true;
	return 0;

error:
	FREE_PTR(pSupportedWfx);
	return -1;
}

int MSWASAPIReader::deactivate()
{
	RELEASE_CLIENT(mAudioCaptureClient);
	mIsActivated = false;
	return 0;
}

void MSWASAPIReader::start()
{
	HRESULT result;

	if (!isStarted() && mIsActivated) {
		mIsStarted = true;
		result = mAudioClient->Start();
		if (result != S_OK) {
			ms_error("Could not start playback on the MSWASAPI audio input interface [%i]", result);
		}
	}
}

void MSWASAPIReader::stop()
{
	HRESULT result;

	if (isStarted() && mIsActivated) {
		mIsStarted = false;
		result = mAudioClient->Stop();
		if (result != S_OK) {
			ms_error("Could not stop playback on the MSWASAPI audio input interface [%i]", result);
		}
	}
}

int MSWASAPIReader::feed(MSQueue *output)
{
	HRESULT result;
	DWORD flags;
	BYTE *pData;
	UINT32 numFramesInNextPacket;
	mblk_t *m;
	int bytesPerFrame = (16 * mNChannels / 8);

	result = mAudioCaptureClient->GetNextPacketSize(&numFramesInNextPacket);
	REPORT_ERROR("Could not get next packet size for the MSWASAPI audio input interface [%i]", result);
	if (numFramesInNextPacket > 0) {
		m = allocb(numFramesInNextPacket * bytesPerFrame, 0);
		result = mAudioCaptureClient->GetBuffer(&pData, &numFramesInNextPacket, &flags, NULL, NULL);
		REPORT_ERROR("Could not get buffer from the MSWASAPI audio input interface [%i]", result);
		if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
			memset(m->b_wptr, 0, numFramesInNextPacket * bytesPerFrame);
		} else {
			memcpy(m->b_wptr, pData, numFramesInNextPacket * bytesPerFrame);
		}
		m->b_wptr += numFramesInNextPacket * bytesPerFrame;
		result = mAudioCaptureClient->ReleaseBuffer(numFramesInNextPacket);
		REPORT_ERROR("Could not release buffer of the MSWASAPI audio input interface [%i]", result);
		ms_queue_put(output, m);
	}
	return 0;

error:
	return -1;
}
