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

#include "mediastreamer2/msaudiomixer.h"
#include "mediastreamer2/msconference.h"
#include "mediastreamer2/msmediaplayer.h"
#include "mediastreamer2/mspacketrouter.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/msvolume.h"
#include "private.h"

static const float audio_threshold_min_db = -30.0f;

struct _MSAudioConference {
	MSTicker *ticker;
	MSFilter *mixer;
	MSAudioConferenceParams params;
	bctbx_list_t *members; /* list of MSAudioEndpoint */
	int nmembers;
	MSAudioEndpoint *active_speaker;
};

struct _MSAudioEndpoint {
	AudioStream *st;
	void *user_data;
	MSFilter *in_resampler, *out_resampler;
	MSCPoint out_cut_point;
	MSCPoint out_cut_point_next;
	MSCPoint in_cut_point;
	MSCPoint in_cut_point_prev;
	MSCPoint mixer_in;
	MSCPoint mixer_out;
	MSAudioConference *conference;
	MSFilter *recorder;         /* in case it is a recorder endpoint*/
	MSFilter *recorder_encoder; /* in case the recorder is mkv */
	MSFilter *player; /* in case it is a player endpoint, but also used as a dummy source connected to the mixer for a
	                     recorder endpoint*/
	MSFilter *player_decoder;
	int player_nchannels;
	int pin;
	int samplerate;
	MSConferenceMode conf_mode;
	bool_t muted;
};

MSAudioConference *ms_audio_conference_new(const MSAudioConferenceParams *params, MSFactory *factory) {
	MSAudioConference *obj = ms_new0(MSAudioConference, 1);
	int tmp = 1;
	MSTickerParams ticker_params = {0};
	ticker_params.name = "Audio conference MSTicker";
	ticker_params.prio = __ms_get_default_prio(FALSE);
	obj->ticker = ms_ticker_new_with_params(&ticker_params);
	obj->params = *params;

	if (params->mode == MSConferenceModeMixer) {
		obj->mixer = ms_factory_create_filter(factory, MS_AUDIO_MIXER_ID);
		ms_filter_call_method(obj->mixer, MS_AUDIO_MIXER_ENABLE_CONFERENCE_MODE, &tmp);
		ms_filter_call_method(obj->mixer, MS_FILTER_SET_SAMPLE_RATE, &obj->params.samplerate);
	} else {
		obj->mixer = ms_factory_create_filter(factory, MS_PACKET_ROUTER_ID);

		MSPacketRouterMode mode = MS_PACKET_ROUTER_MODE_AUDIO;
		ms_filter_call_method(obj->mixer, MS_PACKET_ROUTER_SET_ROUTING_MODE, &mode);

		bool_t full_packet = params->mode == MSConferenceModeRouterFullPacket ? TRUE : FALSE;
		ms_filter_call_method(obj->mixer, MS_PACKET_ROUTER_SET_FULL_PACKET_MODE_ENABLED, &full_packet);

		bool_t end_to_end_encryption = params->security_level == MSStreamSecurityLevelEndToEnd ? TRUE : FALSE;
		ms_filter_call_method(obj->mixer, MS_PACKET_ROUTER_SET_END_TO_END_ENCRYPTION_ENABLED, &end_to_end_encryption);
	}

	return obj;
}

const MSAudioConferenceParams *ms_audio_conference_get_params(MSAudioConference *obj) {
	return &obj->params;
}

static MSCPoint just_before(MSFilter *f) {
	MSQueue *q;
	MSCPoint pnull = {0};
	if ((q = f->inputs[0]) != NULL) {
		return q->prev;
	}
	ms_fatal("No filter before %s", f->desc->name);
	return pnull;
}

static MSCPoint just_after(MSFilter *f) {
	MSQueue *q;
	MSCPoint pnull = {0};
	if ((q = f->outputs[0]) != NULL) {
		return q->next;
	}
	ms_fatal("No filter after %s", f->desc->name);
	return pnull;
}

