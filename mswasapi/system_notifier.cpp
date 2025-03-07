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

//-----------------------------------------------------------------------------------------
// clang-format off
#include "utils.h" // Need to be call first because of #include <mmdeviceapi.h> (sockaddr redefinition)
// clang-format on

#include "system_notifier.h"

SystemNotifier::SystemNotifier() {
}

SystemNotifier::~SystemNotifier() {
	unregister();
}

bool SystemNotifier::unregister() {
	bool unregisterDone = false;
	if (mSpEnumerator) {
		mSpEnumerator->UnregisterEndpointNotificationCallback(this);
		SAFE_RELEASE(mSpEnumerator)
		unregisterDone = true;
	}
	return unregisterDone;
}

void SystemNotifier::init(bool followDefaultDevice) {
	mFollowDefaultDevice = followDefaultDevice;
	if (!mSpEnumerator) {
		HRESULT result = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator,
		                                  (void **)&mSpEnumerator);
		REPORT_ERROR("mswasapi: Cannot instantiate enumerator for endpoint notification callbacks [%x]", result)
		result = mSpEnumerator->RegisterEndpointNotificationCallback(this);
		REPORT_ERROR("mswasapi: Cannot register to endpoint notification callbacks [%x]", result)
		ms_message("mswasapi: Endpoint notifications registred");
	}
	return;
error:
	SAFE_RELEASE(mSpEnumerator)
}

bool SystemNotifier::isDeviceStateChanged() const {
	return mDeviceStateChanged;
}

void SystemNotifier::resetDeviceStateChanged() {
	mDeviceStateChanged = false;
}

HRESULT STDMETHODCALLTYPE SystemNotifier::OnDefaultDeviceChanged(EDataFlow flow,
                                                                 ERole role,
                                                                 LPCWSTR pwstrDefaultDeviceId) {
	if (mFollowDefaultDevice && role == eCommunications && (flow == mFlow || flow == eAll)) {
		bool enabled = !!pwstrDefaultDeviceId;
		bool changed = mDefaultDeviceFirst || (enabled != mDefaultDeviceAvailable);
		ms_message("mswasapi: Default %s device %s%s [%i:%i:%x]", (mFlow == eCapture ? "input" : "output"),
		           (changed ? "changed" : "unchanged"), (enabled ? "" : " to NULL"), flow, role, this);
		mDefaultDeviceChanged = changed;
		mDefaultDeviceAvailable = enabled;
		mDefaultDeviceFirst = false;
		if (mDefaultDeviceChanged) mDeviceStateChanged = true;
	}
	return S_OK;
}

// DeviceAdded/Removed are not always call. OnDeviceStateChanged is preferred to be used.
HRESULT STDMETHODCALLTYPE SystemNotifier::OnDeviceAdded(LPCWSTR pwstrDeviceId) {
	return S_OK;
}

HRESULT STDMETHODCALLTYPE SystemNotifier::OnDeviceRemoved(LPCWSTR pwstrDeviceId) {
	return S_OK;
}

// TODO: Optimization hint: store devices id somewhere, follow states and notify only if needed.
HRESULT STDMETHODCALLTYPE SystemNotifier::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) {
	mDeviceStateChanged = true;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE SystemNotifier::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) {
	return S_OK;
}

HRESULT SystemNotifier::QueryInterface(REFIID riid, void **ppvObject) {
	if (riid == IID_IUnknown) *ppvObject = this;
	else {
		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

ULONG SystemNotifier::AddRef(void) {
	return InterlockedIncrement(&mRefCount);
}

ULONG SystemNotifier::Release(void) {
	ULONG count = InterlockedDecrement(&mRefCount);
	if (count == 0) delete this;
	return count;
}
