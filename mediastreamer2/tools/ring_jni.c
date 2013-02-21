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

#include "mediastreamer2/mediastream.h"
#include <jni.h>

extern void ms_andsnd_register_card(JavaVM *jvm);

void Java_org_mediastreamer2_test_Ring_init(JNIEnv*  env,jobject  this){
	ortp_init();
	ortp_set_log_level_mask(ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
	ms_init();
}
void Java_org_mediastreamer2_test_Ring_play(JNIEnv*  env,jobject  this,jstring jfile){
	RingStream *r;
	const char *file;
	MSSndCard *sc;
	const char * card_id=NULL;

	file = (*env)->GetStringUTFChars(env, jfile, NULL);
	sc=ms_snd_card_manager_get_card(ms_snd_card_manager_get(),card_id);
	r=ring_start(file,2000,sc);
	ms_sleep(2);
	ring_stop(r);
    (*env)->ReleaseStringUTFChars(env, jfile, file);

	return ;
}


void Java_org_mediastreamer2_test_Ring_echo(JNIEnv*  env,jobject  this,jint freq){
    MSFilter *reader,*writer;
    MSFilter *resampler=0;
    MSSndCard *card_capture;
    MSSndCard *card_playback;
    MSTicker *ticker;
    int read_rate = freq;
    int write_rate = freq;
	const char * card_id="ANDROID SND: Android Sound card";
	card_capture = ms_snd_card_manager_get_card(ms_snd_card_manager_get(),card_id);
	card_playback = ms_snd_card_manager_get_card(ms_snd_card_manager_get(),card_id);
	if (card_playback==NULL || card_capture==NULL){
		ms_error("No card.");
		return ;
	}
	reader=ms_snd_card_create_reader(card_capture);
	writer=ms_snd_card_create_writer(card_playback);

	if (read_rate !=write_rate) {
		resampler = ms_filter_new(MS_RESAMPLE_ID);
		ms_filter_call_method(resampler,MS_FILTER_SET_SAMPLE_RATE,&read_rate);
		ms_filter_call_method(resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&write_rate);
	}
	ms_filter_call_method (reader, MS_FILTER_SET_SAMPLE_RATE,
			&read_rate);
	ms_filter_call_method (writer, MS_FILTER_SET_SAMPLE_RATE,
			&write_rate);

	ticker=ms_ticker_new();
	if (resampler) {
		ms_filter_link(reader,0,resampler,0);
		ms_filter_link(resampler,0,writer,0);
	} else {
		ms_filter_link(reader,0,writer,0);
	}
	ms_ticker_attach(ticker,reader);
	while(1)
		ms_sleep(1);
	ms_ticker_detach(ticker,reader);
	ms_ticker_destroy(ticker);
	if (resampler) {
		ms_filter_unlink(reader,0,resampler,0);
		ms_filter_unlink(resampler,0,writer,0);
		ms_filter_destroy(resampler);
	} else {
		ms_filter_unlink(reader,0,writer,0);
	}
	ms_filter_destroy(reader);
	ms_filter_destroy(writer);

}

JNIEXPORT jint JNICALL  JNI_OnLoad(JavaVM *ajvm, void *reserved)
{
#ifdef ANDROID
	ms_set_jvm(ajvm);
#endif /*ANDROID*/
	return JNI_VERSION_1_2;
}
