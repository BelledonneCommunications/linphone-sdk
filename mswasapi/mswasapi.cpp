/*
mswasapi.cpp

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

#include <bctoolbox/defs.h>
#include <mediastreamer2/mscommon.h>

#include "bctoolbox/charconv.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mssndcard.h"

#include "mswasapi.h"
#include "mswasapi_reader.h"
#include "mswasapi_writer.h"

#include <locale>
#include <string>

#define RELEASE_CLIENT(client)                                                                                         \
	if (client != NULL) {                                                                                              \
		client->Release();                                                                                             \
		client = NULL;                                                                                                 \
	}

#define FREE_PTR(ptr)                                                                                                  \
	if (ptr != NULL) {                                                                                                 \
		CoTaskMemFree((LPVOID)ptr);                                                                                    \
		ptr = NULL;                                                                                                    \
	}

const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const IID IID_ISimpleAudioVolume = __uuidof(ISimpleAudioVolume);
#if defined(MS2_WINDOWS_PHONE) || defined(MS2_WINDOWS_UNIVERSAL) || defined(MS2_WINDOWS_UWP)
const IID IID_IAudioClient2 = __uuidof(IAudioClient2);
#else
const IID IID_IAudioClient = __uuidof(IAudioClient);
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
#endif

// Workaround for issues on audio enhancements mode. Check void MSWASAPIReader::init(LPCWSTR id)
#if !defined(MS2_WINDOWS_PHONE) && !defined(MS2_WINDOWS_UNIVERSAL)
DEFINE_GUID(CLSID_PolicyConfig, 0x870af99c, 0x171d, 0x4f9e, 0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9);
MIDL_INTERFACE("f8679f50-850a-41cf-9c72-430f290290c8")
IPolicyConfig : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR pszDeviceName, WAVEFORMATEX * *ppFormat) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR pszDeviceName, bool bDefault, WAVEFORMATEX **ppFormat) = 0;
	virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR pszDeviceName) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR pszDeviceName, WAVEFORMATEX * ppEndpointFormatFormat,
	                                                  WAVEFORMATEX * pMixFormat) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR pszDeviceName, bool bDefault, PINT64 pmftDefaultPeriod,
	                                                      PINT64 pmftMinimumPeriod) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR pszDeviceName, PINT64 pmftPeriod) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR pszDeviceName, struct DeviceShareMode * pMode) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR pszDeviceName, struct DeviceShareMode * pMode) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR pszDeviceName, BOOL bFxStore, const PROPERTYKEY &pKey,
	                                                   PROPVARIANT *pv) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR pszDeviceName, BOOL bFxStore, const PROPERTYKEY &pKey,
	                                                   PROPVARIANT *pv) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR pszDeviceName, int eRole) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR pszDeviceName, bool bVisible) = 0;
};

void MSWasapi::changePolicies(IMMDevice *device) {
	HRESULT result;
	if (mDisableSysFx) { // Workaround: Remove enhancements mode as it can lead to break inputs on some systems
		LPWSTR endpointId = NULL;
		result = device->GetId(&endpointId);

		IPolicyConfig *policyConfig;
		result = CoCreateInstance(CLSID_PolicyConfig, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&policyConfig));
		REPORT_ERROR(("mswasapi: Disabling audio enhancements requested but could not get policy config for audio " +
		              mMediaDirectionStr + " [%x]")
		                 .c_str(),
		             result);
		PROPVARIANT var;
		PropVariantInit(&var);
		result = policyConfig->GetPropertyValue(endpointId, TRUE, PKEY_AudioEndpoint_Disable_SysFx, &var);
		var.uiVal = (USHORT)1;
		result = policyConfig->SetPropertyValue(endpointId, TRUE, PKEY_AudioEndpoint_Disable_SysFx, &var);
		policyConfig->Release();
	}
error:
	return;
}
#endif

MSWasapi::MSWasapi(MSFilter *filter, const std::string &mediaDirectionStr)
    : mAudioClient(NULL), mVolumeController(NULL), mIsActivated(false), mIsStarted(false), mBufferFrameCount(0),
      mFilter(filter) {
	mMediaDirectionStr = mediaDirectionStr;
	mTargetRate = 8000;
	mTargetNChannels = 1;
	mWBitsPerSample = 16; // The SDK limit to 16 bits
	mNBlockAlign = mWBitsPerSample * mTargetNChannels / 8;
	mDisableSysFx = false;
#ifndef MS2_WINDOWS_PHONE
	mActivationEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	if (!mActivationEvent) {
		ms_error("mswasapi: Could not create activation event of the MSWASAPI audio %s interface [%i]",
		         mediaDirectionStr.c_str(), GetLastError());
		return;
	}
#endif
}

MSWasapi::~MSWasapi() {
	destroyAudioClient();
#ifdef MS2_WINDOWS_PHONE
	FREE_PTR(mDeviceId);
#else
	if (mActivationEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(mActivationEvent);
		mActivationEvent = INVALID_HANDLE_VALUE;
	}
	if (mAudioSessionControl) {
		mAudioSessionControl->UnregisterAudioSessionNotification(this);
		RELEASE_CLIENT(mAudioSessionControl);
	}
#endif
}

void MSWasapi::start() {
	HRESULT result;

	if (!isStarted() && mIsActivated) {
		result = mAudioClient->Start();
		if (result != S_OK) {
			ms_error("mswasapi: Could not start MSWASAPI audio %s interface [%x]", mMediaDirectionStr.c_str(), result);
			return;
		}
		mIsStarted = true;
	}
}

void MSWasapi::stop() {
	HRESULT result;

	if (isStarted() && mIsActivated) {
		mIsStarted = false;
		result = mAudioClient->Stop();
		if (result != S_OK) {
			ms_error("mswasapi: Could not stop MSWASAPI audio %s interface [%x]", mMediaDirectionStr.c_str(), result);
		}
	}
}

float MSWasapi::getVolumeLevel() {
	HRESULT result;
	float volume;

	if (!mIsActivated || mVolumeController == nullptr) {
		ms_error("mswasapi: The %s instance is not started to get volume", mMediaDirectionStr.c_str());
		goto error;
	}
	result = mVolumeController->GetMasterVolume(&volume);
	REPORT_ERROR(("mswasapi: Could not get the " + mMediaDirectionStr + " master volume [%x]").c_str(), result);
	return volume;

error:
	return -1.0f;
}

void MSWasapi::setVolumeLevel(float volume) {
	HRESULT result;

	if (!mIsActivated || mVolumeController == nullptr) {
		ms_error("mswasapi: The %s instance is not started to set volume", mMediaDirectionStr.c_str());
		goto error;
	}
	result = mVolumeController->SetMasterVolume(volume, NULL);
	REPORT_ERROR(("mswasapi: Could not set the " + mMediaDirectionStr + " master volume [%x]").c_str(), result);

error:
	return;
}

int MSWasapi::getRate() const {
	return mTargetRate;
}

void MSWasapi::setRate(int rate) {
	if (mTargetRate != rate) {
		ms_filter_lock(mFilter);
		if (mAudioClient) {
			int currentRate = mTargetRate;
			mTargetRate = rate;
			if (isCurrentFormatUsable()) {
				mForceRate = OverwriteState::TO_DO;
				mScheduleUpdateFormat = true;
			} else {
				mTargetRate = currentRate;
				ms_warning("mswasapi: changing %s rate to %d Hz is not supported by the device. Keep %d Hz",
				           mMediaDirectionStr.c_str(), rate, mTargetRate);
			}
		} else
			ms_error("mswasapi: changing %s rate to %d Hz but AudioClient has not been initialized. It is not "
			         "implemented out of this scope. Keep %d Hz",
			         mMediaDirectionStr.c_str(), rate, mTargetRate);
		ms_filter_unlock(mFilter);
	}
}

int MSWasapi::getNChannels() const {
	return mTargetNChannels;
}

void MSWasapi::setNChannels(int channels) {
	if (mTargetNChannels != channels) {
		ms_filter_lock(mFilter);
		if (mAudioClient) {
			int currentChannels = mTargetNChannels;
			mTargetNChannels = channels;
			if (isCurrentFormatUsable()) {
				mForceNChannels = OverwriteState::TO_DO;
				mScheduleUpdateFormat = true;
			} else {
				mTargetNChannels = currentChannels;
				ms_warning(
				    "mswasapi: trying to change %s channel to %d is not supported by the device. Keep %d channels",
				    mMediaDirectionStr.c_str(), channels, mTargetNChannels);
			}
		} else
			ms_error("mswasapi: trying to change %s channel to %d but AudioClient has not been initialized. It is not "
			         "implemented out of this scope. Keep %d channels",
			         mMediaDirectionStr.c_str(), channels, mTargetNChannels);
		ms_filter_unlock(mFilter);
	}
}

void MSWasapi::init(MSSndCard *card, MSFilter *f) {
	WasapiSndCard *wasapicard = static_cast<WasapiSndCard *>(card->data);
	LPCWSTR id = wasapicard->id;
	bool useBestFormat = false;
	WAVEFORMATEX *pWfx = NULL;

	mDeviceName = card->name;
	mIsDefaultDevice = wasapicard->isDefault;
	mStreamCategory = wasapicard->streamCategory;
#if defined(MS2_WINDOWS_UNIVERSAL)
	mDeviceId = ref new Platform::String(id);

	if (mDeviceId == nullptr) {
		ms_error("mswasapi: Could not get the DeviceID of the MSWASAPI audio %s interface", mMediaDirectionStr.c_str());
		goto error;
	}
	if (isInstantiated()) {
		ms_error("mswasapi: An %s MSWASAPI is already instantiated. A second one can not be created.",
		         mMediaDirectionStr.c_str());
		goto error;
	}
#elif defined(MS2_WINDOWS_PHONE)
	mDeviceId = GetDefaultAudioRenderId(Communications);
	if (mDeviceId == NULL) {
		ms_error("mswasapi: Could not get the RenderId of the MSWASAPI audio %s interface", mMediaDirectionStr.c_str());
		goto error;
	}
	if (isInstantiated()) {
		ms_error("mswasapi: An %s MSWASAPI is already instantiated. A second one can not be created.",
		         mMediaDirectionStr.c_str());
		goto error;
	}
#else
	mDeviceId = id;
	if (mDeviceId == nullptr) {
		ms_error("mswasapi: Could not get the DeviceID of the MSWASAPI audio %s interface", mMediaDirectionStr.c_str());
		goto error;
	}
#endif
	if (createAudioClient()) goto error;
	useBestFormat = true;
error:
	updateFormat(useBestFormat);

#ifdef MS2_WINDOWS_UNIVERSAL
	setInstantiated(true);
#endif
	return;
}

//----------------------------------------------------------------------------------
//                                AUDIO CLIENT
//----------------------------------------------------------------------------------

int MSWasapi::createAudioClient() {
	if (mAudioClient != NULL) return -1;
#if defined(MS2_WINDOWS_UNIVERSAL)
	AudioClientProperties properties = {0};
	IActivateAudioInterfaceAsyncOperation *asyncOp;
	HRESULT result = ActivateAudioInterfaceAsync(mDeviceId->Data(), IID_IAudioClient2, NULL, this, &asyncOp);
	REPORT_ERROR(("mswasapi: Could not activate the MSWASAPI audio " + mMediaDirectionStr + " interface [%x]").c_str(),
	             result);
	WaitForSingleObjectEx(mActivationEvent, INFINITE, FALSE);
	if (mAudioClient == NULL) {
		ms_error("Could not create the MSWASAPI audio %s interface client", mMediaDirectionStr.c_str());
		goto error;
	}
#elif defined(MS2_WINDOWS_PHONE)
	AudioClientProperties properties = {0};
	HRESULT result = ActivateAudioInterface(mDeviceId, IID_IAudioClient2, (void **)&mAudioClient);
	REPORT_ERROR(("mswasapi: Could not activate the MSWASAPI audio " + mMediaDirectionStr + " interface [%x]").c_str(),
	             result);
#else
#ifdef ENABLE_MICROSOFT_STORE_APP
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
#else
	CoInitialize(NULL);
#endif
	if (mIsDefaultDevice) {
		IActivateAudioInterfaceAsyncOperation *asyncOp;
		HRESULT result = ActivateAudioInterfaceAsync(mDeviceId, __uuidof(IAudioClient2), NULL, this, &asyncOp);
		REPORT_ERROR(
		    ("mswasapi: Could not activate the MSWASAPI audio " + mMediaDirectionStr + " interface [%x]").c_str(),
		    result);
		WaitForSingleObjectEx(mActivationEvent, INFINITE, FALSE);
		if (mAudioClient == NULL) {
			ms_error("Could not create the MSWASAPI audio %s interface client", mMediaDirectionStr.c_str());
			goto error;
		}
	} else { // On win32, ActivateAudioInterfaceAsync doesn't seem to work with specific ID other than default.
		IMMDeviceEnumerator *pEnumerator = NULL;
		IMMDevice *pDevice = NULL;
		HRESULT result = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator,
		                                  (void **)&pEnumerator);
		REPORT_ERROR("mswasapi: Could not create an instance of the device enumerator [%x]", result);
		result = pEnumerator->GetDevice(mDeviceId, &pDevice);
		SAFE_RELEASE(pEnumerator);
		REPORT_ERROR("mswasapi: Could not get the rendering device [%x]", result);
		changePolicies(pDevice);
		result = pDevice->Activate(__uuidof(IAudioClient2), CLSCTX_ALL, NULL, (void **)&mAudioClient);
		SAFE_RELEASE(pDevice);
		REPORT_ERROR("mswasapi: Could not activate the rendering device [%x]", result);
	}
#endif
#if defined(MS2_WINDOWS_PHONE) || defined(MS2_WINDOWS_UNIVERSAL)
	properties.cbSize = sizeof(AudioClientProperties);
	properties.bIsOffload = false;
	properties.eCategory = AudioCategory_Communications;
	result = mAudioClient->SetClientProperties(&properties);
	REPORT_ERROR(
	    ("mswasapi: Could not set properties of the MSWASAPI audio " + mMediaDirectionStr + " interface [%x]").c_str(),
	    result);
#endif
	return 0;
error:
	return -1;
}

void MSWasapi::destroyAudioClient() {
	RELEASE_CLIENT(mAudioClient);
}

int MSWasapi::restartAudioClient() {
	stop();
	deactivate();
	updateFormat(true);
	destroyAudioClient();
	if (createAudioClient()) return -1;
	if (activate()) return -1;
	start();
	return 0;
}

//----------------------------------------------------------------------------------
//                                      FORMAT
//----------------------------------------------------------------------------------

bool MSWasapi::isFormatUpdated() {
	bool changed = false;
	WAVEFORMATEX *pWfx = NULL;
	HRESULT result = mAudioClient->GetMixFormat(&pWfx); // Get best format for shared mode.
	REPORT_ERROR(
	    ("mswasapi: Could not get the mix format of the MSWASAPI audio " + mMediaDirectionStr + " interface [%x]")
	        .c_str(),
	    result);
	changed = mForceNChannels == OverwriteState::TO_DO || mForceRate == OverwriteState::TO_DO ||
	          (mForceRate == OverwriteState::DO_NOTHING && mTargetRate != pWfx->nSamplesPerSec) ||
	          (mForceNChannels == OverwriteState::DO_NOTHING && mTargetNChannels != pWfx->nChannels);
	FREE_PTR(pWfx);

error:
	return changed;
}

void MSWasapi::updateFormat(bool useBestFormat) {
	if (!mAudioClient) return;
	if (useBestFormat) {
		useBestFormat = false;
		WAVEFORMATEX *pWfx = NULL;
		HRESULT result = mAudioClient->GetMixFormat(&pWfx); // Get best format for shared mode.
		REPORT_ERROR(
		    ("mswasapi: Could not get the mix format of the MSWASAPI audio " + mMediaDirectionStr + " interface [%x]")
		        .c_str(),
		    result);
		if (mForceRate == OverwriteState::DO_NOTHING && mTargetRate != pWfx->nSamplesPerSec) {
			mTargetRate = pWfx->nSamplesPerSec;
		}
		if (mForceNChannels == OverwriteState::DO_NOTHING && mTargetNChannels != pWfx->nChannels) {
			mTargetNChannels = pWfx->nChannels;
		}
		FREE_PTR(pWfx);
		useBestFormat = true;
	}
error:
	if (mWBitsPerSample != 16) {
		mWBitsPerSample = 16; // The SDK limit to 16 bits
	}

	if (!useBestFormat) {
		// Initialize the frame rate and the number of channels to be able to generate silence.
		if (mTargetRate != 8000) { // Read
			mTargetRate = 8000;    // Write
		}                          // Avoid to use writting operation if not needed.
		if (mTargetNChannels != 1) {
			mTargetNChannels = 1;
		}
	}
	mNBlockAlign = mWBitsPerSample * mTargetNChannels / 8;
}

WAVEFORMATPCMEX MSWasapi::buildFormat() const {
	WAVEFORMATPCMEX wfx;
	wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	wfx.Format.nChannels = (WORD)mTargetNChannels;
	wfx.Format.nSamplesPerSec = mTargetRate;
	wfx.Format.wBitsPerSample = (WORD)mWBitsPerSample;
	wfx.Format.nAvgBytesPerSec = (DWORD)mTargetRate * mTargetNChannels * mWBitsPerSample / 8;
	wfx.Format.nBlockAlign = (WORD)mNBlockAlign;
	wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
	wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
	wfx.Samples.wValidBitsPerSample = mWBitsPerSample;
	switch (mTargetNChannels) {
		case 1:
			wfx.dwChannelMask = SPEAKER_FRONT_CENTER;
			break;
		case 2:
			wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
			break;
		case 4:
			wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
			break;
		case 6:
			wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
			                    SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
			break;
		case 8:
			wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
			                    SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
			                    SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER;
			break;
		default:
			wfx.dwChannelMask = 0;
			break;
	}
	return wfx;
}

bool MSWasapi::isCurrentFormatUsable() const {
#if !defined(MS2_WINDOWS_UNIVERSAL)
#if defined(ENABLE_MICROSOFT_STORE_APP) || defined(MS2_WINDOWS_UNIVERSAL)
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
#else
	CoInitialize(NULL);
#endif
#endif
	WAVEFORMATPCMEX proposedWfx = buildFormat();
	WAVEFORMATEX *pSupportedWfx = NULL;
	HRESULT result =
	    mAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX *)&proposedWfx, &pSupportedWfx);
	FREE_PTR(pSupportedWfx);
	return result == S_OK;
}

void MSWasapi::tryToUpdateFormat() {
	if (mScheduleUpdateFormat) {
		mScheduleUpdateFormat = false;
		if (isFormatUpdated()) {
			if (restartAudioClient()) {
				mScheduleUpdateFormat = true; // Couldn't restart. Reschedule the update.
				return;
			}
			if (mForceNChannels == OverwriteState::TO_DO) mForceNChannels = OverwriteState::DONE;
			if (mForceRate == OverwriteState::TO_DO) mForceRate = OverwriteState::DONE;
			ms_message("mswasapi: %s updated for [%s] at %i Hz, %i channels, with buffer size %i (%i ms), %i-bit "
			           "frames are on %i bits",
			           mMediaDirectionStr.c_str(), mDeviceName.c_str(), (int)mTargetRate, (int)mTargetNChannels,
			           (int)mBufferFrameCount, (int)1000 * mBufferFrameCount / mTargetRate, mWBitsPerSample,
			           mNBlockAlign * 8);
			ms_filter_notify_no_arg(mFilter, MS_FILTER_OUTPUT_FMT_CHANGED);
		}
	}
	return;
}

//----------------------------------------------------------------------------------

#ifndef MS2_WINDOWS_PHONE

//----------------------------------------------------------------------------------
//                  IActivateAudioInterfaceCompletionHandler
//----------------------------------------------------------------------------------

HRESULT MSWasapi::QueryInterface(REFIID riid, void **ppvObject) {
	if (riid == IID_IUnknown || riid == IID_IAgileObject) // Need IID_IAgileObject for ActivateAudioInterfaceAsync
		*ppvObject = this;
	else {
		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

ULONG MSWasapi::AddRef(void) {
	return InterlockedIncrement(&m_refCount);
}

ULONG MSWasapi::Release(void) {
	ULONG count = InterlockedDecrement(&m_refCount);
	if (count == 0) delete this;
	return count;
}

// This will be called on MTA thread when results of the activation are available.
HRESULT MSWasapi::ActivateCompleted(IActivateAudioInterfaceAsyncOperation *operation) {
	HRESULT hr = S_OK;
	HRESULT hrActivateResult = S_OK;
	IUnknown *audioInterface = NULL;

	hr = operation->GetActivateResult(&hrActivateResult, &audioInterface);
	if (SUCCEEDED(hr) && SUCCEEDED(hrActivateResult)) {
		audioInterface->QueryInterface(IID_PPV_ARGS(&mAudioClient));
		if (mAudioClient == NULL) {
			hr = E_FAIL;
			goto exit;
		}else{
			if(mStreamCategory != AudioCategory_Other){
				AudioClientProperties prop = {0};
				prop.eCategory = mStreamCategory ;
				mAudioClient->SetClientProperties(&prop);
			}
		}
	} else
		ms_error("mswasapi: Could not activate the MSWASAPI audio %s interface [%x]", mMediaDirectionStr.c_str(),
		         hrActivateResult);
exit:
	SAFE_RELEASE(audioInterface);

	if (FAILED(hr)) {
		SAFE_RELEASE(mAudioClient);
	}

	SetEvent(mActivationEvent);
	return S_OK;
}

//----------------------------------------------------------------------------------
//                            IAudioSessionEvents
//----------------------------------------------------------------------------------

HRESULT MSWasapi::OnSessionDisconnected(AudioSessionDisconnectReason disconnectReason) {
	if (disconnectReason == DisconnectReasonFormatChanged) mScheduleUpdateFormat = true;
	return S_OK;
}
HRESULT MSWasapi::OnDisplayNameChanged(LPCWSTR NewDisplayName, LPCGUID EventContext) {
	return S_OK;
}

HRESULT MSWasapi::OnIconPathChanged(LPCWSTR NewIconPath, LPCGUID EventContext) {
	return S_OK;
}

HRESULT MSWasapi::OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext) {
	return S_OK;
}

HRESULT MSWasapi::OnChannelVolumeChanged(DWORD ChannelCount,
                                         float NewChannelVolumeArray[],
                                         DWORD ChangedChannel,
                                         LPCGUID EventContext) {
	return S_OK;
}

HRESULT MSWasapi::OnGroupingParamChanged(LPCGUID NewGroupingParam, LPCGUID EventContext) {
	return S_OK;
}

HRESULT MSWasapi::OnStateChanged(AudioSessionState NewState) {
	mScheduleUpdateFormat = true;
	return S_OK;
}

//----------------------------------------------------------------------------------

#endif

/******************************************************************************
 * Methods to (de)initialize and run the WASAPI sound capture filter          *
 *****************************************************************************/

