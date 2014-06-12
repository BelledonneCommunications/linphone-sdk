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

#include "isacfix.h"
#include "signal_processing_library.h"

#include "isac_constants.h"

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mscodecutils.h"


extern MSFilterDesc ms_isac_dec_desc;
extern MSFilterDesc ms_isac_enc_desc;

#ifndef VERSION
#define VERSION "debug"
#endif

#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) type
#endif

MS_PLUGIN_DECLARE ( void ) libmswebrtc_init() {
	char isac_version[64];
	isac_version[0] = 0;

	WebRtcSpl_Init();
	WebRtcIsacfix_version(isac_version);

	ms_filter_register ( &ms_isac_enc_desc );
	ms_filter_register ( &ms_isac_dec_desc );

	ms_message ( " libmswebrtc " VERSION " plugin loaded, iSAC codec version %s", isac_version );
}


