/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <bctoolbox/defs.h>

#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msfileplayer.h"
#include "private.h"

static void ring_player_event_handler(void *ud, BCTBX_UNUSED(MSFilter *f), unsigned int evid, BCTBX_UNUSED(void *arg)) {
	RingStream *stream = (RingStream *)ud;

	if (evid == MS_FILTER_OUTPUT_FMT_CHANGED) {
		MSPinFormat pinfmt = {0};
		ms_filter_call_method(stream->source, MS_FILTER_GET_OUTPUT_FMT, &pinfmt);
		if (pinfmt.fmt == NULL) {
			pinfmt.pin = 1;
			ms_filter_call_method(stream->source, MS_FILTER_GET_OUTPUT_FMT, &pinfmt);
		}

		if (stream->write_resampler) {
			ms_message("Configuring resampler input with rate=[%i], nchannels=[%i]", pinfmt.fmt->rate,
			           pinfmt.fmt->nchannels);
			ms_filter_call_method(stream->write_resampler, MS_FILTER_SET_NCHANNELS, (void *)&pinfmt.fmt->nchannels);
			ms_filter_call_method(stream->write_resampler, MS_FILTER_SET_SAMPLE_RATE, (void *)&pinfmt.fmt->rate);
		}
		ms_filter_call_method(stream->gendtmf, MS_FILTER_SET_SAMPLE_RATE, (void *)&pinfmt.fmt->rate);
		ms_filter_call_method(stream->gendtmf, MS_FILTER_SET_NCHANNELS, (void *)&pinfmt.fmt->nchannels);
	}
}

RingStream *ring_start(MSFactory *factory, const char *file, const int interval, const bctbx_list_t *snd_cards) {
	return ring_start_with_cb(factory, file, interval, snd_cards, NULL, NULL);
}

static void write_device_event_handler(void *user_data,
                                       BCTBX_UNUSED(MSFilter *f),
                                       const unsigned int event,
                                       BCTBX_UNUSED(void *eventdata)) {
	if (event == MS_FILTER_OUTPUT_FMT_CHANGED) {
		RingStream *stream = (RingStream *)user_data;
		int playbackRate, playbackChannels;

		for (int i = 0; (i < MS_RING_STREAM_MAX_SND_CARDS); i++) {
			if (stream->sndwrite[i] == NULL) break;

			ms_filter_call_method(stream->sndwrite[i], MS_FILTER_GET_SAMPLE_RATE, &playbackRate);
			ms_filter_call_method(stream->sndwrite[i], MS_FILTER_GET_NCHANNELS, &playbackChannels);
		}

		ms_filter_call_method(stream->write_resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &playbackRate);
		ms_filter_call_method(stream->write_resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &playbackChannels);
		ms_message("reconfiguring resampler output to rate=[%i], nchannels=[%i]", playbackRate, playbackChannels);
	}
}