static void ms_wasapi_read_init(MSFilter *f) {
	MSWASAPIReaderPtr r = MSWASAPIReaderNew(f);
	f->data = r;
}

static void ms_wasapi_read_preprocess(MSFilter *f) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	if (!r->activate()) r->start();
}

static void ms_wasapi_read_process(MSFilter *f) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	ms_filter_lock(f);
	r->tryToUpdateFormat();
	r->feed(f);
	ms_filter_unlock(f);
}

static void ms_wasapi_read_postprocess(MSFilter *f) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	r->stop();
	r->deactivate();
}

static void ms_wasapi_read_uninit(MSFilter *f) {
	MSWASAPIReaderDelete(static_cast<MSWASAPIReaderPtr>(f->data));
}

/******************************************************************************
 * Methods to configure the WASAPI sound capture filter                       *
 *****************************************************************************/

static int ms_wasapi_read_get_sample_rate(MSFilter *f, void *arg) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	*((int *)arg) = r->getRate();
	return 0;
}

static int ms_wasapi_read_set_sample_rate(BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(void *arg)) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	int sampleRate = 0;
	r->setRate(*((int *)arg));
	sampleRate = r->getRate();
	if (sampleRate == *((int *)arg)) // We are setting what the Audio Client managed : do not considered it as an error
	                                 // to avoid misleading debug logs.
		return 0;
	else return -1;
}

