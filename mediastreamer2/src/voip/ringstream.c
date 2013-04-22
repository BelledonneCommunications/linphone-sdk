/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2013 Belledonne Communications, Grenoble

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
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msfileplayer.h"


RingStream * ring_start(const char *file, int interval, MSSndCard *sndcard){
   return ring_start_with_cb(file,interval,sndcard,NULL,NULL);
}

RingStream * ring_start_with_cb(const char *file,int interval,MSSndCard *sndcard, MSFilterNotifyFunc func,void * user_data)
{
	RingStream *stream;
	int srcchannels=1, dstchannels=1;
	int srcrate,dstrate;
	MSConnectionHelper h;
	MSTickerParams params={0};

	stream=(RingStream *)ms_new0(RingStream,1);
	stream->source=ms_filter_new(MS_FILE_PLAYER_ID);
	if (file)
		ms_filter_call_method(stream->source,MS_FILE_PLAYER_OPEN,(void*)file);

	ms_filter_call_method(stream->source,MS_FILE_PLAYER_LOOP,&interval);
	ms_filter_call_method_noarg(stream->source,MS_FILE_PLAYER_START);
	if (func!=NULL)
		ms_filter_set_notify_callback(stream->source,func,user_data);
	stream->gendtmf=ms_filter_new(MS_DTMF_GEN_ID);

	stream->sndwrite=ms_snd_card_create_writer(sndcard);
	ms_filter_call_method(stream->source,MS_FILTER_GET_SAMPLE_RATE,&srcrate);
	ms_filter_call_method(stream->gendtmf,MS_FILTER_SET_SAMPLE_RATE,&srcrate);
	ms_filter_call_method(stream->sndwrite,MS_FILTER_SET_SAMPLE_RATE,&srcrate);
	ms_filter_call_method(stream->sndwrite,MS_FILTER_GET_SAMPLE_RATE,&dstrate);
	if (srcrate!=dstrate){
		stream->write_resampler=ms_filter_new(MS_RESAMPLE_ID);
		ms_filter_call_method(stream->write_resampler,MS_FILTER_SET_SAMPLE_RATE,&srcrate);
		ms_filter_call_method(stream->write_resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&dstrate);
		ms_message("configuring resampler from rate [%i] to rate [%i]", srcrate,dstrate);
	}
	ms_filter_call_method(stream->source,MS_FILTER_GET_NCHANNELS,&srcchannels);
	ms_filter_call_method(stream->gendtmf,MS_FILTER_SET_NCHANNELS,&srcchannels);
	ms_filter_call_method(stream->sndwrite,MS_FILTER_SET_NCHANNELS,&srcchannels);
	ms_filter_call_method(stream->sndwrite,MS_FILTER_GET_NCHANNELS,&dstchannels);
	if (srcchannels != dstchannels) {
		if (!stream->write_resampler) {
			stream->write_resampler=ms_filter_new(MS_RESAMPLE_ID);
		}
		ms_filter_call_method(stream->write_resampler,MS_FILTER_SET_NCHANNELS,&srcchannels);
		ms_filter_call_method(stream->write_resampler,MS_FILTER_SET_OUTPUT_NCHANNELS,&dstchannels);
		ms_message("configuring resampler from channels [%i] to channels [%i]", srcchannels, dstchannels);
	}
	

	params.name="Ring MSTicker";
	params.prio=MS_TICKER_PRIO_HIGH;
	stream->ticker=ms_ticker_new_with_params(&params);

	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h,stream->source,-1,0);
	ms_connection_helper_link(&h,stream->gendtmf,0,0);
	if (stream->write_resampler)
		ms_connection_helper_link(&h,stream->write_resampler,0,0);
	ms_connection_helper_link(&h,stream->sndwrite,0,-1);
	ms_ticker_attach(stream->ticker,stream->source);

	return stream;
}

void ring_play_dtmf(RingStream *stream, char dtmf, int duration_ms){
	if (duration_ms>0)
		ms_filter_call_method(stream->gendtmf, MS_DTMF_GEN_PLAY, &dtmf);
	else ms_filter_call_method(stream->gendtmf, MS_DTMF_GEN_START, &dtmf);
}

void ring_stop_dtmf(RingStream *stream){
	ms_filter_call_method_noarg(stream->gendtmf, MS_DTMF_GEN_STOP);
}

void ring_stop(RingStream *stream){
	MSConnectionHelper h;
	ms_ticker_detach(stream->ticker,stream->source);

	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h,stream->source,-1,0);
	ms_connection_helper_unlink(&h,stream->gendtmf,0,0);
	if (stream->write_resampler)
		ms_connection_helper_unlink(&h,stream->write_resampler,0,0);
	ms_connection_helper_unlink(&h,stream->sndwrite,0,-1);

	ms_ticker_destroy(stream->ticker);
	ms_filter_destroy(stream->source);
	ms_filter_destroy(stream->gendtmf);
	ms_filter_destroy(stream->sndwrite);
	ms_free(stream);
#ifdef _WIN32_WCE
	ms_warning("Sleeping a bit after closing the audio device...");
	ms_sleep(1);
#endif
}
