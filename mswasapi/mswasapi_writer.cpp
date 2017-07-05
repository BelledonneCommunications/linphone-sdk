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
#include "mswasapi_writer.h"


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


static const int flowControlInterval = 5000; // ms
static const int flowControlThreshold = 40; // ms

bool MSWASAPIWriter::smInstantiated = false;


MSWASAPIWriter::MSWASAPIWriter()
	: mThread(0), mAudioClient(NULL), mAudioRenderClient(NULL), mVolumeControler(NULL), mBufferFrameCount(0), mIsInitialized(false), mIsActivated(false), mIsStarted(false), mIsReadyToWrite(false), mBufferizer(NULL)
{
	mSamplesRequestedEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	if (!mSamplesRequestedEvent) {
		ms_error("Could not create samples requested event of the MSWASAPI audio output interface [%i]", GetLastError());
		return;
	}
	ms_mutex_init(&mThreadMutex, NULL);
#ifdef MS2_WINDOWS_UNIVERSAL
	mActivationEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	if (!mActivationEvent) {
		ms_error("Could not create activation event of the MSWASAPI audio output interface [%i]", GetLastError());
		return;
	}
#endif
}

MSWASAPIWriter::~MSWASAPIWriter()
{
	RELEASE_CLIENT(mAudioClient);
#ifdef MS2_WINDOWS_PHONE
	FREE_PTR(mRenderId);
#endif
#ifdef MS2_WINDOWS_UNIVERSAL
	if (mActivationEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(mActivationEvent);
		mActivationEvent = INVALID_HANDLE_VALUE;
	}
#endif
	ms_mutex_destroy(&mThreadMutex);
	if (mSamplesRequestedEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(mSamplesRequestedEvent);
		mSamplesRequestedEvent = INVALID_HANDLE_VALUE;
	}
	if (mBufferizer) ms_flow_controlled_bufferizer_destroy(mBufferizer);
	smInstantiated = false;
}


void MSWASAPIWriter::init(LPCWSTR id, MSFilter *f) {
	HRESULT result;
	WAVEFORMATEX *pWfx = NULL;
#if defined(MS2_WINDOWS_PHONE) || defined(MS2_WINDOWS_UNIVERSAL)
	AudioClientProperties properties = { 0 };
#endif

#if defined(MS2_WINDOWS_UNIVERSAL)
	IActivateAudioInterfaceAsyncOperation *asyncOp;
	mRenderId = ref new Platform::String(id);
	if (mRenderId == nullptr) {
		ms_error("Could not get the RenderID of the MSWASAPI audio output interface");
		goto error;
	}
	if (smInstantiated) {
		ms_error("An MSWASAPIWriter is already instantiated. A second one can not be created.");
		goto error;
	}
	result = ActivateAudioInterfaceAsync(mRenderId->Data(), IID_IAudioClient2, NULL, this, &asyncOp);
	REPORT_ERROR("Could not activate the MSWASAPI audio output interface [%i]", result);
	WaitForSingleObjectEx(mActivationEvent, INFINITE, FALSE);
	if (mAudioClient == NULL) {
		ms_error("Could not create the MSWASAPI audio output interface client");
		goto error;
	}
#elif defined(MS2_WINDOWS_PHONE)
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
#if defined(MS2_WINDOWS_PHONE) || defined(MS2_WINDOWS_UNIVERSAL)
	properties.cbSize = sizeof(AudioClientProperties);
	properties.bIsOffload = false;
	properties.eCategory = AudioCategory_Communications;
	result = mAudioClient->SetClientProperties(&properties);
	REPORT_ERROR("Could not set properties of the MSWASAPI audio output interface [%x]", result);
#endif
	result = mAudioClient->GetMixFormat(&pWfx);
	REPORT_ERROR("Could not get the mix format of the MSWASAPI audio output interface [%x]", result);
	mRate = pWfx->nSamplesPerSec;
	mNChannels = pWfx->nChannels;
	FREE_PTR(pWfx);
	mIsInitialized = true;
	smInstantiated = true;
	createBufferizer(f);
	return;

error:
	// Initialize the frame rate and the number of channels to prevent configure a resampler with crappy parameters.
	mRate = 8000;
	mNChannels = 1;
	createBufferizer(f);
	return;
}

int MSWASAPIWriter::activate()
{
	HRESULT result;
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
	result = mAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST, 0, 0, pUsedWfx, NULL);
	if ((result != S_OK) && (result != AUDCLNT_E_ALREADY_INITIALIZED)) {
		REPORT_ERROR("Could not initialize the MSWASAPI audio output interface [%x]", result);
	}
	result = mAudioClient->SetEventHandle(mSamplesRequestedEvent);
	REPORT_ERROR("SetEventHandle for MSWASAPI audio output interface failed [%x]", result)
	result = mAudioClient->GetBufferSize(&mBufferFrameCount);
	REPORT_ERROR("Could not get buffer size for the MSWASAPI audio output interface [%x]", result);
	ms_message("MSWASAPI audio output interface buffer size: %i", mBufferFrameCount);
	result = mAudioClient->GetService(IID_IAudioRenderClient, (void **)&mAudioRenderClient);
	REPORT_ERROR("Could not get render service from the MSWASAPI audio output interface [%x]", result);
	result = mAudioClient->GetService(IID_ISimpleAudioVolume, (void **)&mVolumeControler);
	REPORT_ERROR("Could not get volume control service from the MSWASAPI audio output interface [%x]", result);
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
		ms_thread_create(&mThread, NULL, MSWASAPIWriter::feedThread, this);
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
		if (mThread != 0) {
			ms_thread_join(mThread, NULL);
			mThread = 0;
		}
	}
}