static int ms_wasapi_read_get_nchannels(MSFilter *f, void *arg) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	*((int *)arg) = r->getNChannels();
	return 0;
}

static int ms_wasapi_read_set_nchannels(BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(void *arg)) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	int channelCount = 2;
	r->setNChannels(*((int *)arg)); // Note that Mediastreamer use only 1 or 2 channels.
	channelCount = r->getNChannels();
	if (channelCount == *((int *)arg)) // We are setting what the Audio Client managed : do not considered it as an
	                                   // error to avoid misleading debug logs.
		return 0;
	else return -1;
}

static int ms_wasapi_read_set_volume_gain(MSFilter *f, void *arg) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	r->setVolumeLevel(*(float *)arg);
	return 0;
}

static int ms_wasapi_read_get_volume_gain(MSFilter *f, void *arg) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	float *volume = (float *)arg;
	*volume = r->getVolumeLevel();
	return *volume >= 0.0f ? 0 : -1;
}

static MSFilterMethod ms_wasapi_read_methods[] = {{MS_FILTER_SET_SAMPLE_RATE, ms_wasapi_read_set_sample_rate},
                                                  {MS_FILTER_GET_SAMPLE_RATE, ms_wasapi_read_get_sample_rate},
                                                  {MS_FILTER_SET_NCHANNELS, ms_wasapi_read_set_nchannels},
                                                  {MS_FILTER_GET_NCHANNELS, ms_wasapi_read_get_nchannels},
                                                  {MS_AUDIO_CAPTURE_SET_VOLUME_GAIN, ms_wasapi_read_set_volume_gain},
                                                  {MS_AUDIO_CAPTURE_GET_VOLUME_GAIN, ms_wasapi_read_get_volume_gain},
                                                  {0, NULL}};