static void cut_audio_stream_graph(MSAudioEndpoint *ep, bool_t is_remote) {
	AudioStream *st = ep->st;

	/*stop the audio graph*/
	ms_ticker_detach(st->ms.sessions.ticker, st->soundread);
	if (!st->ec) ms_ticker_detach(st->ms.sessions.ticker, st->soundwrite);

	ep->in_cut_point_prev.pin = 0;
	if (ep->conf_mode == MSConferenceModeMixer) {
		if (is_remote) {
			/*we would like to keep the volrecv (MSVolume filter) in the graph to measure the output level*/
			ep->in_cut_point_prev.filter = st->volrecv;
		} else {
			ep->in_cut_point_prev.filter = st->plc ? st->plc : st->ms.decoder;
		}
	} else {
		// Full packet mode is remote only
		ep->in_cut_point_prev.filter = st->ms.rtprecv;
	}

	ep->in_cut_point = just_after(ep->in_cut_point_prev.filter);
	ms_filter_unlink(ep->in_cut_point_prev.filter, ep->in_cut_point_prev.pin, ep->in_cut_point.filter,
	                 ep->in_cut_point.pin);

	if (ep->conf_mode == MSConferenceModeMixer) {
		ep->out_cut_point = just_before(st->ms.encoder);
		ep->out_cut_point_next.filter = st->ms.encoder;
		ep->out_cut_point_next.pin = 0;
	} else {
		ep->out_cut_point = just_before(st->ms.rtpsend);
		ep->out_cut_point_next.filter = st->ms.rtpsend;
		ep->out_cut_point_next.pin = 0;
	}

	ms_filter_unlink(ep->out_cut_point.filter, ep->out_cut_point.pin, ep->out_cut_point_next.filter,
	                 ep->out_cut_point_next.pin);

	if (ep->conf_mode == MSConferenceModeMixer) {
		if (ms_filter_has_method(st->ms.encoder, MS_FILTER_GET_SAMPLE_RATE)) {
			ms_filter_call_method(st->ms.encoder, MS_FILTER_GET_SAMPLE_RATE, &ep->samplerate);
		} else {
			ms_filter_call_method(st->ms.rtpsend, MS_FILTER_GET_SAMPLE_RATE, &ep->samplerate);
		}
	} else if (ep->conf_mode == MSConferenceModeRouterFullPacket) {
		bool_t enable = TRUE;
		ms_filter_call_method(st->ms.rtpsend, MS_RTP_SEND_ENABLE_RTP_TRANSFER_MODE, &enable);
		ms_filter_call_method(st->ms.rtprecv, MS_RTP_RECV_ENABLE_RTP_TRANSFER_MODE, &enable);
		rtp_session_enable_transfer_mode(st->ms.sessions.rtp_session, TRUE);
	}

	if (is_remote) {
		ep->mixer_in.filter = ep->in_cut_point_prev.filter;
		ep->mixer_in.pin = ep->in_cut_point_prev.pin;
		ep->mixer_out.filter = ep->out_cut_point_next.filter;
		ep->mixer_out.pin = ep->out_cut_point_next.pin;
	} else {
		ep->mixer_in = ep->out_cut_point;
		ep->mixer_out = ep->in_cut_point;
	}
}

static void redo_audio_stream_graph(MSAudioEndpoint *ep) {
	AudioStream *st = ep->st;

	if (ep->conf_mode == MSConferenceModeRouterFullPacket) {
		bool_t enable = FALSE;
		ms_filter_call_method(st->ms.rtpsend, MS_RTP_SEND_ENABLE_RTP_TRANSFER_MODE, &enable);
		ms_filter_call_method(st->ms.rtprecv, MS_RTP_RECV_ENABLE_RTP_TRANSFER_MODE, &enable);
		rtp_session_enable_transfer_mode(st->ms.sessions.rtp_session, FALSE);
	}

	ms_filter_link(ep->in_cut_point_prev.filter, ep->in_cut_point_prev.pin, ep->in_cut_point.filter,
	               ep->in_cut_point.pin);
	ms_filter_link(ep->out_cut_point.filter, ep->out_cut_point.pin, ep->out_cut_point_next.filter,
	               ep->out_cut_point_next.pin);
	ms_ticker_attach(st->ms.sessions.ticker, st->soundread);
	if (!st->ec) ms_ticker_attach(st->ms.sessions.ticker, st->soundwrite);
}

