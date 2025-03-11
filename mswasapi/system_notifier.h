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

// Only supported for Desktop Win32
#if !defined(MS2_WINDOWS_UNIVERSAL) && !defined(MS2_WINDOWS_PHONE)
#include <mmdeviceapi.h>

class SystemNotifier : public IMMNotificationClient {
public:
	SystemNotifier();
	virtual ~SystemNotifier();
	bool unregister();

	void init(bool followDefaultDevice = false);
	bool isDeviceStateChanged() const;
	void resetDeviceStateChanged();

	// IMMNotificationClient Handlers
	virtual HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow,
	                                                         ERole role,
	                                                         LPCWSTR pwstrDefaultDeviceId) override;
	virtual HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
	virtual HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
	virtual HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
	virtual HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;
	virtual ULONG STDMETHODCALLTYPE AddRef(void) override;
	virtual ULONG STDMETHODCALLTYPE Release(void) override;

protected:
	bool mDeviceStateChanged = false;
	bool mDefaultDeviceAvailable = true;
	bool mDefaultDeviceChanged = false;
	int mDefaultDeviceFirst = true; // Needed to send a first change without having to know the start state.
	bool mFollowDefaultDevice = false;
	EDataFlow mFlow = eAll;

private:
	ULONG mRefCount = 1;
	IMMDeviceEnumerator *mSpEnumerator = NULL;
};
#endif
