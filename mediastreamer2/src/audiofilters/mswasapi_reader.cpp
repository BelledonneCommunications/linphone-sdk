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


bool MSWASAPIReader::smInstantiated = false;


MSWASAPIReader::MSWASAPIReader(MSFilter *filter)
	: mAudioClient(NULL), mAudioCaptureClient(NULL), mVolumeControler(NULL), mBufferFrameCount(0), mIsInitialized(false), mIsActivated(false), mIsStarted(false), mFilter(filter)
{
	mTickerSynchronizer = ms_ticker_synchronizer_new();
#ifdef MS2_WINDOWS_UNIVERSAL
	mActivationEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	if (!mActivationEvent) {
		ms_error("Could not create activation event of the MSWASAPI audio input interface [%i]", GetLastError());
		return;
	}
#endif
}

MSWASAPIReader::~MSWASAPIReader()
{
	RELEASE_CLIENT(mAudioClient);
#ifdef MS2_WINDOWS_PHONE
	FREE_PTR(mCaptureId);
#endif
#ifdef MS2_WINDOWS_UNIVERSAL
	if (mActivationEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(mActivationEvent);
		mActivationEvent = INVALID_HANDLE_VALUE;
	}
#endif
	ms_ticker_synchronizer_destroy(mTickerSynchronizer);
	smInstantiated = false;
}


void MSWASAPIReader::init(LPCWSTR id)
{
	HRESULT result;
	WAVEFORMATEX *pWfx = NULL;
#if defined(MS2_WINDOWS_PHONE) || defined(MS2_WINDOWS_UNIVERSAL)
	AudioClientProperties properties = { 0 };
#endif

#if defined(MS2_WINDOWS_UNIVERSAL)
	ComPtr<IActivateAudioInterfaceAsyncOperation> asyncOp;
	mCaptureId = ref new Platform::String(id);
	if (mCaptureId == nullptr) {
		ms_error("Could not get the CaptureID of the MSWASAPI audio input interface");
		goto error;
	}
	if (smInstantiated) {
		ms_error("An MSWASAPIReader is already instantiated. A second one can not be created.");
		goto error;
	}
	result = ActivateAudioInterfaceAsync(mCaptureId->Data(), IID_IAudioClient2, NULL, this, &asyncOp);
	REPORT_ERROR("Could not activate the MSWASAPI audio input interface [%i]", result);
	WaitForSingleObjectEx(mActivationEvent, INFINITE, FALSE);
	if (mAudioClient == NULL) {
		ms_error("Could not create the MSWASAPI audio input interface client");
		goto error;
	}
#elif defined(MS2_WINDOWS_PHONE)
	mCaptureId = GetDefaultAudioCaptureId(Communications);
	if (mCaptureId == NULL) {
		ms_error("Could not get the CaptureId of the MSWASAPI audio input interface");
		goto error;
	}

	if (smInstantiated) {
		ms_error("An MSWASAPIReader is already instantiated. A second one can not be created.");
		goto error;
	}

	result = ActivateAudioInterface(mCaptureId, IID_IAudioClient2, (void **)&mAudioClient);
	REPORT_ERROR("Could not activate the MSWASAPI audio input interface [%x]", result);
#else
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDevice *pDevice = NULL;
	CoInitialize(NULL);
	result = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
	REPORT_ERROR("mswasapi: Could not create an instance of the device enumerator", result);
	mCaptureId = id;
	result = pEnumerator->GetDevice(mCaptureId, &pDevice);
	SAFE_RELEASE(pEnumerator);
	REPORT_ERROR("mswasapi: Could not get the capture device", result);
	result = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&mAudioClient);
	SAFE_RELEASE(pDevice);
	REPORT_ERROR("mswasapi: Could not activate the capture device", result);
#endif
#if defined(MS2_WINDOWS_PHONE) || defined(MS2_WINDOWS_UNIVERSAL)
	properties.cbSize = sizeof(AudioClientProperties);
	properties.bIsOffload = false;
	properties.eCategory = AudioCategory_Communications;
	result = mAudioClient->SetClientProperties(&properties);
	REPORT_ERROR("Could not set properties of the MSWASAPI audio input interface [%x]", result);
#endif
	result = mAudioClient->GetMixFormat(&pWfx);
	REPORT_ERROR("Could not get the mix format of the MSWASAPI audio input interface [%x]", result);
	mRate = pWfx->nSamplesPerSec;
	mNChannels = pWfx->nChannels;
	FREE_PTR(pWfx);
	mIsInitialized = true;
	smInstantiated = true;
	return;

error:
	// Initialize the frame rate and the number of channels to be able to generate silence.
	mRate = 8000;
	mNChannels = 1;
	return;
}