/******************************************************************************
 * Definition of the WASAPI sound capture filter                              *
 *****************************************************************************/

#define MS_WASAPI_READ_ID MS_FILTER_PLUGIN_ID
#define MS_WASAPI_READ_NAME "MSWASAPIRead"
#define MS_WASAPI_READ_DESCRIPTION "Windows Audio Session sound capture"
#define MS_WASAPI_READ_CATEGORY MS_FILTER_OTHER
#define MS_WASAPI_READ_ENC_FMT NULL
#define MS_WASAPI_READ_NINPUTS 0
#define MS_WASAPI_READ_NOUTPUTS 1
#define MS_WASAPI_READ_FLAGS 0

#ifndef _MSC_VER

MSFilterDesc ms_wasapi_read_desc = {.id = MS_WASAPI_READ_ID,
                                    .name = MS_WASAPI_READ_NAME,
                                    .text = MS_WASAPI_READ_DESCRIPTION,
                                    .category = MS_WASAPI_READ_CATEGORY,
                                    .enc_fmt = MS_WASAPI_READ_ENC_FMT,
                                    .ninputs = MS_WASAPI_READ_NINPUTS,
                                    .noutputs = MS_WASAPI_READ_NOUTPUTS,
                                    .init = ms_wasapi_read_init,
                                    .preprocess = ms_wasapi_read_preprocess,
                                    .process = ms_wasapi_read_process,
                                    .postprocess = ms_wasapi_read_postprocess,
                                    .uninit = ms_wasapi_read_uninit,
                                    .methods = ms_wasapi_read_methods,
                                    .flags = MS_WASAPI_READ_FLAGS};

