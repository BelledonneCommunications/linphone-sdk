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

#pragma once

#include <bctoolbox/utils.hh>
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
#include <initguid.h> // Put it at first or there will be a mess with MMDeviceAPI and PKEY_AudioEndpoint* symbols
#include <audiopolicy.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#endif

#ifdef MS2_WINDOWS_UNIVERSAL
#include <ppltasks.h>

using namespace Microsoft::WRL;
using namespace Platform;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Media::Devices;
#endif

#define REPORT_ERROR_NOGOTO(msg, result)                                                                               \
	if (result != S_OK) {                                                                                              \
		ms_error(msg, result);                                                                                         \
		if (result == E_OUTOFMEMORY) ms_error(("mswasapi: " + bctoolbox::Utils::getMemoryReportAsString()).c_str());   \
	}
#define REPORT_ERROR(msg, result)                                                                                      \
	if (result != S_OK) {                                                                                              \
		ms_error(msg, result);                                                                                         \
		if (result == E_OUTOFMEMORY) ms_error(("mswasapi: " + bctoolbox::Utils::getMemoryReportAsString()).c_str());   \
		goto error;                                                                                                    \
	}
#define SAFE_RELEASE(obj)                                                                                              \
	if ((obj) != NULL) {                                                                                               \
		(obj)->Release();                                                                                              \
		(obj) = NULL;                                                                                                  \
	}

typedef struct WasapiSndCard {
	std::vector<wchar_t> *id_vector;
	LPWSTR id;
	bool isDefault;
	AUDIO_STREAM_CATEGORY streamCategory;
} WasapiSndCard;

class MSWasapi
#ifdef MS2_WINDOWS_UNIVERSAL
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, FtmBase, IActivateAudioInterfaceCompletionHandler, IAudioSessionEvents> {
#elif defined(MS2_WINDOWS_PHONE)
	{
#else
    : public IActivateAudioInterfaceCompletionHandler,
      public IAudioSessionEvents {
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
		int mMediaDirection; // 0: Reader, 1: Writer
		std::string mMediaDirectionStr;
		bool mDisableSysFx; // Option to remove audio enhancements mode. This mode can break inputs on some systems.
		std::string mDeviceName;
		bool mIsDefaultDevice = false;
		bool mIsActivated;
		bool mIsStarted;
		bool mScheduleUpdateFormat = false; /* Update format on next msticker loop */
		UINT32 mBufferFrameCount;           /* The buffer size obtained from the wasapi.*/
		MSFilter *mFilter;
		ISimpleAudioVolume *mVolumeController;

		MSWasapi(MSFilter * filter, const std::string &mediaDirectionStr);
		virtual ~MSWasapi();

#ifndef MS2_WINDOWS_PHONE
		// IActivateAudioInterfaceCompletionHandler
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
		virtual ULONG STDMETHODCALLTYPE AddRef(void);
		virtual ULONG STDMETHODCALLTYPE Release(void);
		STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation * operation);

		// IAudioSessionEvents
		virtual HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason);
		virtual HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR NewDisplayName, LPCGUID EventContext);
		virtual HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR NewIconPath, LPCGUID EventContext);
		virtual HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext);
		virtual HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD ChannelCount, float NewChannelVolumeArray[],
		                                                         DWORD ChangedChannel, LPCGUID EventContext);
		virtual HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID NewGroupingParam, LPCGUID EventContext);
		virtual HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState NewState);

	private:
		ULONG m_refCount = 0;
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

		virtual void init(MSSndCard * card, MSFilter * f);

		int createAudioClient();
		void destroyAudioClient();
		int restartAudioClient();

#ifdef MS2_WINDOWS_UNIVERSAL
		virtual bool isInstantiated() = 0;
		virtual void setInstantiated(bool instantiated) = 0;
#endif
		bool isFormatUpdated(); // Check with WASAPI if device format is different from what it is using.
		void updateFormat(bool useBestFormat);
		void tryToUpdateFormat();
		WAVEFORMATPCMEX buildFormat() const;
		bool isCurrentFormatUsable() const;

#if !defined(MS2_WINDOWS_PHONE) && !defined(MS2_WINDOWS_UNIVERSAL)
		void changePolicies(IMMDevice * device);
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