static int find_free_pin(MSFilter *mixer) {
	int i;
	for (i = 0; i < mixer->desc->ninputs; ++i) {
		if (mixer->inputs[i] == NULL) {
			return i;
		}
	}
	ms_fatal("No more free pin in mixer filter");
	return -1;
}

static void plumb_to_conf(MSAudioEndpoint *ep) {
	MSAudioConference *conf = ep->conference;

	ep->pin = find_free_pin(conf->mixer);

	if (ep->conf_mode != MSConferenceModeMixer) {
		if (ep->mixer_in.filter) {
			ms_filter_link(ep->mixer_in.filter, ep->mixer_in.pin, conf->mixer, ep->pin);
		}

		if (ep->mixer_out.filter) {
			ms_filter_link(conf->mixer, ep->pin, ep->mixer_out.filter, ep->mixer_out.pin);
		}

		return;
	}

	int in_rate = ep->samplerate, out_rate = ep->samplerate;

	if (ep->samplerate != -1) {
		out_rate = in_rate = ep->samplerate;
	} else in_rate = out_rate = conf->params.samplerate;

	if (ep->recorder_encoder) {
		ms_filter_call_method(ep->recorder_encoder, MS_FILTER_SET_SAMPLE_RATE, &conf->params.samplerate);
	} else if (ep->recorder) {
		ms_filter_call_method(ep->recorder, MS_FILTER_SET_SAMPLE_RATE, &conf->params.samplerate);
	}

	if (ep->mixer_in.filter) {
		ms_filter_link(ep->mixer_in.filter, ep->mixer_in.pin, ep->in_resampler, 0);
		ms_filter_link(ep->in_resampler, 0, conf->mixer, ep->pin);
	}
	if (ep->mixer_out.filter) {
		ms_filter_link(conf->mixer, ep->pin, ep->out_resampler, 0);
		ms_filter_link(ep->out_resampler, 0, ep->mixer_out.filter, ep->mixer_out.pin);
	}

	/*configure resamplers*/
	ms_filter_call_method(ep->in_resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &conf->params.samplerate);
	ms_filter_call_method(ep->out_resampler, MS_FILTER_SET_SAMPLE_RATE, &conf->params.samplerate);
	ms_filter_call_method(ep->in_resampler, MS_FILTER_SET_SAMPLE_RATE, &in_rate);
	ms_filter_call_method(ep->out_resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &out_rate);
	if (ep->player_nchannels != -1) {
		int nchannels = 2;
		ms_filter_call_method(ep->in_resampler, MS_FILTER_SET_NCHANNELS, &ep->player_nchannels);
		ms_filter_call_method(ep->in_resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &nchannels);
	}
}