#else

MSFilterDesc ms_wasapi_read_desc = {MS_WASAPI_READ_ID,       MS_WASAPI_READ_NAME,        MS_WASAPI_READ_DESCRIPTION,
                                    MS_WASAPI_READ_CATEGORY, MS_WASAPI_READ_ENC_FMT,     MS_WASAPI_READ_NINPUTS,
                                    MS_WASAPI_READ_NOUTPUTS, ms_wasapi_read_init,        ms_wasapi_read_preprocess,
                                    ms_wasapi_read_process,  ms_wasapi_read_postprocess, ms_wasapi_read_uninit,
                                    ms_wasapi_read_methods,  MS_WASAPI_READ_FLAGS};

#endif

MS_FILTER_DESC_EXPORT(ms_wasapi_read_desc)

/******************************************************************************
 * Methods to (de)initialize and run the WASAPI sound output filter           *
 *****************************************************************************/

static void ms_wasapi_write_init(MSFilter *f) {
	MSWASAPIWriterPtr w = MSWASAPIWriterNew(f);
	f->data = w;
}

static void ms_wasapi_write_preprocess(MSFilter *f) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	if (!w->activate()) w->start();
}

static void ms_wasapi_write_process(MSFilter *f) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	ms_filter_lock(f);
	w->tryToUpdateFormat();
	w->feed(f);
	ms_filter_unlock(f);
}

static void ms_wasapi_write_postprocess(MSFilter *f) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	w->stop();
	w->deactivate();
}

static void ms_wasapi_write_uninit(MSFilter *f) {
	MSWASAPIWriterDelete(static_cast<MSWASAPIWriterPtr>(f->data));
}

/******************************************************************************
 * Methods to configure the WASAPI sound output filter                        *
 *****************************************************************************/

static int ms_wasapi_write_get_sample_rate(MSFilter *f, void *arg) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	*((int *)arg) = w->getRate();
	return 0;
}

static int ms_wasapi_write_set_sample_rate(BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(void *arg)) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	int sampleRate = 0;
	w->setRate(*((int *)arg));
	sampleRate = w->getRate();
	if (sampleRate == *((int *)arg)) // We are setting what the Audio Client managed : do not considered it as an error
	                                 // to avoid misleading debug logs.
		return 0;
	else return -1;
}

static int ms_wasapi_write_get_nchannels(MSFilter *f, void *arg) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	*((int *)arg) = w->getNChannels();
	return 0;
}

static int ms_wasapi_write_set_nchannels(BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(void *arg)) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	int channelCount = 2;
	w->setNChannels(*((int *)arg)); // Note that Mediastreamer use only 1 or 2 channels.
	channelCount = w->getNChannels();
	if (channelCount == *((int *)arg)) // We are setting what the Audio Client managed : do not considered it as an
	                                   // error to avoid misleading debug logs.
		return 0;
	else return -1;
}

static int ms_wasapi_write_set_volume_gain(MSFilter *f, void *arg) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	w->setVolumeLevel(*(float *)arg);
	return 0;
}

static int ms_wasapi_write_get_volume_gain(MSFilter *f, void *arg) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	float *volume = (float *)arg;
	*volume = w->getVolumeLevel();
	return *volume >= 0.0f ? 0 : -1;
}

