/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2011  Belledonne Communications SARL.
Author: Simon Morlat (simon.morlat@linphone.org)

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
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mstonedetector.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msticker.h"

static void tone_detected_cb(void *data, MSFilter *f, unsigned int event_id, MSToneDetectorEvent *ev){
	ms_message("Tone detected  at time %u",(unsigned int)ev->tone_start_time);
}

static void tone_sent_cb(void *data, MSFilter *f, unsigned int event_id, MSDtmfGenEvent *ev){
	ms_message("Tone sent at time %u",(unsigned int)ev->tone_start_time);
}

int main(int argc, char *argv[]){
	MSFilter *src, *gen, *det, *rec;
	MSTicker *ticker;
	MSFactory *factory ;
	//ms_base_init();

	factory = ms_factory_new();
	ms_factory_init_voip(factory);
	ms_factory_init_plugins(factory);

	ortp_set_log_level_mask (ORTP_LOG_DOMAIN, ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);

	src=ms_factory_create_filter(factory,MS_FILE_PLAYER_ID);
	rec=ms_factory_create_filter(factory,MS_FILE_REC_ID);
	gen=ms_factory_create_filter(factory,MS_DTMF_GEN_ID);
	det=ms_factory_create_filter(factory,MS_TONE_DETECTOR_ID);

	ms_filter_link(src,0,gen,0);
	ms_filter_link(gen,0,det,0);
	//ms_filter_link(gen,0,rec,0);
	ms_filter_link(det,0,rec,0);

	ticker=ms_ticker_new();

	ms_ticker_attach(ticker,src);

	ms_filter_call_method(rec,MS_FILE_REC_OPEN,"/tmp/output.wav");
	ms_filter_call_method_noarg(rec,MS_FILE_REC_START);
	{
		/*generate and detect the tones*/
		MSDtmfGenCustomTone tone;
		MSToneDetectorDef expected_tone;
		char dtmf='*';

		memset(&tone,0,sizeof(tone));
		memset(&expected_tone,0,sizeof(expected_tone));

		tone.frequencies[0]=2000;
		tone.duration=400;
		tone.amplitude=0.6f;

		expected_tone.frequency=2000;
		expected_tone.min_duration=200;
		expected_tone.min_amplitude=0.5;
		ms_filter_add_notify_callback(det,(MSFilterNotifyFunc)tone_detected_cb,NULL,TRUE);
		ms_filter_add_notify_callback(gen,(MSFilterNotifyFunc)tone_sent_cb,NULL,TRUE);

		ms_filter_call_method(det,MS_TONE_DETECTOR_ADD_SCAN,&expected_tone);

		ms_filter_call_method(gen,MS_DTMF_GEN_PLAY,&dtmf);
		ms_sleep(1);

		ms_filter_call_method(gen,MS_DTMF_GEN_PLAY_CUSTOM,&tone);
		ms_sleep(1);
		ms_filter_call_method(gen,MS_DTMF_GEN_PLAY_CUSTOM,&tone);
		ms_sleep(1);
		ms_filter_call_method(gen,MS_DTMF_GEN_PLAY_CUSTOM,&tone);
		ms_sleep(1);
		tone.frequencies[0]=1500;
		tone.amplitude=1.0f;
		ms_filter_call_method(gen,MS_DTMF_GEN_PLAY_CUSTOM,&tone);
		ms_sleep(1);
	}

	ms_filter_call_method_noarg(rec,MS_FILE_REC_CLOSE);
	ms_ticker_detach(ticker,src);

	ms_filter_unlink(src,0,gen,0);
	ms_filter_unlink(gen,0,det,0);
	//ms_filter_unlink(gen,0,rec,0);
	ms_filter_unlink(det,0,rec,0);

	ms_ticker_destroy(ticker);

	ms_filter_destroy(src);
	ms_filter_destroy(gen);
	ms_filter_destroy(det);
	ms_filter_destroy(rec);

	ms_factory_destroy(factory);
	//ms_base_exit();
	return 0;
}