static int request_volumes(BCTBX_UNUSED(MSFilter *filter), rtp_audio_level_t **audio_levels, void *user_data) {
	MSAudioEndpoint *ep = (MSAudioEndpoint *)user_data;
	bctbx_list_t *it;

	if (ep == NULL || ep->conference == NULL) return 0;

	AudioStreamVolumes *volumes = audio_stream_volumes_new();
	int volumes_size = 0;

	for (it = ep->conference->members; it != NULL; it = it->next) {
		MSAudioEndpoint *data = (MSAudioEndpoint *)it->data;
		if (data != NULL && data->st != NULL) {
			int is_remote = (data->in_cut_point_prev.filter == data->st->volrecv);

			if (is_remote) {
				volumes_size += audio_stream_volumes_append(volumes, data->st->participants_volumes);
			} else {
				if (data->st->volsend) {
					float db = MS_VOLUME_DB_LOWEST;

					ms_filter_call_method(data->st->volsend, MS_VOLUME_GET_GAIN, &db);
					if (db != 0) ms_filter_call_method(data->st->volsend, MS_VOLUME_GET, &db);
					else db = MS_VOLUME_DB_MUTED;

					audio_stream_volumes_insert(volumes, rtp_session_get_send_ssrc(data->st->ms.sessions.rtp_session),
					                            (int)db);
					volumes_size++;
				}
			}
		}
	}

	if (volumes_size > 0) {
		*audio_levels = (rtp_audio_level_t *)ms_malloc0(volumes_size * sizeof(rtp_audio_level_t));
		audio_stream_volumes_populate_audio_levels(volumes, *audio_levels);
	}

	audio_stream_volumes_delete(volumes);

	return volumes_size;
}

static void configure_output(MSAudioEndpoint *ep) {
	if (ep->pin < 0) return;

	MSPacketRouterPinData pd;
	pd.input = pd.output = pd.self = ep->pin;
	memset(pd.extension_ids, 0, sizeof(pd.extension_ids));

	// Set the correct extension ids negotiated
	// We don't do MID as it is already rewritten by the session in transfer mode
	pd.extension_ids[RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL] = ep->st->client_to_mixer_extension_id;
	pd.extension_ids[RTP_EXTENSION_MIXER_TO_CLIENT_AUDIO_LEVEL] = ep->st->mixer_to_client_extension_id;

	ms_filter_call_method(ep->conference->mixer, MS_PACKET_ROUTER_CONFIGURE_OUTPUT, &pd);
}

static void unconfigure_output(MSAudioEndpoint *ep) {
	if (ep->pin < 0) return;

	ms_filter_call_method(ep->conference->mixer, MS_PACKET_ROUTER_UNCONFIGURE_OUTPUT, &ep->pin);
}

void ms_audio_conference_add_member(MSAudioConference *obj, MSAudioEndpoint *ep) {
	/* now connect to the mixer */
	ep->conference = obj;
	if (obj->nmembers > 0) ms_ticker_detach(obj->ticker, obj->mixer);
	plumb_to_conf(ep);
	ms_ticker_attach(obj->ticker, obj->mixer);
	obj->members = bctbx_list_append(obj->members, ep);
	obj->nmembers++;

	if (obj->params.mode == MSConferenceModeMixer) {
		ms_audio_conference_mute_member(obj, ep, ep->muted);

		// If mixer to client extension id is configured then add the needed callback
		if (ep->st && ep->st->mixer_to_client_extension_id > 0) {
			MSFilterRequestMixerToClientDataCb callback;
			callback.cb = request_volumes;
			callback.user_data = ep;
			ms_filter_call_method(ep->st->ms.rtpsend, MS_RTP_SEND_SET_MIXER_TO_CLIENT_DATA_REQUEST_CB, &callback);
		}
	} else {
		configure_output(ep);
	}
}

static void unplumb_from_conf(MSAudioEndpoint *ep) {
	MSAudioConference *conf = ep->conference;

	if (ep->mixer_in.filter) {
		if (ep->conf_mode == MSConferenceModeMixer) {
			ms_filter_unlink(ep->mixer_in.filter, ep->mixer_in.pin, ep->in_resampler, 0);
			ms_filter_unlink(ep->in_resampler, 0, conf->mixer, ep->pin);
		} else {
			ms_filter_unlink(ep->mixer_in.filter, ep->mixer_in.pin, conf->mixer, ep->pin);
		}
	}
	if (ep->mixer_out.filter) {
		if (ep->conf_mode == MSConferenceModeMixer) {
			ms_filter_unlink(conf->mixer, ep->pin, ep->out_resampler, 0);
			ms_filter_unlink(ep->out_resampler, 0, ep->mixer_out.filter, ep->mixer_out.pin);
		} else {
			ms_filter_unlink(conf->mixer, ep->pin, ep->mixer_out.filter, ep->mixer_out.pin);
		}
	}
}

