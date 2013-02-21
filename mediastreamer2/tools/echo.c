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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msticker.h"
#include <ortp/ortp.h>

#ifndef _WIN32_WCE
#include <signal.h>
#elif defined(_MSC_VER)
#define main _tmain
#endif

static int run=1;

static void stop(int signum){
	run=0;
}

static void print_usage(void){
	printf("echo\t\t[--card <sound card name>]\n"
	       "\t\t[--capt-card <sound card name>]\n"
	       "\t\t[--play-card <sound card name>]\n");
	exit(-1);
}

int main(int argc, char *argv[]){
	MSFilter *f1,*f2;
	MSSndCard *card_capture;
	MSSndCard *card_playback;
	MSTicker *ticker;
	char *capt_card=NULL,*play_card=NULL;
	int rate = 8000;
	int i;
#ifdef __linux
	const char *alsadev=NULL;
#endif

	ortp_init();
	ortp_set_log_level_mask(ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
	ms_init();

#ifndef _WIN32_WCE
	signal(SIGINT,stop);
#endif

#ifdef __linux
	alsadev=getenv("MS2_ALSADEV");
	if (alsadev!=NULL){
		ms_snd_card_manager_add_card(ms_snd_card_manager_get(),
			ms_alsa_card_new_custom (alsadev,alsadev));
	}
#endif
	
	for(i=1;i<argc;++i){
		if (strcmp(argv[i],"--help")==0){
			print_usage();
		}else if (strcmp(argv[i],"--card")==0){
			i++;
			capt_card=play_card=argv[i];
		}else if (strcmp(argv[i],"--capt-card")==0){
			i++;
			capt_card=argv[i];
		}else if (strcmp(argv[i],"--play-card")==0){
			i++;
			play_card=argv[i];
		}
	}

	if (capt_card)
		card_capture = ms_snd_card_manager_get_card(ms_snd_card_manager_get(),capt_card);
	else card_capture = ms_snd_card_manager_get_default_capture_card(ms_snd_card_manager_get());
	if (play_card)
		card_playback = ms_snd_card_manager_get_card(ms_snd_card_manager_get(),play_card);
	else card_playback = ms_snd_card_manager_get_default_playback_card(ms_snd_card_manager_get());
	
	if (card_playback==NULL || card_capture==NULL){
		ms_error("No card.");
		return -1;
	}
	f1=ms_snd_card_create_reader(card_capture);
	f2=ms_snd_card_create_writer(card_playback);

	ms_filter_call_method (f1, MS_FILTER_SET_SAMPLE_RATE,
		&rate);
	ms_filter_call_method (f2, MS_FILTER_SET_SAMPLE_RATE,
		&rate);

	ticker=ms_ticker_new();
	ms_filter_link(f1,0,f2,0);
	ms_ticker_attach(ticker,f1);
#ifndef _WIN32_WCE
	while(run)
		ms_sleep(1);
#else
	ms_sleep(5);
#endif
	ms_ticker_detach(ticker,f1);
	ms_ticker_destroy(ticker);
	ms_filter_unlink(f1,0,f2,0);
	ms_filter_destroy(f1);
	ms_filter_destroy(f2);
	return 0;
}