static MSFilterMethod ms_wasapi_write_methods[] = {{MS_FILTER_SET_SAMPLE_RATE, ms_wasapi_write_set_sample_rate},
                                                   {MS_FILTER_GET_SAMPLE_RATE, ms_wasapi_write_get_sample_rate},
                                                   {MS_FILTER_SET_NCHANNELS, ms_wasapi_write_set_nchannels},
                                                   {MS_FILTER_GET_NCHANNELS, ms_wasapi_write_get_nchannels},
                                                   {MS_AUDIO_PLAYBACK_SET_VOLUME_GAIN, ms_wasapi_write_set_volume_gain},
                                                   {MS_AUDIO_PLAYBACK_GET_VOLUME_GAIN, ms_wasapi_write_get_volume_gain},
                                                   {0, NULL}};

/******************************************************************************
 * Definition of the WASAPI sound output filter                               *
 *****************************************************************************/

#define MS_WASAPI_WRITE_ID MS_FILTER_PLUGIN_ID
#define MS_WASAPI_WRITE_NAME "MSWASAPIWrite"
#define MS_WASAPI_WRITE_DESCRIPTION "Windows Audio Session sound output"
#define MS_WASAPI_WRITE_CATEGORY MS_FILTER_OTHER
#define MS_WASAPI_WRITE_ENC_FMT NULL
#define MS_WASAPI_WRITE_NINPUTS 1
#define MS_WASAPI_WRITE_NOUTPUTS 0
#define MS_WASAPI_WRITE_FLAGS MS_FILTER_IS_PUMP

#ifndef _MSC_VER

MSFilterDesc ms_wasapi_write_desc = {.id = MS_WASAPI_WRITE_ID,
                                     .name = MS_WASAPI_WRITE_NAME,
                                     .text = MS_WASAPI_WRITE_DESCRIPTION,
                                     .category = MS_WASAPI_WRITE_CATEGORY,
                                     .enc_fmt = MS_WASAPI_WRITE_ENC_FMT,
                                     .ninputs = MS_WASAPI_WRITE_NINPUTS,
                                     .noutputs = MS_WASAPI_WRITE_NOUTPUTS,
                                     .init = ms_wasapi_write_init,
                                     .preprocess = ms_wasapi_write_preprocess,
                                     .process = ms_wasapi_write_process,
                                     .postprocess = ms_wasapi_write_postprocess,
                                     .uninit = ms_wasapi_write_uninit,
                                     .methods = ms_wasapi_write_methods,
                                     .flags = MS_WASAPI_WRITE_FLAGS};

#else

MSFilterDesc ms_wasapi_write_desc = {MS_WASAPI_WRITE_ID,       MS_WASAPI_WRITE_NAME,        MS_WASAPI_WRITE_DESCRIPTION,
                                     MS_WASAPI_WRITE_CATEGORY, MS_WASAPI_WRITE_ENC_FMT,     MS_WASAPI_WRITE_NINPUTS,
                                     MS_WASAPI_WRITE_NOUTPUTS, ms_wasapi_write_init,        ms_wasapi_write_preprocess,
                                     ms_wasapi_write_process,  ms_wasapi_write_postprocess, ms_wasapi_write_uninit,
                                     ms_wasapi_write_methods,  MS_WASAPI_WRITE_FLAGS};

#endif

MS_FILTER_DESC_EXPORT(ms_wasapi_write_desc)

static void ms_wasapi_snd_card_detect(MSSndCardManager *m);
static MSFilter *ms_wasapi_snd_card_create_reader(MSSndCard *card);
static MSFilter *ms_wasapi_snd_card_create_writer(MSSndCard *card);

static void ms_wasapi_snd_card_init(MSSndCard *card) {
	WasapiSndCard *c = static_cast<WasapiSndCard *>(ms_new0(WasapiSndCard, 1));
	card->data = c;
}

static void ms_wasapi_snd_card_uninit(MSSndCard *card) {
	WasapiSndCard *c = static_cast<WasapiSndCard *>(card->data);
	if (c->id_vector) delete c->id_vector;
	ms_free(c);
}

static MSSndCardDesc ms_wasapi_snd_card_desc = {"WASAPI",
                                                ms_wasapi_snd_card_detect,
                                                ms_wasapi_snd_card_init,
                                                NULL,
                                                NULL,
                                                NULL,
                                                NULL,
                                                NULL,
                                                ms_wasapi_snd_card_create_reader,
                                                ms_wasapi_snd_card_create_writer,
                                                ms_wasapi_snd_card_uninit,
                                                NULL,
                                                NULL};

#if defined(MS2_WINDOWS_PHONE)
static MSSndCard *ms_wasapi_phone_snd_card_new(void) {
	MSSndCard *card = ms_snd_card_new(&ms_wasapi_snd_card_desc);
	card->name = ms_strdup("WASAPI sound card");
	card->latency = 250;
	return card;
}

#elif defined(MS2_WINDOWS_UNIVERSAL)

class MSWASAPIDeviceEnumerator {
public:
	MSWASAPIDeviceEnumerator() : _l(NULL) {
		_DetectEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
		if (!_DetectEvent) {
			ms_error("mswasapi: Could not create detect event [%i]", GetLastError());
			return;
		}
	}
	~MSWASAPIDeviceEnumerator() {
		bctbx_list_free(_l);
		if (_DetectEvent != INVALID_HANDLE_VALUE) {
			CloseHandle(_DetectEvent);
			_DetectEvent = INVALID_HANDLE_VALUE;
		}
	}

	MSSndCard *NewCard(String ^ DeviceId, const char *name, uint8_t capabilities) {
		const wchar_t *id = DeviceId->Data();
		MSSndCard *card = ms_snd_card_new(&ms_wasapi_snd_card_desc);
		WasapiSndCard *wasapicard = static_cast<WasapiSndCard *>(card->data);
		card->name = ms_strdup(name);
		card->capabilities = capabilities;
		wasapicard->id_vector = new std::vector<wchar_t>(wcslen(id) + 1);
		wcscpy_s(&wasapicard->id_vector->front(), wasapicard->id_vector->size(), id);
		wasapicard->id = &wasapicard->id_vector->front();
		return card;
	}

