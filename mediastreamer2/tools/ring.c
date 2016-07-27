/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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


#include "mediastreamer2/mediastream.h"

int main(int argc, char *argv[]){
	RingStream *r;
	const char *file;
	MSSndCard *sc;
	const char * card_id=NULL;
	MSFactory *factory;

	ortp_init();
	ortp_set_log_level_mask(ORTP_LOG_DOMAIN, ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
	
	
	factory = ms_factory_new_with_voip();
	
	if (argc>1){
		file=argv[1];
	}else file="/usr/share/sounds/linphone/rings/oldphone.wav";
	if (argc>2){
		card_id=argv[2];
	}

	sc=ms_snd_card_manager_get_card(ms_factory_get_snd_card_manager(factory),card_id);
#ifdef __linux
	if (sc==NULL)
	  sc = ms_alsa_card_new_custom(card_id, card_id);
#endif

	r=ring_start(factory, file,2000,sc);
	ms_sleep(10);
	ring_stop(r);
	
	ms_factory_destroy(factory);
	return 0;
}
