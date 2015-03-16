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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#pragma once

#if !defined(WINAPI_FAMILY_PARTITION) || !defined(WINAPI_PARTITION_PHONE)
/* Old version of Visual Studio, no support of Windows Phone. */
#define BUILD_FOR_WINDOWS_PHONE 0
#else
#define BUILD_FOR_WINDOWS_PHONE WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE)
#endif

#include <vector>
#include <objbase.h>
#include <audioclient.h>
#if BUILD_FOR_WINDOWS_PHONE
#include <phoneaudioclient.h>
#else
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#endif

#define REPORT_ERROR(msg, result) \
	if (result != S_OK) { \
		ms_error(msg, result); \
		goto error; \
		}
#define SAFE_RELEASE(obj) \
	if ((obj) != NULL) { \
		(obj)->Release(); \
		(obj) = NULL; \
		}


typedef struct WasapiSndCard {
	std::vector<wchar_t> *id_vector;
	LPWSTR id;
} WasapiSndCard;


extern const IID IID_IAudioClient2;
extern const IID IID_IAudioCaptureClient;
extern const IID IID_IAudioRenderClient;

#if !BUILD_FOR_WINDOWS_PHONE
extern const CLSID CLSID_MMDeviceEnumerator;
extern const IID IID_IMMDeviceEnumerator;
#endif