	void AddOrUpdateCard(String ^ DeviceId, String ^ DeviceName, DeviceClass dc) {
		char *name;
		const bctbx_list_t *elem = _l;
		size_t inputlen;
		size_t returnlen;
		uint8_t capabilities = 0;
		int err;

		inputlen = wcslen(DeviceName->Data()) + 1;
		returnlen = inputlen * 2;
		name = (char *)ms_malloc(returnlen);
		UINT currentCodePage = bctbx_get_code_page(NULL);
		if ((err = WideCharToMultiByte(currentCodePage, 0, DeviceName->Data(), -1, name, (int)returnlen, NULL, NULL)) ==
		    0) {
			ms_error("mswasapi: Cannot convert card name to multi-byte string.");
			return;
		}
		switch (dc) {
			case DeviceClass::AudioRender:
				capabilities = MS_SND_CARD_CAP_PLAYBACK;
				break;
			case DeviceClass::AudioCapture:
				capabilities = MS_SND_CARD_CAP_CAPTURE;
				break;
			default:
				capabilities = MS_SND_CARD_CAP_PLAYBACK | MS_SND_CARD_CAP_CAPTURE;
				break;
		}

		for (; elem != NULL; elem = elem->next) {
			MSSndCard *card = static_cast<MSSndCard *>(elem->data);
			WasapiSndCard *d = static_cast<WasapiSndCard *>(card->data);
			if (wcscmp(d->id, DeviceId->Data()) == 0) {
				/* Update an existing card. */
				if (card->name != NULL) {
					ms_free(card->name);
					card->name = ms_strdup(name);
				}
				card->capabilities |= capabilities;
				ms_free(name);
				return;
			}
		}

		/* Add a new card. */
		_l = bctbx_list_append(_l, NewCard(DeviceId, name, capabilities));
		ms_free(name);
	}

	void Detect(DeviceClass dc) {
		_dc = dc;
		String ^ DefaultId;
		String ^ DefaultName = "Default audio card";
		if (_dc == DeviceClass::AudioCapture) {
			DefaultId = MediaDevice::GetDefaultAudioCaptureId(AudioDeviceRole::Communications);
		} else {
			DefaultId = MediaDevice::GetDefaultAudioRenderId(AudioDeviceRole::Communications);
		}
		if (DefaultId != "") AddOrUpdateCard(DefaultId, DefaultName, _dc);
		Windows::Foundation::IAsyncOperation<DeviceInformationCollection ^> ^ op = DeviceInformation::FindAllAsync(_dc);
		op->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<DeviceInformationCollection ^>(
		    [this](Windows::Foundation::IAsyncOperation<DeviceInformationCollection ^> ^ asyncOp,
		           Windows::Foundation::AsyncStatus asyncStatus) {
			    if (asyncStatus == Windows::Foundation::AsyncStatus::Completed) {
				    DeviceInformationCollection ^ deviceInfoCollection = asyncOp->GetResults();
				    if ((deviceInfoCollection == nullptr) || (deviceInfoCollection->Size == 0)) {
					    ms_error("mswasapi: No audio device found");
				    } else {
					    try {
						    for (unsigned int i = 0; i < deviceInfoCollection->Size; i++) {
							    DeviceInformation ^ deviceInfo = deviceInfoCollection->GetAt(i);
							    AddOrUpdateCard(deviceInfo->Id, deviceInfo->Name, _dc);
						    }
					    } catch (Platform::Exception ^ e) {
						    ms_error("mswaspi: Error of audio device detection");
					    }
				    }
			    } else {
				    ms_error("mswasapi: DeviceInformation::FindAllAsync failed");
			    }
			    SetEvent(_DetectEvent);
		    });
		WaitForSingleObjectEx(_DetectEvent, INFINITE, FALSE);
	}

	void Detect() {
		Detect(DeviceClass::AudioCapture);
		Detect(DeviceClass::AudioRender);
	}

	bctbx_list_t *GetList() {
		return _l;
	}

private:
	bctbx_list_t *_l;
	DeviceClass _dc;
	HANDLE _DetectEvent;
};

#else

static MSSndCard *ms_wasapi_snd_card_new(LPWSTR id, const char *name, uint8_t capabilities, AUDIO_STREAM_CATEGORY streamCategory, bool isDefault) {
	MSSndCard *card = ms_snd_card_new(&ms_wasapi_snd_card_desc);
	WasapiSndCard *wasapicard = static_cast<WasapiSndCard *>(card->data);
	card->name = ms_strdup(name);
	card->capabilities = capabilities;
	card->latency = (capabilities & MS_SND_CARD_CAP_CAPTURE) ? 70 : 0;
	wasapicard->id_vector = new std::vector<wchar_t>(wcslen(id) + 1);
	wcscpy_s(&wasapicard->id_vector->front(), wasapicard->id_vector->size(), id);
	wasapicard->id = &wasapicard->id_vector->front();
	wasapicard->isDefault = isDefault;
	wasapicard->streamCategory = streamCategory;
	return card;
}

static void add_or_update_card(
    MSSndCardManager *m, bctbx_list_t **l, LPWSTR id, LPWSTR wname, EDataFlow data_flow, AUDIO_STREAM_CATEGORY streamCategory, bool isDefault) {
	MSSndCard *card;
	const bctbx_list_t *elem = *l;
	uint8_t capabilities = 0;
	char *idStr = NULL;
	char *nameStr = NULL;
	char *name = NULL;
	size_t inputlen = wcslen(wname) + 1;
	size_t returnlen;
	UINT currentCodePage = bctbx_get_code_page(NULL);
	int sizeNeeded = WideCharToMultiByte(currentCodePage, 0, wname, (int)inputlen, NULL, 0, NULL, NULL);
	std::string strConversion(sizeNeeded, 0);
	if (WideCharToMultiByte(currentCodePage, 0, wname, (int)inputlen, &strConversion[0], sizeNeeded, NULL, NULL)) {
		size_t size = strConversion.length() + 1;
		nameStr = (char *)ms_malloc(size);
		strcpy(nameStr, strConversion.c_str());
		nameStr[size - 1] = '\0';
	}
	if (!nameStr) {
		ms_error("mswasapi: Cannot convert card name to multi-byte string.");
		goto error;
	}
	inputlen = wcslen(id) + 1;
	idStr = (char *)ms_malloc(inputlen);
	if (!idStr || wcstombs_s(&returnlen, idStr, inputlen, id, inputlen) != 0) {
		ms_error("mswasapi: Cannot convert card id to multi-byte string.");
		goto error;
	}

	if (isDefault) {
		name = ms_strdup_printf("Default %s",
		                        (data_flow == eCapture ? "Capture" : "Playback"));
	} else name = ms_strdup(nameStr);
	switch (data_flow) {
		case eRender:
			capabilities = MS_SND_CARD_CAP_PLAYBACK;
			break;
		case eCapture:
			capabilities = MS_SND_CARD_CAP_CAPTURE;
			break;
		case eAll:
		default:
			capabilities = MS_SND_CARD_CAP_PLAYBACK | MS_SND_CARD_CAP_CAPTURE;
			break;
	}
	if (!isDefault) { // Update only on non default card because it is just redondant.
		for (; elem != NULL; elem = elem->next) {
			card = static_cast<MSSndCard *>(elem->data);
			WasapiSndCard *d = static_cast<WasapiSndCard *>(card->data);
			if (wcscmp(d->id, id) == 0) {
				/* Update an existing card. */
				card->capabilities |= capabilities;
				goto error; // Not really an error
			}
		}
	}

	/* Add a new card. */
	*l = bctbx_list_append(*l, ms_wasapi_snd_card_new(id, name, capabilities, streamCategory, isDefault));
error:
	if (nameStr) {
		ms_free(nameStr);
	}
	if (name) {
		ms_free(name);
	}
	if (idStr) {
		ms_free(idStr);
	}
}

