/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
 *
 * This file is part of mswasapi library - modular sound and video processing and streaming Windows Audio Session API
 * sound card plugin for mediastreamer2 (see https://gitlab.linphone.org/BC/public/mswasapi).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/mssndcard.h>

#include <audioclient.h>
#include <objbase.h>
#include <vector>
#ifdef MS2_WINDOWS_UNIVERSAL
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <wrl\implements.h>
#endif
#ifdef MS2_WINDOWS_PHONE
#include <phoneaudioclient.h>
#else
// clang-format off
#include <initguid.h> // Put it at first or there will be a mess with MMDeviceAPI and PKEY_AudioEndpoint* symbols
#include <audiopolicy.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include "system_notifier.h"
// clang-format on
#endif

#ifdef MS2_WINDOWS_UNIVERSAL
#include <ppltasks.h>

using namespace Microsoft::WRL;
using namespace Platform;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Media::Devices;
#endif

typedef struct WasapiSndCard {
	std::vector<wchar_t> *id_vector;
	LPWSTR id;
	bool isDefault;
	AUDIO_STREAM_CATEGORY streamCategory;
} WasapiSndCard;

class MSWasapi
#ifdef MS2_WINDOWS_UNIVERSAL
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
                          FtmBase,
                          IActivateAudioInterfaceCompletionHandler,
                          IAudioSessionEvents> {
#elif defined(MS2_WINDOWS_PHONE)
{
#else
    : public IActivateAudioInterfaceCompletionHandler,
      public IAudioSessionEvents,
      public SystemNotifier {
#endif
public:
	IAudioClient2 *mAudioClient;
	AUDIO_STREAM_CATEGORY mStreamCategory = AudioCategory_Communications;

#ifdef MS2_WINDOWS_PHONE
	LPCWSTR mDeviceId;
#elif defined MS2_WINDOWS_UNIVERSAL
	Platform::String ^ mDeviceId;
	HANDLE mActivationEvent;
	IAudioSessionControl *mAudioSessionControl = nullptr;
#else
	LPCWSTR mDeviceId;
	HANDLE mActivationEvent;
	IAudioSessionControl *mAudioSessionControl = nullptr;
#endif
	typedef enum { DO_NOTHING, DONE, TO_DO } OverwriteState;

	int mTargetRate;
	OverwriteState mForceRate = DO_NOTHING;
	int mTargetNChannels;
	OverwriteState mForceNChannels = DO_NOTHING;
	int mNBlockAlign;
	int mWBitsPerSample;
	std::string mMediaDirectionStr;
	bool mDisableSysFx; // Option to remove audio enhancements mode. This mode can break inputs on some systems.
	std::string mDeviceName;
	bool mIsDefaultDevice = false;
	bool mIsActivated;
	bool mIsStarted = false;
	bool mScheduleUpdateFormat = false; /* Update format on next msticker loop */
	UINT32 mBufferFrameCount;           /* The buffer size obtained from the wasapi.*/
	MSFilter *mFilter;
	ISimpleAudioVolume *mVolumeController;

	// capabilities: MS_SND_CARD_CAP_CAPTURE/MS_SND_CARD_CAP_PLAYBACK
	MSWasapi(MSFilter *filter, const uint8_t capabilities);
	virtual ~MSWasapi();

#ifndef MS2_WINDOWS_PHONE
	// IActivateAudioInterfaceCompletionHandler
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;
	virtual ULONG STDMETHODCALLTYPE AddRef(void) override;
	virtual ULONG STDMETHODCALLTYPE Release(void) override;
	STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation *operation);

	// IAudioSessionEvents
	virtual HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason) override;
	virtual HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR NewDisplayName, LPCGUID EventContext) override;
	virtual HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR NewIconPath, LPCGUID EventContext) override;
	virtual HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float NewVolume,
	                                                        BOOL NewMute,
	                                                        LPCGUID EventContext) override;
	virtual HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD ChannelCount,
	                                                         float NewChannelVolumeArray[],
	                                                         DWORD ChangedChannel,
	                                                         LPCGUID EventContext) override;
	virtual HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID NewGroupingParam, LPCGUID EventContext) override;
	virtual HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState NewState) override;

private:
	ULONG mRefCount = 1;
#endif
protected:
	int mCurrentRate;
	int mCurrentNChannels;

public:
	virtual int activate() = 0;
	virtual int deactivate() = 0;
	virtual void start();
	virtual void stop();

	virtual float getVolumeLevel();
	virtual void setVolumeLevel(float volume);
	virtual int getRate() const;
	virtual void setRate(int rate);
	virtual int getNChannels() const;
	virtual void setNChannels(int channels);
	virtual bool isStarted() const {
		return mIsStarted;
	}

	virtual void init(MSSndCard *card, MSFilter *f);
	bool unregister();

	int createAudioClient();
	void destroyAudioClient();
	int restartAudioClient();
	int activate(bool isCapture, void **audioClient);
	int deactivate(IUnknown **audioClient);

#ifdef MS2_WINDOWS_UNIVERSAL
	virtual bool isInstantiated() = 0;
	virtual void setInstantiated(bool instantiated) = 0;
#endif
	int checkDefaultDevice();
	bool isFormatUpdated(); // Check with WASAPI if device format is different from what it is using.
	void updateFormat(bool useBestFormat);
	int tryToUpdateFormat();
	WAVEFORMATPCMEX buildFormat() const;
	bool isCurrentFormatUsable() const;

#if !defined(MS2_WINDOWS_PHONE) && !defined(MS2_WINDOWS_UNIVERSAL)
	void changePolicies(IMMDevice *device);
#endif
};

extern const IID IID_IAudioClient2;
extern const IID IID_IAudioCaptureClient;
extern const IID IID_IAudioRenderClient;
extern const IID IID_ISimpleAudioVolume;

#ifdef MS2_WINDOWS_DESKTOP
extern const CLSID CLSID_MMDeviceEnumerator;
extern const IID IID_IMMDeviceEnumerator;
#endif

extern "C" MSFilterDesc ms_wasapi_read_desc;
extern "C" MSFilterDesc ms_wasapi_write_desc;
extern "C" MSSndCardDesc ms_wasapi_snd_card_desc;