RingStream *ring_start_with_cb(MSFactory *factory,
                               const char *file,
                               int interval,
                               const bctbx_list_t *snd_cards,
                               const MSFilterNotifyFunc func,
                               void *user_data) {
	int srcchannels = 1, dstchannels = 1;
	int srcrate, dstrate;
	MSConnectionHelper h;
	MSTickerParams params = {0};
	MSPinFormat pinfmt = {0};
	int i = 0;
	RingStream *stream = ms_new0(RingStream, 1);

	for (const bctbx_list_t *item = snd_cards; (i < MS_RING_STREAM_MAX_SND_CARDS) && (item != NULL);
	     item = bctbx_list_next(item), i++) {
		stream->card[i] = ms_snd_card_ref(bctbx_list_get_data(item));
	}
	if (file) {
		stream->source = _ms_create_av_player(file, factory);
		if (stream->source == NULL) {
			ms_error("ring_start_with_cb(): could not create player for playing '%s'", file);
			ms_free(stream);
			return NULL;
		}
	} else {
		/*create dummy source*/
		stream->source = ms_factory_create_filter(factory, MS_FILE_PLAYER_ID);
	}
	ms_filter_add_notify_callback(stream->source, ring_player_event_handler, stream, TRUE);
	if (func != NULL) ms_filter_add_notify_callback(stream->source, func, user_data, FALSE);
	stream->gendtmf = ms_factory_create_filter(factory, MS_DTMF_GEN_ID);

	if (snd_cards) {
		i = 0;
		for (const bctbx_list_t *item = snd_cards; (i < MS_RING_STREAM_MAX_SND_CARDS) && (item != NULL);
		     item = bctbx_list_next(item), i++) {
			MSSndCard *snd_card = bctbx_list_get_data(item);
			stream->sndwrite[i] = (snd_card != NULL) ? ms_snd_card_create_writer(snd_card)
			                                         : ms_factory_create_filter(factory, MS_VOID_SINK_ID);
			// sndwrite Callback with TRUE because RingStream doesn't replumb graph so updates need to be done without
			// restarting it.
			ms_filter_add_notify_callback(stream->sndwrite[i], write_device_event_handler, stream, TRUE);
		}
	} else {
		stream->sndwrite[0] = ms_factory_create_filter(factory, MS_VOID_SINK_ID);
		ms_filter_add_notify_callback(stream->sndwrite[0], write_device_event_handler, stream, TRUE);
	}
	stream->sndwrite_tee = ms_factory_create_filter(factory, MS_TEE_ID);

	stream->write_resampler = ms_factory_create_filter(factory, MS_RESAMPLE_ID);

	if (file) {
		/*in we failed to open the file, we must release the stream*/
		if (ms_filter_call_method(stream->source, MS_PLAYER_OPEN, (void *)file) != 0) {
			ring_stop(stream);
			return NULL;
		}
		ms_filter_call_method(stream->source, MS_PLAYER_SET_LOOP, &interval);
		ms_filter_call_method_noarg(stream->source, MS_PLAYER_START);
	}

	/*configure sound output filter*/
	ms_filter_call_method(stream->source, MS_FILTER_GET_OUTPUT_FMT, &pinfmt);
	if (pinfmt.fmt == NULL) {
		pinfmt.pin = 1;
		ms_filter_call_method(stream->source, MS_FILTER_GET_OUTPUT_FMT, &pinfmt);
		if (pinfmt.fmt == NULL) {
			/*probably no file is being played, assume pcm*/
			pinfmt.fmt = ms_factory_get_audio_format(factory, "pcm", 8000, 1, NULL);
		}
	}
	dstrate = srcrate = pinfmt.fmt->rate;
	dstchannels = srcchannels = pinfmt.fmt->nchannels;

	for (i = 0; (i < MS_RING_STREAM_MAX_SND_CARDS); i++) {
		if (!stream->sndwrite[i]) break;
		ms_filter_call_method(stream->sndwrite[i], MS_FILTER_SET_SAMPLE_RATE, &srcrate);
		ms_filter_call_method(stream->sndwrite[i], MS_FILTER_GET_SAMPLE_RATE, &dstrate);
		ms_filter_call_method(stream->sndwrite[i], MS_FILTER_SET_NCHANNELS, &srcchannels);
		ms_filter_call_method(stream->sndwrite[i], MS_FILTER_GET_NCHANNELS, &dstchannels);
	}

	/*eventually create a decoder*/
	if (strcasecmp(pinfmt.fmt->encoding, "pcm") != 0) {
		stream->decoder = ms_factory_create_decoder(factory, pinfmt.fmt->encoding);
		if (!stream->decoder) {
			ms_error("RingStream: could not create decoder for '%s'", pinfmt.fmt->encoding);
			ring_stop(stream);
			return NULL;
		}
	}

	/*configure output of resampler*/
	if (stream->write_resampler) {
		ms_filter_call_method(stream->write_resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &dstrate);
		ms_filter_call_method(stream->write_resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &dstchannels);

		/*the input of the resampler, as well as dtmf generator are configured within the ring_player_event_handler()
		 * callback triggered during the open of the file player*/
		ms_message("configuring resampler output to rate=[%i], nchannels=[%i]", dstrate, dstchannels);
	}

	params.name = "Ring MSTicker";
	params.prio = MS_TICKER_PRIO_HIGH;
	stream->ticker = ms_ticker_new_with_params(&params);

	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, stream->source, -1, pinfmt.pin);
	stream->srcpin = pinfmt.pin;
	if (stream->decoder) {
		ms_filter_call_method(stream->decoder, MS_FILTER_SET_NCHANNELS, &srcchannels);
		ms_connection_helper_link(&h, stream->decoder, 0, 0);
	}
	ms_connection_helper_link(&h, stream->gendtmf, 0, 0);
	if (stream->write_resampler) ms_connection_helper_link(&h, stream->write_resampler, 0, 0);
	ms_connection_helper_link(&h, stream->sndwrite_tee, 0, 0);
	for (i = 0; i < MS_RING_STREAM_MAX_SND_CARDS; i++) {
		if (!stream->sndwrite[i]) break;
		ms_filter_link(stream->sndwrite_tee, i + 1, stream->sndwrite[i], 0);
	}
	ms_ticker_attach(stream->ticker, stream->source);

	return stream;
}

void ring_play_dtmf(RingStream *stream, char dtmf, int duration_ms) {
	if (duration_ms > 0) ms_filter_call_method(stream->gendtmf, MS_DTMF_GEN_PLAY, &dtmf);
	else ms_filter_call_method(stream->gendtmf, MS_DTMF_GEN_START, &dtmf);
}

void ring_stop_dtmf(RingStream *stream) {
	//	ms_message("DADA [RingStream] stop dtmf ringing card is %s", ((stream->card) ? stream->card->id : "No
	// default"));
	ms_filter_call_method_noarg(stream->gendtmf, MS_DTMF_GEN_STOP);
}