int MSWASAPIReader::activate()
{
	HRESULT result;
	REFERENCE_TIME requestedDuration = REFTIME_250MS;
	WAVEFORMATPCMEX proposedWfx;
	WAVEFORMATEX *pUsedWfx = NULL;
	WAVEFORMATEX *pSupportedWfx = NULL;
	DWORD flags = 0;

	if (!mIsInitialized) goto error;

	ms_ticker_set_synchronizer(mFilter->ticker, mTickerSynchronizer);

#ifdef MS2_WINDOWS_PHONE
	flags = AUDCLNT_SESSIONFLAGS_EXPIREWHENUNOWNED | AUDCLNT_SESSIONFLAGS_DISPLAY_HIDE | AUDCLNT_SESSIONFLAGS_DISPLAY_HIDEWHENEXPIRED;
#endif

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
		REPORT_ERROR("Audio format not supported by the MSWASAPI audio input interface [%x]", result);
	}
	result = mAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, requestedDuration, 0, pUsedWfx, NULL);
	if ((result != S_OK) && (result != AUDCLNT_E_ALREADY_INITIALIZED)) {
		REPORT_ERROR("Could not initialize the MSWASAPI audio input interface [%x]", result);
	}
	result = mAudioClient->GetBufferSize(&mBufferFrameCount);
	REPORT_ERROR("Could not get buffer size for the MSWASAPI audio input interface [%x]", result);
	ms_message("MSWASAPI audio input interface buffer size: %i", mBufferFrameCount);
	result = mAudioClient->GetService(IID_IAudioCaptureClient, (void **)&mAudioCaptureClient);
	REPORT_ERROR("Could not get render service from the MSWASAPI audio input interface [%x]", result);
	result = mAudioClient->GetService(IID_ISimpleAudioVolume, (void **)&mVolumeControler);
	REPORT_ERROR("Could not get volume control service from the MSWASAPI audio input interface [%x]", result);
	mIsActivated = true;
	return 0;

error:
	FREE_PTR(pSupportedWfx);
	return -1;
}

int MSWASAPIReader::deactivate()
{
	RELEASE_CLIENT(mAudioCaptureClient);
	RELEASE_CLIENT(mVolumeControler);
	ms_ticker_set_synchronizer(mFilter->ticker, nullptr);
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
			ms_error("Could not start playback on the MSWASAPI audio input interface [%x]", result);
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
			ms_error("Could not stop playback on the MSWASAPI audio input interface [%x]", result);
		}
	}
}

int MSWASAPIReader::feed(MSFilter *f)
{
	HRESULT result;
	DWORD flags;
	BYTE *pData;


	UINT32 numFramesAvailable;
	UINT32 numFramesInNextPacket = 0;
	mblk_t *m;
	int bytesPerFrame = (16 * mNChannels / 8);

	if (isStarted()) {
		result = mAudioCaptureClient->GetNextPacketSize(&numFramesInNextPacket);
		while (numFramesInNextPacket != 0) {
			REPORT_ERROR("Could not get next packet size for the MSWASAPI audio input interface [%x]", result);

			result = mAudioCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
			REPORT_ERROR("Could not get buffer from the MSWASAPI audio input interface [%x]", result);
			if (numFramesAvailable > 0) {
				m = allocb(numFramesAvailable * bytesPerFrame, 0);
				if (m == NULL) {
					ms_error("Could not allocate memory for the captured data from the MSWASAPI audio input interface");
					goto error;
				}
				
				if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
					memset(m->b_wptr, 0, numFramesAvailable * bytesPerFrame);
				}
				else {
					memcpy(m->b_wptr, pData, numFramesAvailable * bytesPerFrame);
				}
				result = mAudioCaptureClient->ReleaseBuffer(numFramesAvailable);
				REPORT_ERROR("Could not release buffer of the MSWASAPI audio input interface [%x]", result);

				m->b_wptr += numFramesAvailable * bytesPerFrame;
				mReadFrames += numFramesAvailable;
				ms_ticker_synchronizer_update(mTickerSynchronizer, mReadFrames, (unsigned int)mRate);

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


#ifdef MS2_WINDOWS_UNIVERSAL
//
//  ActivateCompleted()
//
//  Callback implementation of ActivateAudioInterfaceAsync function.  This will be called on MTA thread
//  when results of the activation are available.
//
HRESULT MSWASAPIReader::ActivateCompleted(IActivateAudioInterfaceAsyncOperation *operation)
{
	HRESULT hr = S_OK;
	HRESULT hrActivateResult = S_OK;
	ComPtr<IUnknown> audioInterface;

	hr = operation->GetActivateResult(&hrActivateResult, &audioInterface);
	if (FAILED(hr))	goto exit;
	hr = hrActivateResult;
	if (FAILED(hr)) goto exit;

	audioInterface.CopyTo(&mAudioClient);
	if (mAudioClient == NULL) {
		hr = E_NOINTERFACE;
		goto exit;
	}

exit:
	if (FAILED(hr))	{
		SAFE_RELEASE(mAudioClient);
		SAFE_RELEASE(mAudioCaptureClient);
	}

	SetEvent(mActivationEvent);
	return S_OK;
}


MSWASAPIReaderPtr MSWASAPIReaderNew(MSFilter *f)
{
	MSWASAPIReaderPtr r = new MSWASAPIReaderWrapper(f);
	r->reader = Make<MSWASAPIReader>();
	return r;
}
void MSWASAPIReaderDelete(MSWASAPIReaderPtr ptr)
{
	ptr->reader->setAsNotInstantiated();
	ptr->reader = nullptr;
	delete ptr;
}
#else
MSWASAPIReaderPtr MSWASAPIReaderNew(MSFilter *f)
{
	return (MSWASAPIReaderPtr) new MSWASAPIReader(f);
}
void MSWASAPIReaderDelete(MSWASAPIReaderPtr ptr)
{
	delete ptr;
}
#endif