void ms_audio_conference_remove_member(MSAudioConference *obj, MSAudioEndpoint *ep) {
	if (ep->conf_mode != MSConferenceModeMixer) unconfigure_output(ep);
	ms_ticker_detach(obj->ticker, obj->mixer);
	unplumb_from_conf(ep);
	ep->conference = NULL;
	obj->nmembers--;
	obj->members = bctbx_list_remove(obj->members, ep);
	if (obj->nmembers > 0) ms_ticker_attach(obj->ticker, obj->mixer);
}

void ms_audio_conference_mute_member(MSAudioConference *obj, MSAudioEndpoint *ep, bool_t muted) {
	if (obj->params.mode != MSConferenceModeMixer) {
		ms_warning("Cannot mute participant when the conference is not in mixer mode");
		return;
	}

	MSAudioMixerCtl ctl = {0};
	ctl.pin = ep->pin;
	ctl.param.active = !muted;
	ep->muted = muted;
	ms_filter_call_method(ep->conference->mixer, MS_AUDIO_MIXER_SET_ACTIVE, &ctl);
}

int ms_audio_conference_get_size(MSAudioConference *obj) {
	return obj->nmembers;
}

int ms_audio_conference_get_participant_volume(MSAudioConference *obj, uint32_t ssrc) {
	bctbx_list_t *it;

	for (it = obj->members; it != NULL; it = it->next) {
		MSAudioEndpoint *data = (MSAudioEndpoint *)it->data;
		int is_remote = (data->in_cut_point_prev.filter == data->st->volrecv);
		MSFilter *volume_filter = is_remote ? data->st->volrecv : data->st->volsend;
		uint32_t member_ssrc = is_remote ? rtp_session_get_recv_ssrc(data->st->ms.sessions.rtp_session)
		                                 : rtp_session_get_send_ssrc(data->st->ms.sessions.rtp_session);

		if (member_ssrc != ssrc) continue;
		if (data->muted) return MS_VOLUME_DB_LOWEST;

		if (volume_filter) {
			float db = MS_VOLUME_DB_LOWEST;
			if (ms_filter_call_method(volume_filter, MS_VOLUME_GET, &db) == 0) {
				return (int)db;
			}
		}
	}

	return AUDIOSTREAMVOLUMES_NOT_FOUND;
}

void ms_audio_conference_process_events(MSAudioConference *obj) {
	const bctbx_list_t *elem;
	MSAudioEndpoint *winner = NULL;

	if (obj->params.mode == MSConferenceModeRouterFullPacket) {
		int pin = -1;
		ms_filter_call_method(obj->mixer, MS_PACKET_ROUTER_GET_ACTIVE_SPEAKER_PIN, &pin);

		if (pin > -1) {
			for (elem = obj->members; elem != NULL; elem = elem->next) {
				MSAudioEndpoint *ep = (MSAudioEndpoint *)elem->data;
				if (ep->pin == pin) {
					winner = ep;
					break;
				}
			}
		}
	} else if (obj->params.mode == MSConferenceModeMixer) {
		float max_db_over_member = MS_VOLUME_DB_LOWEST;

		for (elem = obj->members; elem != NULL; elem = elem->next) {
			MSAudioEndpoint *ep = (MSAudioEndpoint *)elem->data;
			int is_remote;
			if (ep->st == NULL) continue; /* This happens for the player/recorder special endpoint */
			is_remote = (ep->in_cut_point_prev.filter == ep->st->volrecv);
			MSFilter *volume_filter = is_remote ? ep->st->volrecv : ep->st->volsend;
			if (ep->muted) continue;
			if (volume_filter) {
				float max_db = MS_VOLUME_DB_LOWEST;
				if (ms_filter_call_method(volume_filter, MS_VOLUME_GET_MAX, &max_db) == 0) {
					if (max_db > audio_threshold_min_db && max_db > max_db_over_member) {
						max_db_over_member = max_db;
						winner = ep;
					}
				}
			}
		}
	}

	if (obj->active_speaker != winner && winner != NULL) {
		ms_message("Active speaker changed: now on pin %i", winner->pin);
		if (obj->params.active_talker_callback) obj->params.active_talker_callback(obj, winner);
		obj->active_speaker = winner;
	}
}