void ring_stop(RingStream *stream) {
	//	ms_message("DADA [RingStream] stop ringing card in stream is %s", ((stream->card) ? stream->card->id : "No
	// card"));
	MSConnectionHelper h;
	int i;

	if (stream->ticker) {
		ms_ticker_detach(stream->ticker, stream->source);

		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h, stream->source, -1, stream->srcpin);
		if (stream->decoder) {
			ms_connection_helper_unlink(&h, stream->decoder, 0, 0);
		}
		ms_connection_helper_unlink(&h, stream->gendtmf, 0, 0);
		if (stream->write_resampler) ms_connection_helper_unlink(&h, stream->write_resampler, 0, 0);
		ms_connection_helper_unlink(&h, stream->sndwrite_tee, 0, 0);
		for (i = 0; i < MS_RING_STREAM_MAX_SND_CARDS; i++) {
			if (stream->sndwrite[i]) ms_filter_unlink(stream->sndwrite_tee, i + 1, stream->sndwrite[i], 0);
		}
		ms_ticker_destroy(stream->ticker);
	}
	if (stream->source) ms_filter_destroy(stream->source);
	if (stream->gendtmf) ms_filter_destroy(stream->gendtmf);
	for (i = 0; i < MS_RING_STREAM_MAX_SND_CARDS; i++) {
		if (stream->sndwrite[i]) ms_filter_destroy(stream->sndwrite[i]);
	}
	if (stream->sndwrite_tee) ms_filter_destroy(stream->sndwrite_tee);
	if (stream->decoder) ms_filter_destroy(stream->decoder);
	if (stream->write_resampler) ms_filter_destroy(stream->write_resampler);
	for (i = 0; i < MS_RING_STREAM_MAX_SND_CARDS; i++) {
		if (stream->card[i]) ms_snd_card_unref(stream->card[i]);
	}
	ms_free(stream);
}

/*
 * note: Only AAudio and OpenSLES leverage internal ID for output streams.
 */
static void ring_stream_configure_output_snd_card(RingStream *stream) {
	for (int i = 0; i < MS_RING_STREAM_MAX_SND_CARDS; i++) {
		MSSndCard *card = stream->card[i];
		if (card && stream->sndwrite[i]) {
			if (ms_filter_implements_interface(stream->sndwrite[i], MSFilterAudioPlaybackInterface)) {
				ms_filter_call_method(stream->sndwrite[i], MS_AUDIO_PLAYBACK_SET_INTERNAL_ID, card);
				ms_message("[RingStream] set output sound card for %s:%p to %s",
				           ms_filter_get_name(stream->sndwrite[i]), stream->sndwrite, card->id);
			}
		}
	}
}

void ring_stream_set_output_ms_snd_card(RingStream *stream, MSSndCard *snd_card) {
	bctbx_list_t *snd_cards = bctbx_list_new(snd_card);
	ring_stream_set_output_ms_snd_cards(stream, snd_cards);
	bctbx_list_free(snd_cards);
}

MSSndCard *ring_stream_get_output_ms_snd_card(const RingStream *stream) {
	MSSndCard *snd_card = NULL;
	bctbx_list_t *snd_cards = ring_stream_get_output_ms_snd_cards(stream);
	if (snd_cards) snd_card = (MSSndCard *)bctbx_list_get_data(snd_cards);
	bctbx_list_free(snd_cards);
	return snd_card;
}

void ring_stream_set_output_ms_snd_cards(RingStream *stream, const bctbx_list_t *snd_cards) {
	for (int i = 0; i < MS_RING_STREAM_MAX_SND_CARDS; i++) {
		if (stream->card[i]) {
			ms_snd_card_unref(stream->card[i]);
			stream->card[i] = NULL;
		}
	}

	int i;
	const bctbx_list_t *item;
	for (item = snd_cards, i = 0; item; item = bctbx_list_next(item), i++) {
		if (i >= MS_RING_STREAM_MAX_SND_CARDS) {
			ms_warning("Too many sound cards passed to ring_stream_set_output_ms_snd_cards(), some will be ignored!");
			break;
		}
		stream->card[i] = ms_snd_card_ref(bctbx_list_get_data(item));
	}

	ring_stream_configure_output_snd_card(stream);
}

bctbx_list_t *ring_stream_get_output_ms_snd_cards(const RingStream *stream) {
	bctbx_list_t *snd_cards = NULL;

	// If the stream is null, then do not try to access the cards.
	if (stream) {
		for (int i = 0; i < MS_RING_STREAM_MAX_SND_CARDS; i++) {
			if (!stream->card[i]) break;
			snd_cards = bctbx_list_append(snd_cards, stream->card[i]);
		}
	}

	return snd_cards;
}
