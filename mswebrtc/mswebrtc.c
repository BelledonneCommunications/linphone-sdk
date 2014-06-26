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
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef BUILD_ISAC
#include "isac_constants.h"
#include "isacfix.h"
#endif

#include "signal_processing_library.h"

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mscodecutils.h"


#ifdef BUILD_ISAC
extern MSFilterDesc ms_isac_dec_desc;
extern MSFilterDesc ms_isac_enc_desc;
#endif
#ifdef BUILD_AEC
extern MSFilterDesc ms_webrtc_aec_desc;
#endif

#ifndef VERSION
#define VERSION "debug"
#endif

#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) type
#endif

MS_PLUGIN_DECLARE ( void ) libmswebrtc_init() {
#ifdef BUILD_ISAC
	char isac_version[64];
	isac_version[0] = 0;
#endif

	WebRtcSpl_Init();

#ifdef BUILD_ISAC
	WebRtcIsacfix_version(isac_version);
	ms_filter_register(&ms_isac_enc_desc);
	ms_filter_register(&ms_isac_dec_desc);
#endif
#ifdef BUILD_AEC
	ms_filter_register(&ms_webrtc_aec_desc);
#endif

	ms_message("libmswebrtc " VERSION " plugin loaded"
#ifdef BUILD_ISAC
	", iSAC codec version %s", isac_version
#endif
	);
}