void ms_audio_conference_destroy(MSAudioConference *obj) {
	ms_ticker_destroy(obj->ticker);
	ms_filter_destroy(obj->mixer);
	ms_free(obj);
}

MSAudioEndpoint *ms_audio_endpoint_new(void) {
	MSAudioEndpoint *ep = ms_new0(MSAudioEndpoint, 1);

	ep->samplerate = 8000;
	ep->player_nchannels = -1;
	return ep;
}

void ms_audio_endpoint_set_user_data(MSAudioEndpoint *ep, void *user_data) {
	ep->user_data = user_data;
}

void *ms_audio_endpoint_get_user_data(const MSAudioEndpoint *ep) {
	return ep->user_data;
}

MSAudioEndpoint *ms_audio_endpoint_get_from_stream(AudioStream *st, bool_t is_remote, MSConferenceMode conf_mode) {
	MSAudioEndpoint *ep = ms_audio_endpoint_new();
	ep->st = st;
	ep->conf_mode = conf_mode;

	if (ep->conf_mode == MSConferenceModeMixer) {
		ep->in_resampler = ms_factory_create_filter(st->ms.factory, MS_RESAMPLE_ID);
		ep->out_resampler = ms_factory_create_filter(st->ms.factory, MS_RESAMPLE_ID);
	} else {
		if (!is_remote) {
			ms_error("Cannot set local audio endpoint if full packet mode is enabled, changing to remote.");
			is_remote = TRUE;
		}
	}

	cut_audio_stream_graph(ep, is_remote);

	return ep;
}

void ms_audio_endpoint_release_from_stream(MSAudioEndpoint *obj) {
	// Remove volumes callback if any before destroying the endpoint
	if (obj->st && obj->st->mixer_to_client_extension_id > 0) {
		MSFilterRequestMixerToClientDataCb callback;
		callback.cb = NULL;
		callback.user_data = NULL;
		ms_filter_call_method(obj->st->ms.rtpsend, MS_RTP_SEND_SET_MIXER_TO_CLIENT_DATA_REQUEST_CB, &callback);
	}

	redo_audio_stream_graph(obj);
	ms_audio_endpoint_destroy(obj);
}

void ms_audio_endpoint_destroy(MSAudioEndpoint *ep) {
	if (ep->in_resampler) ms_filter_destroy(ep->in_resampler);
	if (ep->out_resampler) ms_filter_destroy(ep->out_resampler);
	if (ep->recorder_encoder) {
		ms_filter_unlink(ep->recorder_encoder, 0, ep->recorder, 0);
		ms_filter_destroy(ep->recorder_encoder);
	}
	if (ep->recorder) ms_filter_destroy(ep->recorder);
	if (ep->player) {
		MSPlayerState state;
		if (ms_filter_call_method(ep->player, MS_PLAYER_GET_STATE, &state) == 0) {
			if (state != MSPlayerClosed) ms_filter_call_method_noarg(ep->player, MS_PLAYER_CLOSE);
		}
		ms_filter_destroy(ep->player);
	}
	ms_free(ep);
}