void * MSWASAPIWriter::feedThread(void *p)
{
	MSWASAPIWriter *writer = (MSWASAPIWriter *)p;
	return writer->feedThread();
}

void * MSWASAPIWriter::feedThread()
{
	HRESULT result;
	BYTE *buffer;
	UINT32 numFramesPadding;
	UINT32 numFramesAvailable;
	UINT32 numFramesFed;
	size_t msBufferSizeAvailable;
	int msNumFramesAvailable;
	int bytesPerFrame;

	while (mIsStarted) {
		WaitForSingleObjectEx(mSamplesRequestedEvent, INFINITE, false);

		mIsReadyToWrite = true;
		bytesPerFrame = (16 * mNChannels / 8);
		numFramesFed = 0;
		ms_mutex_lock(&mThreadMutex);
		msBufferSizeAvailable = ms_flow_controlled_bufferizer_get_avail(mBufferizer);
		if (msBufferSizeAvailable > 0) {
			msNumFramesAvailable = (int)(msBufferSizeAvailable / (size_t)bytesPerFrame);
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
					ms_flow_controlled_bufferizer_read(mBufferizer, buffer, numFramesFed * bytesPerFrame);
					result = mAudioRenderClient->ReleaseBuffer(numFramesFed, 0);
					REPORT_ERROR("Could not release buffer of the MSWASAPI audio output interface [%x]", result);
				}
			}
		} else {
			result = mAudioClient->GetCurrentPadding(&numFramesPadding);
			REPORT_ERROR("Could not get current buffer padding for the MSWASAPI audio output interface [%x]", result);
			numFramesAvailable = mBufferFrameCount - numFramesPadding;
			result = mAudioRenderClient->GetBuffer(numFramesAvailable, &buffer);
			REPORT_ERROR("Could not get buffer from the MSWASAPI audio output interface [%x]", result);
			memset(buffer, 0, numFramesAvailable * bytesPerFrame);
			result = mAudioRenderClient->ReleaseBuffer(numFramesAvailable, 0);
			REPORT_ERROR("Could not release buffer of the MSWASAPI audio output interface [%x]", result);
		}
		ms_mutex_unlock(&mThreadMutex);
	};

	return nullptr;

error:
	ms_mutex_unlock(&mThreadMutex);
	return nullptr;
}

int MSWASAPIWriter::feed(MSFilter *f)
{
	if (isStarted() && mIsReadyToWrite) {
		ms_mutex_lock(&mThreadMutex);
		ms_flow_controlled_bufferizer_put_from_queue(mBufferizer, f->inputs[0]);
		ms_mutex_unlock(&mThreadMutex);
	} else {
		drop(f);
	}
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

void MSWASAPIWriter::createBufferizer(MSFilter *f) {
	mBufferizer = ms_flow_controlled_bufferizer_new(f, mRate, mNChannels);
	ms_flow_controlled_bufferizer_set_max_size_ms(mBufferizer, flowControlThreshold);
	ms_flow_controlled_bufferizer_set_flow_control_interval_ms(mBufferizer, flowControlInterval);
}

void MSWASAPIWriter::drop(MSFilter *f) {
	mblk_t *im;

	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		freemsg(im);
	}
}


#ifdef MS2_WINDOWS_UNIVERSAL
//
//  ActivateCompleted()
//
//  Callback implementation of ActivateAudioInterfaceAsync function.  This will be called on MTA thread
//  when results of the activation are available.
//
HRESULT MSWASAPIWriter::ActivateCompleted(IActivateAudioInterfaceAsyncOperation *operation)
{
	HRESULT hr = S_OK;
	HRESULT hrActivateResult = S_OK;
	IUnknown *audioInterface = NULL;

	if (mIsInitialized) {
		hr = E_NOT_VALID_STATE;
		goto exit;
	}

	hr = operation->GetActivateResult(&hrActivateResult, &audioInterface);
	if (SUCCEEDED(hr) && SUCCEEDED(hrActivateResult))
	{
		audioInterface->QueryInterface(IID_PPV_ARGS(&mAudioClient));
		if (mAudioClient == NULL) {
			hr = E_FAIL;
			goto exit;
		}
	}

exit:
	SAFE_RELEASE(audioInterface);

	if (FAILED(hr))
	{
		SAFE_RELEASE(mAudioClient);
		SAFE_RELEASE(mAudioRenderClient);
	}

	SetEvent(mActivationEvent);
	return S_OK;
}


MSWASAPIWriterPtr MSWASAPIWriterNew()
{
	MSWASAPIWriterPtr w = new MSWASAPIWriterWrapper();
	w->writer = Make<MSWASAPIWriter>();
	return w;
}
void MSWASAPIWriterDelete(MSWASAPIWriterPtr ptr)
{
	ptr->writer->setAsNotInstantiated();
	ptr->writer = nullptr;
	delete ptr;
}
#else
MSWASAPIWriterPtr MSWASAPIWriterNew()
{
	return (MSWASAPIWriterPtr) new MSWASAPIWriter();
}
void MSWASAPIWriterDelete(MSWASAPIWriterPtr ptr)
{
	delete ptr;
}
#endif
