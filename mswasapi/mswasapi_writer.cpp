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

bool MSWASAPIWriter::smInstantiated = false;


MSWASAPIWriter::MSWASAPIWriter()
	:  mAudioClient(NULL), mAudioRenderClient(NULL), mVolumeControler(NULL), mBufferFrameCount(0), mIsInitialized(false), mIsActivated(false), mIsStarted(false)
{
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
#ifdef ENABLE_MICROSOFT_STORE_APP
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
#else
	CoInitialize(NULL);
#endif
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
	mNBlockAlign = pWfx->nBlockAlign;	// Get the selected bock size
	FREE_PTR(pWfx);
	mIsInitialized = true;
	smInstantiated = true;
	activate();
	return;

error:
	// Initialize the frame rate and the number of channels to prevent configure a resampler with crappy parameters.
	mRate = 8000;
	mNChannels = 1;
	mNBlockAlign = 16 * 1 / 8;
	return;
}

int MSWASAPIWriter::activate()
{
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
		ms_error("MSWASAPIWriter: GetDevicePeriod() failed.");
	}
	devicePeriodMs = (int)(devicePeriod / 10000);
	
	/*Compute our requested buffer duration.*/
	requestedBufferDuration = minBufferDurationMs * 1000LL * 100LL;
	
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
	result = mAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_NOPERSIST, requestedBufferDuration, 0, pUsedWfx, NULL);
	if ((result != S_OK) && (result != AUDCLNT_E_ALREADY_INITIALIZED)) {
		REPORT_ERROR("Could not initialize the MSWASAPI audio output interface [%x]", result);
	}
	mNBlockAlign = pUsedWfx->nBlockAlign;
	result = mAudioClient->GetBufferSize(&mBufferFrameCount);
	REPORT_ERROR("Could not get buffer size for the MSWASAPI audio output interface [%x]", result);
	result = mAudioClient->GetService(IID_IAudioRenderClient, (void **)&mAudioRenderClient);
	REPORT_ERROR("Could not get render service from the MSWASAPI audio output interface [%x]", result);
	result = mAudioClient->GetService(IID_ISimpleAudioVolume, (void **)&mVolumeControler);
	REPORT_ERROR("Could not get volume control service from the MSWASAPI audio output interface [%x]", result);
	mIsActivated = true;

	ms_message("Wasapi playback output initialized at %i Hz, %i channels, with buffer size %i (%i ms), device period is %i, %i-bit frames are on %i bits", (int)mRate, (int)mNChannels,
		(int)mBufferFrameCount, (int)1000*mBufferFrameCount/(mNChannels*2* mRate), devicePeriodMs, (int)pUsedWfx->wBitsPerSample, mNBlockAlign*8);
	FREE_PTR(pSupportedWfx);
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
		result = mAudioClient->Start();
		if (result != S_OK) {
			ms_error("Could not start playback on the MSWASAPI audio output interface [%x]", result);
			return;
		}
		mIsStarted = true;
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

/* We go here everytick, as the filter is declared with the IS_PUMP flag.*/
int MSWASAPIWriter::feed(MSFilter *f){
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
			ms_error("MSWASAPIWriter: cannot write output buffer of %i samples, not enough space.", inputFrames);
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
			ms_message("MSWASAPIWrite: buffer was filled with a minmum of %i ms in the last %i ms, all is good.", (int)((mMinFrameCount * 1000) / mRate), flowControlInterval);
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