MSAudioEndpoint *ms_audio_endpoint_new_recorder(MSFactory *factory, const char *path) {
	MSAudioEndpoint *ep = ms_audio_endpoint_new();

	if (ms_path_ends_with(path, ".mkv") || ms_path_ends_with(path, ".mka") || ms_path_ends_with(path, ".smff")) {
		MSPinFormat pinfmt = {0};

		ep->recorder_encoder = ms_factory_create_filter(factory, MS_OPUS_ENC_ID);
		if (ms_path_ends_with(path, ".smff")) {
			ep->recorder = ms_factory_create_filter(factory, MS_SMFF_RECORDER_ID);
		} else {
			ep->recorder = ms_factory_create_filter(factory, MS_MKV_RECORDER_ID);
		}
		ms_filter_link(ep->recorder_encoder, 0, ep->recorder, 0);

		pinfmt.pin = 0;
		pinfmt.fmt = ms_factory_get_audio_format(factory, "opus", 48000, 1, NULL);
		ms_filter_call_method(ep->recorder, MS_FILTER_SET_INPUT_FMT, &pinfmt);
	} else if (ms_path_ends_with(path, ".wav")) {
		ep->recorder = ms_factory_create_filter(factory, MS_FILE_REC_ID);
	} else {
		ms_error("Unsupported audio file extension for path %s .", path);
		ms_audio_endpoint_destroy(ep);
		return NULL;
	}
	ms_filter_call_method(ep->recorder, MS_RECORDER_OPEN, (void *)path);

	ep->in_resampler = ms_factory_create_filter(factory, MS_RESAMPLE_ID);
	ep->out_resampler = ms_factory_create_filter(factory, MS_RESAMPLE_ID);
	ep->player = ms_factory_create_filter(factory, MS_FILE_PLAYER_ID);
	ep->mixer_out.filter = ep->recorder_encoder ? ep->recorder_encoder : ep->recorder;
	ep->mixer_in.filter = ep->player;
	ep->samplerate = -1;
	return ep;
}

int ms_audio_recorder_endpoint_start(MSAudioEndpoint *ep) {
	MSRecorderState state;
	if (!ep->recorder) {
		ms_error("This endpoint isn't a recorder endpoint.");
		return -1;
	}
	ms_filter_call_method(ep->recorder, MS_RECORDER_GET_STATE, &state);
	if (state != MSRecorderPaused) {
		ms_error("Recorder not bad state, cannot start.");
		return -1;
	}
	return ms_filter_call_method_noarg(ep->recorder, MS_RECORDER_START);
}

int ms_audio_recorder_endpoint_stop(MSAudioEndpoint *ep) {
	if (!ep->recorder) {
		return -1;
	}
	return ms_filter_call_method_noarg(ep->recorder, MS_RECORDER_CLOSE);
}

