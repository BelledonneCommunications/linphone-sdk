/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Yann DIORCET
Belledonne Communications SARL, All rights reserved.
yann.diorcet@belledonne-communications.com

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

#include <mediastreamer2/msfilter.h>

#ifdef HAVE_AMRNB
extern MSFilterDesc amrnb_dec_desc;
extern MSFilterDesc amrnb_enc_desc;
#endif

#ifdef HAVE_AMRWB
extern MSFilterDesc amrwb_dec_desc;
extern MSFilterDesc amrwb_enc_desc;
#endif

#ifdef USE_ANDROID_AMR
int opencore_amr_wrapper_init(const char **missing);
#endif

void libmsamr_init(){
#ifdef HAVE_AMRNB
#ifdef USE_ANDROID_AMR
	const char *missing=NULL;
	if (opencore_amr_wrapper_init(&missing)==-1){
		ms_error("Could not find AMR codec of android, no AMR support possible (missing symbol=%s)",missing);
		return;
	}
#endif
        ms_filter_register(&amrnb_dec_desc);
	ms_filter_register(&amrnb_enc_desc);
#endif
#ifdef HAVE_AMRWB
        ms_filter_register(&amrwb_dec_desc);
        ms_filter_register(&amrwb_enc_desc);
#endif
        
	ms_message("libmsamr " VERSION " plugin loaded");
}
