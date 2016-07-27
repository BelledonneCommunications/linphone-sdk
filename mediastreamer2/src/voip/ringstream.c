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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/



#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msfileplayer.h"
#include "private.h"


static void ring_player_event_handler(void *ud, MSFilter *f, unsigned int evid, void *arg){
	RingStream *stream=(RingStream*)ud;

	if (evid==MS_FILTER_OUTPUT_FMT_CHANGED){
		MSPinFormat pinfmt={0};
		ms_filter_call_method(stream->source, MS_FILTER_GET_OUTPUT_FMT, &pinfmt);
		if (pinfmt.fmt == NULL){
			pinfmt.pin = 1;
			ms_filter_call_method(stream->source, MS_FILTER_GET_OUTPUT_FMT, &pinfmt);
		}

		if (stream->write_resampler){
			ms_message("Configuring resampler input with rate=[%i], nchannels=[%i]",pinfmt.fmt->rate, pinfmt.fmt->nchannels);
			ms_filter_call_method(stream->write_resampler,MS_FILTER_SET_NCHANNELS,(void*)&pinfmt.fmt->nchannels);
			ms_filter_call_method(stream->write_resampler,MS_FILTER_SET_SAMPLE_RATE,(void*)&pinfmt.fmt->rate);
		}
		ms_filter_call_method(stream->gendtmf,MS_FILTER_SET_SAMPLE_RATE, (void*)&pinfmt.fmt->rate);
		ms_filter_call_method(stream->gendtmf,MS_FILTER_SET_NCHANNELS, (void*)&pinfmt.fmt->nchannels);
	}
}

RingStream * ring_start(MSFactory *factory, const char *file, int interval, MSSndCard *sndcard){
   return ring_start_with_cb(factory, file,interval,sndcard,NULL,NULL);
}

RingStream * ring_start_with_cb(MSFactory* factory, const char *file, int interval, MSSndCard *sndcard, MSFilterNotifyFunc func,void * user_data )
{
	RingStream *stream;
	int srcchannels=1, dstchannels=1;
	int srcrate,dstrate;
	MSConnectionHelper h;
	MSTickerParams params={0};
	MSPinFormat pinfmt={0};

	stream=(RingStream *)ms_new0(RingStream,1);
	if (file) {
		stream->source=_ms_create_av_player(file,factory);
		if (stream->source == NULL){
			ms_error("ring_start_with_cb(): could not create player for playing '%s'", file);
			ms_free(stream);
			return NULL;
		}
	} else {
		/*create dummy source*/
		stream->source=ms_factory_create_filter(factory, MS_FILE_PLAYER_ID);
	}
	ms_filter_add_notify_callback(stream->source,ring_player_event_handler,stream,TRUE);
	if (func!=NULL)
		ms_filter_add_notify_callback(stream->source,func,user_data,FALSE);
	stream->gendtmf=ms_factory_create_filter(factory, MS_DTMF_GEN_ID);
	stream->sndwrite=ms_snd_card_create_writer(sndcard);
	stream->write_resampler=ms_factory_create_filter(factory, MS_RESAMPLE_ID);

	if (file){
		/*in we failed to open the file, we must release the stream*/
		if (ms_filter_call_method(stream->source,MS_PLAYER_OPEN,(void*)file) != 0) {
			ring_stop(stream);
			return NULL;
		}
		ms_filter_call_method(stream->source,MS_PLAYER_SET_LOOP,&interval);
		ms_filter_call_method_noarg(stream->source,MS_PLAYER_START);
	}

	/*configure sound output filter*/
	ms_filter_call_method(stream->source, MS_FILTER_GET_OUTPUT_FMT, &pinfmt);
	if (pinfmt.fmt == NULL){
		pinfmt.pin = 1;
		ms_filter_call_method(stream->source, MS_FILTER_GET_OUTPUT_FMT, &pinfmt);
		if (pinfmt.fmt == NULL){
			/*probably no file is being played, assume pcm*/
			pinfmt.fmt = ms_factory_get_audio_format(factory, "pcm", 8000, 1, NULL);
		}
	}
	srcrate = pinfmt.fmt->rate;
	srcchannels = pinfmt.fmt->nchannels;

	ms_filter_call_method(stream->sndwrite,MS_FILTER_SET_SAMPLE_RATE,&srcrate);
	ms_filter_call_method(stream->sndwrite,MS_FILTER_GET_SAMPLE_RATE,&dstrate);
	ms_filter_call_method(stream->sndwrite,MS_FILTER_SET_NCHANNELS,&srcchannels);
	ms_filter_call_method(stream->sndwrite,MS_FILTER_GET_NCHANNELS,&dstchannels);

	/*eventually create a decoder*/
	if (strcasecmp(pinfmt.fmt->encoding, "pcm") != 0){
		stream->decoder = ms_factory_create_decoder(factory, pinfmt.fmt->encoding);
		if (!stream->decoder){
			ms_error("RingStream: could not create decoder for '%s'", pinfmt.fmt->encoding);
			ring_stop(stream);
			return NULL;
		}
	}

	/*configure output of resampler*/
	if (stream->write_resampler){
		ms_filter_call_method(stream->write_resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&dstrate);
		ms_filter_call_method(stream->write_resampler,MS_FILTER_SET_OUTPUT_NCHANNELS,&dstchannels);

		/*the input of the resampler, as well as dtmf generator are configured within the ring_player_event_handler()
		 * callback triggered during the open of the file player*/
		ms_message("configuring resampler output to rate=[%i], nchannels=[%i]",dstrate,dstchannels);
	}

	params.name="Ring MSTicker";
	params.prio=MS_TICKER_PRIO_HIGH;
	stream->ticker=ms_ticker_new_with_params(&params);

	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h,stream->source, -1, pinfmt.pin);
	stream->srcpin = pinfmt.pin;
	if (stream->decoder){
		ms_filter_call_method(stream->decoder, MS_FILTER_SET_NCHANNELS, &srcchannels);
		ms_connection_helper_link(&h, stream->decoder, 0, 0);
	}
	ms_connection_helper_link(&h,stream->gendtmf,0,0);
	if (stream->write_resampler)
		ms_connection_helper_link(&h,stream->write_resampler,0,0);
	ms_connection_helper_link(&h, stream->sndwrite, 0, -1);
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

	if (stream->ticker){
		ms_ticker_detach(stream->ticker,stream->source);

		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h,stream->source,-1, stream->srcpin);
		if (stream->decoder){
			ms_connection_helper_unlink(&h,stream->decoder, 0, 0);
		}
		ms_connection_helper_unlink(&h,stream->gendtmf,0,0);
		if (stream->write_resampler)
			ms_connection_helper_unlink(&h,stream->write_resampler,0,0);
		ms_connection_helper_unlink(&h,stream->sndwrite,0,-1);
		ms_ticker_destroy(stream->ticker);
	}
	if (stream->source) ms_filter_destroy(stream->source);
	if (stream->gendtmf) ms_filter_destroy(stream->gendtmf);
	if (stream->sndwrite) ms_filter_destroy(stream->sndwrite);
	if (stream->decoder) ms_filter_destroy(stream->decoder);
	if (stream->write_resampler)
		ms_filter_destroy(stream->write_resampler);
	ms_free(stream);
}