MSAudioEndpoint *ms_audio_endpoint_new_player(MSFactory *factory, const char *path) {
	MSFileFormat format = MS_FILE_FORMAT_UNKNOWN;
	MSAudioEndpoint *ep = ms_audio_endpoint_new();

	if (ms_path_ends_with(path, ".mkv") || ms_path_ends_with(path, ".mka")) {
		format = MS_FILE_FORMAT_MATROSKA;
		ep->player = ms_factory_create_filter(factory, MS_MKV_PLAYER_ID);
	} else if (ms_path_ends_with(path, ".wav")) {
		format = MS_FILE_FORMAT_WAVE;
		ep->player = ms_factory_create_filter(factory, MS_FILE_PLAYER_ID);
	} else if (ms_path_ends_with(path, ".smff")) {
		format = MS_FILE_FORMAT_SMFF;
		ep->player = ms_factory_create_filter(factory, MS_SMFF_PLAYER_ID);
	} else {
		ms_error("Unsupported audio file extension for path %s.", path);
		ms_audio_endpoint_destroy(ep);
		return NULL;
	}

	if (ms_filter_call_method(ep->player, MS_PLAYER_OPEN, (void *)path) == -1) {
		ms_audio_endpoint_destroy(ep);
		return NULL;
	}

	MSPinFormat pinfmt = {0};
	switch (format) {
		case MS_FILE_FORMAT_WAVE:
			ms_filter_call_method(ep->player, MS_FILTER_GET_SAMPLE_RATE, &ep->samplerate);
			break;
		case MS_FILE_FORMAT_MATROSKA:
		case MS_FILE_FORMAT_SMFF:
			pinfmt.pin = 1;
			ms_filter_call_method(ep->player, MS_FILTER_GET_OUTPUT_FMT, &pinfmt);
			if (pinfmt.fmt) {
				ep->player_decoder = ms_factory_create_decoder(factory, pinfmt.fmt->encoding);
				if (ep->player_decoder == NULL) {
					ms_error("Could not create audio decoder for %s", pinfmt.fmt->encoding);
				} else {
					ep->samplerate = pinfmt.fmt->rate;
					ep->player_nchannels = pinfmt.fmt->nchannels;
					if (strcmp(pinfmt.fmt->encoding, "opus") == 0) {
						// Disable PLC in Opus decoder to prevent white noise when pausing media player
						const char *fmtp = "plc=0";
						ms_filter_call_method(ep->player_decoder, MS_FILTER_ADD_FMTP, (void *)fmtp);
					}
					ms_filter_link(ep->player, 1, ep->player_decoder, 0);
				}
			}
			break;
		case MS_FILE_FORMAT_UNKNOWN:
			break;
	}

	ep->in_resampler = ms_factory_create_filter(factory, MS_RESAMPLE_ID);
	ep->out_resampler = ms_factory_create_filter(factory, MS_RESAMPLE_ID);
	ep->mixer_in.filter = ep->player_decoder ? ep->player_decoder : ep->player;
	/* we create a dummy recorder in order to always have something connected in output of the mixer
	 * if the event where the player audio endpoint is the only conference member,
	 * otherwise this will create an invalid mediastreamer2 graph */
	ep->recorder = ms_factory_create_filter(factory, MS_VOID_SINK_ID);
	ep->mixer_out.filter = ep->recorder;

	return ep;
}

void ms_audio_player_endpoint_set_eof_cb(MSAudioEndpoint *ep, MSFilterNotifyFunc cb, void *user_data) {
	ms_filter_add_notify_callback(ep->player, cb, user_data, FALSE);
}

int ms_audio_player_endpoint_start(MSAudioEndpoint *ep) {
	MSPlayerState state;
	if (!ep->player || !ep->recorder || ep->recorder->desc->id != MS_VOID_SINK_ID) {
		ms_error("This endpoint isn't a player endpoint.");
		return -1;
	}
	ms_filter_call_method(ep->player, MS_PLAYER_GET_STATE, &state);
	if (state != MSPlayerPaused) {
		ms_error("The endpoint player is in a bad state, cannot start.");
		return -1;
	}
	return ms_filter_call_method_noarg(ep->player, MS_PLAYER_START);
}

int ms_audio_player_endpoint_stop(MSAudioEndpoint *ep) {
	if (!ep->player) {
		return -1;
	}
	return ms_filter_call_method_noarg(ep->player, MS_PLAYER_CLOSE);
}

int ms_audio_player_endpoint_pause(MSAudioEndpoint *ep) {
	return ms_filter_call_method_noarg(ep->player, MS_PLAYER_PAUSE);
}

int ms_audio_player_endpoint_seek(MSAudioEndpoint *ep, int time_ms) {
	return ms_filter_call_method(ep->player, MS_PLAYER_SEEK_MS, &time_ms);
}

MSPlayerState ms_audio_player_endpoint_get_state(const MSAudioEndpoint *ep) {
	MSPlayerState state = MSPlayerClosed;
	ms_filter_call_method(ep->player, MS_PLAYER_GET_STATE, &state);
	return state;
}
