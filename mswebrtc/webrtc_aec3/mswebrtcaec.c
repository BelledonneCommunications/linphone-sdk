/*
 mswebrtc.c
 Copyright (C) 2013 Belledonne Communications, Grenoble, France

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mediastreamer2/msfilter.h"
// #include "signal_processing_library.h"

// #include "mediastreamer2/mscodecutils.h"

#ifdef BUILD_AEC
extern MSFilterDesc ms_webrtc_aec_desc;
#endif
#ifdef BUILD_VAD
extern MSFilterDesc ms_webrtc_vad_desc;
#endif

#ifndef VERSION
#define VERSION "debug"
#endif

#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) type
#endif

MS_PLUGIN_DECLARE(void) libmswebrtcaec_init(MSFactory *factory) {

#ifdef BUILD_AEC
	ms_factory_register_filter(factory, &ms_webrtc_aec_desc);
#endif
#ifdef BUILD_VAD
	ms_factory_register_filter(factory, &ms_webrtc_vad_desc);
#endif

	ms_message("libmswebrtcaec " VERSION " plugin loaded");
}