static void add_endpoint(
	MSSndCardManager *m, EDataFlow data_flow, bctbx_list_t **l, IMMDevice *pEndpoint, AUDIO_STREAM_CATEGORY streamCategory, bool isDefault) {
	IPropertyStore *pProps = NULL;
	LPWSTR pwszID = NULL;
	HRESULT result;
	if (isDefault)
		result =
		    StringFromIID((data_flow == eRender ? DEVINTERFACE_AUDIO_RENDER : DEVINTERFACE_AUDIO_CAPTURE), &pwszID);
	else result = pEndpoint->GetId(&pwszID);
	REPORT_ERROR("mswasapi: Could not get ID of audio endpoint", result);
	result = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
	REPORT_ERROR("mswasapi: Could not open property store", result);
	PROPVARIANT varName;
	PropVariantInit(&varName);
	result = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
	REPORT_ERROR("mswasapi: Could not get friendly-name of audio endpoint", result);
	add_or_update_card(m, l, pwszID, varName.pwszVal, data_flow, streamCategory, isDefault);
	CoTaskMemFree(pwszID);
	pwszID = NULL;
	PropVariantClear(&varName);
	SAFE_RELEASE(pProps);
error:
	CoTaskMemFree(pwszID);
	SAFE_RELEASE(pProps);
}

static void ms_wasapi_snd_card_detect_with_data_flow(MSSndCardManager *m, EDataFlow data_flow, bctbx_list_t **l) {
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDeviceCollection *pCollection = NULL;
	IMMDevice *pEndpoint = NULL;
	HRESULT result;
#if !defined(MS2_WINDOWS_UNIVERSAL)
#ifdef ENABLE_MICROSOFT_STORE_APP
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
#else
	CoInitialize(NULL);
#endif
#endif

	result =
	    CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void **)&pEnumerator);

	REPORT_ERROR("mswasapi: Could not create an instance of the device enumerator", result);
	result = pEnumerator->GetDefaultAudioEndpoint(data_flow, eCommunications, &pEndpoint);
	if (result == S_OK) {
		add_endpoint(m, data_flow, l, pEndpoint, AudioCategory_Communications, true);	// Follow communication stream for default devices
		SAFE_RELEASE(pEndpoint);
	}
	result = pEnumerator->EnumAudioEndpoints(data_flow, DEVICE_STATE_ACTIVE, &pCollection);
	REPORT_ERROR("mswasapi: Could not enumerate audio endpoints", result);
	UINT count;
	result = pCollection->GetCount(&count);
	REPORT_ERROR("mswasapi: Could not get the number of audio endpoints", result);
	if (count == 0) {
		ms_warning("mswasapi: No audio endpoint found");
		return;
	}
	for (ULONG i = 0; i < count; i++) {
		result = pCollection->Item(i, &pEndpoint);
		REPORT_ERROR("mswasapi: Could not get pointer to audio endpoint", result);
		add_endpoint(m, data_flow, l, pEndpoint, AudioCategory_Other, false);// AudioCategory_Other will not override stream category of the audio session for this end point
		SAFE_RELEASE(pEndpoint);
	}
error:
	SAFE_RELEASE(pEnumerator);
	SAFE_RELEASE(pCollection);
	SAFE_RELEASE(pEndpoint);
}
#endif

static void ms_wasapi_snd_card_detect(MSSndCardManager *m) {
#if defined(MS2_WINDOWS_PHONE)
	MSSndCard *card = ms_wasapi_phone_snd_card_new();
	ms_snd_card_set_manager(m, card);
	ms_snd_card_manager_add_card(m, card);
#elif defined(MS2_WINDOWS_UNIVERSAL)
	MSWASAPIDeviceEnumerator *enumerator = new MSWASAPIDeviceEnumerator();
	enumerator->Detect();
	ms_snd_card_manager_prepend_cards(m, enumerator->GetList());
	delete enumerator;
#else
	bctbx_list_t *l = NULL;
	ms_wasapi_snd_card_detect_with_data_flow(m, eCapture, &l);
	ms_wasapi_snd_card_detect_with_data_flow(m, eRender, &l);
	ms_snd_card_manager_prepend_cards(m, l);
	bctbx_list_free(l);
#endif
}

static MSFilter *ms_wasapi_snd_card_create_reader(MSSndCard *card) {
	MSFilter *f = ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &ms_wasapi_read_desc);
	MSWASAPIReaderType reader = MSWASAPI_READER(f->data);
	reader->init(card, f);
	return f;
}

static MSFilter *ms_wasapi_snd_card_create_writer(MSSndCard *card) {
	MSFilter *f = ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &ms_wasapi_write_desc);
	MSWASAPIWriterType writer = MSWASAPI_WRITER(f->data);
	writer->init(card, f);
	return f;
}

#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) extern "C" __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) extern "C" type
#endif

MS_PLUGIN_DECLARE(void) libmswasapi_init(MSFactory *factory) {
	MSSndCardManager *manager = ms_factory_get_snd_card_manager(factory);
	ms_snd_card_manager_register_desc(manager, &ms_wasapi_snd_card_desc);
	ms_message("libmswasapi plugin loaded");
}
