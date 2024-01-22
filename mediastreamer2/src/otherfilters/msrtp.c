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

#include "bctoolbox/crypto.h"
#include "bctoolbox/defs.h"

#include "mediastreamer2/box-plot.h"
#include "mediastreamer2/msfactory.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/msticker.h"

#include "ortp/telephonyevents.h"
#if defined(__cplusplus)
#define B64_NO_NAMESPACE
#endif
#include "mediastreamer2/stun.h"

static const int default_dtmf_duration_ms = 100; /*in milliseconds*/

struct SenderData {
	RtpSession *session;
	MSBoxPlot processing_delay_stats;
	uint32_t tsoff;
	uint32_t last_ts;
	int64_t last_sent_time;
	int64_t last_rtp_stun_sent_time;
	int64_t last_rtcp_stun_sent_time;
	uint32_t skip_until;
	int rate;
	int dtmf_duration;
	int dtmf_ts_step;
	uint32_t dtmf_ts_cur;
	char relay_session_id[64];
	int relay_session_id_size;
	uint64_t last_rsi_time;
	int timestamp_adjustment_threshold;
	MSCngData cng_data;
	char dtmf;
	bool_t dtmf_start;
	bool_t skip;
	bool_t mute;
	bool_t use_task;
	bool_t stun_enabled;
	bool_t stun_forced_enabled;
	bool_t enable_ts_adjustment;
	int audio_level_send_interval;
	uint64_t last_audio_level_sent_time;
	int mixer_to_client_extension_id;
	MSRtpSendRequestMixerToClientDataCb mtc_request_data_cb;
	void *mtc_request_data_user_data;
	int mtc_volumes_left_to_send;
	int mtc_volumes_size;
	rtp_audio_level_t *mtc_volumes;
	int client_to_mixer_extension_id;
	MSRtpSendRequestClientToMixerDataCb ctm_request_data_cb;
	void *ctm_request_data_user_data;
	int frame_marking_extension_id;
	bool_t frame_start;
	bool_t rtp_transfer_mode;
	bool_t voice_activity;
};

typedef struct SenderData SenderData;

/* Send dummy STUN packet to open NAT ports ASAP. */
static void send_stun_packet(SenderData *d, bool_t enable_rtp, bool_t enable_rtcp) {
	MSStunMessage *msg;
	mblk_t *mp;
	RtpSession *s = d->session;
	char *buf = NULL;
	size_t len;

	if (!d->stun_enabled && !d->stun_forced_enabled) return;
	if (ms_is_multicast_addr((const struct sockaddr *)&s->rtcp.gs.loc_addr)) {
		ms_debug("Stun packet not sent for session [%p] because of multicast", s);
		return;
	}

	msg = ms_stun_binding_request_create();
	len = ms_stun_message_encode(msg, &buf);
	if (len > 0) {
		if (enable_rtp) {
			mp = allocb(len, BPRI_MED);
			memcpy(mp->b_wptr, buf, len);
			mp->b_wptr += len;

			ms_message("Stun packet of length %0zd sent on rtp for session [%p] %s", len, s,
			           d->stun_forced_enabled ? "(forced)" : "");
			rtp_session_sendm_with_ts(s, mp, 0);
		}
		if (enable_rtcp) {
			mp = allocb(len, BPRI_MED);
			memcpy(mp->b_wptr, buf, len);
			mp->b_wptr += len;
			ms_message("Stun packet of length %0zd sent on rtcp for session [%p] %s", len, s,
			           d->stun_forced_enabled ? "(forced)" : "");
			rtp_session_rtcp_sendm_raw(s, mp);
		}
	}
	if (buf != NULL) ms_free(buf);
	ms_stun_message_destroy(msg);
}

static void sender_init(MSFilter *f) {
	SenderData *d = (SenderData *)ms_new0(SenderData, 1);
	const char *tmp = NULL;
#ifndef MS2_WINDOWS_UNIVERSAL
	tmp = getenv("MS2_RTP_FIXED_DELAY");
#endif

	d->session = NULL;
	d->tsoff = 0;
	d->skip_until = 0;
	d->skip = FALSE;
	d->rate = 8000;
	d->dtmf = 0;
	d->dtmf_duration = 800;
	d->dtmf_ts_step = 160;
	d->mute = FALSE;
	d->relay_session_id_size = 0;
	d->last_rsi_time = 0;
	d->last_sent_time = -1;
	d->last_rtp_stun_sent_time = -1;
	d->last_rtcp_stun_sent_time = -1;
	d->last_ts = 0;
	d->use_task = tmp ? (!!atoi(tmp)) : FALSE;
	d->timestamp_adjustment_threshold = d->rate / 5;
	if (d->use_task) ms_message("MSRtpSend will use tasks to send out packet at the beginning of ticks.");
	d->stun_enabled = TRUE;
	d->enable_ts_adjustment = TRUE;
	d->audio_level_send_interval = 250;
	d->last_audio_level_sent_time = 0;
	d->mixer_to_client_extension_id = 0;
	d->mtc_request_data_cb = NULL;
	d->mtc_request_data_user_data = NULL;
	d->mtc_volumes_left_to_send = 0;
	d->mtc_volumes_size = 0;
	d->mtc_volumes = NULL;
	d->client_to_mixer_extension_id = 0;
	d->ctm_request_data_cb = NULL;
	d->ctm_request_data_user_data = NULL;
	d->frame_marking_extension_id = 0;
	d->frame_start = TRUE;
	d->rtp_transfer_mode = FALSE;
	d->voice_activity = FALSE;
	f->data = d;
}

static void sender_uninit(MSFilter *f) {
	SenderData *d = (SenderData *)f->data;
	if (d->mtc_volumes) ms_free(d->mtc_volumes);
	ms_free(d);
}

static int sender_telephone_event_supported(MSFilter *f, BCTBX_UNUSED(void *_)) {
	SenderData *d = (SenderData *)f->data;
	return -1 < rtp_profile_find_payload_number(d->session->snd.profile, "telephone-event", d->rate, 1);
};

static int sender_send_dtmf(MSFilter *f, void *arg) {
	const char *dtmf = (const char *)arg;
	SenderData *d = (SenderData *)f->data;

	ms_filter_lock(f);
	if (d->skip == TRUE) {
		ms_filter_unlock(f);
		ms_warning("MSRtpSend: already sending a dtmf.");
		return -1;
	}
	d->dtmf = dtmf[0];
	ms_filter_unlock(f);
	return 0;
}

static int sender_set_dtmf_duration(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	d->dtmf_duration = *((int *)arg);
	return 0;
}

static int sender_set_session(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	RtpSession *s = (RtpSession *)arg;
	PayloadType *pt = rtp_profile_get_payload(rtp_session_get_profile(s), rtp_session_get_send_payload_type(s));
	d->session = s;
	if (pt != NULL) {
		d->rate = pt->clock_rate;
		d->dtmf_duration = (default_dtmf_duration_ms * d->rate) / 1000;
		d->dtmf_ts_step = (20 * d->rate) / 1000;
		/*Tolerate more jitter in video streams, knowing that video packets are not discarded if they are late*/
		d->timestamp_adjustment_threshold = (pt->type == PAYLOAD_VIDEO) ? d->rate : d->rate / 5;
		send_stun_packet(d, TRUE, TRUE);
	} else {
		ms_warning("Sending undefined payload type ?");
	}
	return 0;
}

static int sender_mute(MSFilter *f, BCTBX_UNUSED(void *arg)) {
	SenderData *d = (SenderData *)f->data;
	ms_filter_lock(f);
	d->mute = TRUE;
	ms_filter_unlock(f);
	return 0;
}

static int sender_unmute(MSFilter *f, BCTBX_UNUSED(void *arg)) {
	SenderData *d = (SenderData *)f->data;
	ms_filter_lock(f);
	d->mute = FALSE;
	ms_filter_unlock(f);
	return 0;
}

static int sender_set_relay_session_id(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	const char *tmp = (const char *)arg;
	size_t id_size = sizeof(d->relay_session_id);
	bctbx_base64_decode((void *)d->relay_session_id, &id_size, (const unsigned char *)tmp, strlen(tmp));
	d->relay_session_id_size = (int)id_size;
	return 0;
}

static int sender_set_audio_level_send_interval(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	int *interval = (int *)arg;
	d->audio_level_send_interval = *interval;
	return 0;
}

static int sender_set_mixer_to_client_extension_id(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	int *id = (int *)arg;
	d->mixer_to_client_extension_id = *id;
	return 0;
}

static int sender_set_mixer_to_client_data_request_cb(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	MSFilterRequestMixerToClientDataCb *cb = (MSFilterRequestMixerToClientDataCb *)arg;
	d->mtc_request_data_cb = cb->cb;
	d->mtc_request_data_user_data = cb->user_data;
	return 0;
}

static int sender_set_client_to_mixer_extension_id(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	int *id = (int *)arg;
	d->client_to_mixer_extension_id = *id;
	return 0;
}

static int sender_set_client_to_mixer_data_request_cb(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	MSFilterRequestClientToMixerDataCb *cb = (MSFilterRequestClientToMixerDataCb *)arg;
	d->ctm_request_data_cb = cb->cb;
	d->ctm_request_data_user_data = cb->user_data;
	return 0;
}

static int sender_set_frame_marking_extension_id(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	int *id = (int *)arg;
	d->frame_marking_extension_id = *id;
	return 0;
}

static int sender_get_sr(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	PayloadType *pt;
	if (d->session == NULL) {
		ms_warning("Could not obtain sample rate, session is not set.");
		return -1;
	}
	pt = rtp_profile_get_payload(rtp_session_get_profile(d->session), rtp_session_get_recv_payload_type(d->session));
	if (pt != NULL) {
		if (strcasecmp(pt->mime_type, "G722") == 0) *(int *)arg = 16000;
		else *(int *)arg = pt->clock_rate;
	} else {
		ms_warning("MSRtpSend: Could not obtain sample rate, payload type is unknown.");
		return -1;
	}
	return 0;
}

static int sender_get_ch(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	PayloadType *pt;
	if (d->session == NULL) {
		ms_warning("Could not obtain number of channels, session is not set.");
		return -1;
	}
	pt = rtp_profile_get_payload(rtp_session_get_profile(d->session), rtp_session_get_recv_payload_type(d->session));
	if (pt == NULL) {
		ms_warning("MSRtpSend: Could not obtain number of channels, payload type is unknown.");
		return -1;
	}
	*(int *)arg = pt->channels;
	return 0;
}

/* the goal of that function is to return a absolute timestamp closest to real time, with respect of given packet_ts,
 * which is a relative to an undefined origin*/
static uint32_t get_cur_timestamp(MSFilter *f, mblk_t *im) {
	SenderData *d = (SenderData *)f->data;
	uint32_t curts = (uint32_t)((f->ticker->time * (uint64_t)d->rate) / (uint64_t)1000);
	int diffts;
	uint32_t netts;
	int difftime_ts;

	if (im &&
	    d->dtmf ==
	        0) { /*do not perform timestamp adjustment while a dtmf is being sent, otherwise durations are erroneous */
		uint32_t packet_ts = mblk_get_timestamp_info(im);
		if (d->last_sent_time == -1) {
			d->tsoff = curts - packet_ts;
		} else if (d->enable_ts_adjustment) {
			diffts = packet_ts - d->last_ts;
			difftime_ts = (int)(((f->ticker->time - d->last_sent_time) * d->rate) / 1000);
			/* detect timestamp jump in the stream and adjust so that they become continuous on the network*/
			if (abs(diffts - difftime_ts) > (d->timestamp_adjustment_threshold)) {
				uint32_t tsoff = curts - packet_ts;
				ms_message("Adjusting output timestamp by %i", (tsoff - d->tsoff));
				d->tsoff = tsoff;
			}
		}
		netts = packet_ts + d->tsoff;
		d->last_sent_time = f->ticker->time;
		d->last_ts = packet_ts;
	} else netts = curts;
	return netts;
}

static int send_dtmf(MSFilter *f, uint32_t timestamp_start) {
	SenderData *d = (SenderData *)f->data;
	mblk_t *m1;
	int tev_type;

	/* create the first telephony event packet */
	switch (d->dtmf) {
		case '1':
			tev_type = TEV_DTMF_1;
			break;
		case '2':
			tev_type = TEV_DTMF_2;
			break;
		case '3':
			tev_type = TEV_DTMF_3;
			break;
		case '4':
			tev_type = TEV_DTMF_4;
			break;
		case '5':
			tev_type = TEV_DTMF_5;
			break;
		case '6':
			tev_type = TEV_DTMF_6;
			break;
		case '7':
			tev_type = TEV_DTMF_7;
			break;
		case '8':
			tev_type = TEV_DTMF_8;
			break;
		case '9':
			tev_type = TEV_DTMF_9;
			break;
		case '*':
			tev_type = TEV_DTMF_STAR;
			break;
		case '0':
			tev_type = TEV_DTMF_0;
			break;
		case '#':
			tev_type = TEV_DTMF_POUND;
			break;

		case 'A':
		case 'a':
			tev_type = TEV_DTMF_A;
			break;

		case 'B':
		case 'b':
			tev_type = TEV_DTMF_B;
			break;

		case 'C':
		case 'c':
			tev_type = TEV_DTMF_C;
			break;

		case 'D':
		case 'd':
			tev_type = TEV_DTMF_D;
			break;

		case '!':
			tev_type = TEV_FLASH;
			break;

		default:
			ms_warning("Bad dtmf: %c.", d->dtmf);
			d->skip = FALSE; /*reset dtmf*/
			d->dtmf = 0;
			return -1;
	}

	m1 = rtp_session_create_telephone_event_packet(d->session, timestamp_start == d->dtmf_ts_cur);

	if (m1 == NULL) {
		ms_warning(
		    "There is no usable telephone-event payload type configured in the RTP session. The DTMF cannot be sent.");
		return -1;
	}

	d->dtmf_ts_cur += d->dtmf_ts_step;
	if (RTP_TIMESTAMP_IS_NEWER_THAN(d->dtmf_ts_cur, d->skip_until)) {
		// retransmit end of rtp dtmf event
		mblk_t *tmp;
		rtp_session_add_telephone_event(d->session, m1, tev_type, 1, 10, (d->dtmf_ts_cur - timestamp_start));
		tmp = copymsg(m1);
		rtp_session_sendm_with_ts(d->session, tmp, timestamp_start);
		d->session->rtp.snd_seq--;
		tmp = copymsg(m1);
		rtp_session_sendm_with_ts(d->session, tmp, timestamp_start);
		d->session->rtp.snd_seq--;
		rtp_session_sendm_with_ts(d->session, m1, timestamp_start);
		d->skip = FALSE;
		d->dtmf = 0;
		ms_message("Finished sending RFC2833 dtmf %c", d->dtmf);
	} else {
		rtp_session_add_telephone_event(d->session, m1, tev_type, 0, 10, (d->dtmf_ts_cur - timestamp_start));
		rtp_session_sendm_with_ts(d->session, m1, timestamp_start);
	}
	return 0;
}

static void check_stun_sending(MSFilter *f) {
	SenderData *d = (SenderData *)f->data;
	RtpSession *s = d->session;
	/* No need to send stun packets if media was sent during last 20s (or the last 2s while we still have not sent any
	 * packets) */
	uint64_t last_sent_timeout = 20000;
	if (rtp_session_get_stats(s)->packet_sent == 0) last_sent_timeout = 2000;

	if ((d->last_rtp_stun_sent_time == -1) ||
	    ((d->stun_forced_enabled || ((f->ticker->time - d->last_sent_time) > last_sent_timeout)) &&
	     (f->ticker->time - d->last_rtp_stun_sent_time) >= 500)) {
		/* send stun packet every 500 ms:
		 * - in absence of any RTP packet for more than last_sent_timeout
		 * - or always in "forced" mode */
		d->last_rtp_stun_sent_time = f->ticker->time;
		send_stun_packet(d, TRUE, FALSE);
	}

	if (rtp_session_rtcp_enabled(s) &&
	    ((d->last_rtcp_stun_sent_time == -1) || (rtp_session_get_stats(s)->recv_rtcp_packets ==
	                                                 0 /*no need to send stun packets if rtcp packet already received*/
	                                             && (f->ticker->time - d->last_rtcp_stun_sent_time) >= 500))) {
		d->last_rtcp_stun_sent_time = f->ticker->time;
		send_stun_packet(d, FALSE, TRUE);
	}
}

static mblk_t *create_packet_with_volume_data_at_intervals(MSFilter *f) {
	SenderData *d = (SenderData *)f->data;
	RtpSession *s = d->session;
	rtp_audio_level_t *audio_levels = NULL;
	mblk_t *header = NULL;

	// Mixer to client volumes
	if (d->mixer_to_client_extension_id > 0 && d->mtc_request_data_cb) {
		if (d->mtc_volumes_left_to_send > 0) {
			rtp_audio_level_t *tmp = d->mtc_volumes + (d->mtc_volumes_size - d->mtc_volumes_left_to_send);

			if (d->mtc_volumes_left_to_send <= RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL) {
				header = rtp_session_create_packet_header_with_mixer_to_client_audio_level(
				    s, 0, d->mixer_to_client_extension_id, d->mtc_volumes_left_to_send, tmp);
				ms_free(d->mtc_volumes);
				d->mtc_volumes = NULL;
				d->mtc_volumes_left_to_send = 0;
			} else {
				header = rtp_session_create_packet_header_with_mixer_to_client_audio_level(
				    s, 0, d->mixer_to_client_extension_id, RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL, tmp);
				d->mtc_volumes_left_to_send -= RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL;
			}

			return header;
		}

		if (f->ticker->time - d->last_audio_level_sent_time > (uint64_t)d->audio_level_send_interval) {
			int size = d->mtc_request_data_cb(f, &audio_levels, d->mtc_request_data_user_data);
			if (audio_levels != NULL) {
				if (size <= RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL) {
					header = rtp_session_create_packet_header_with_mixer_to_client_audio_level(
					    s, 0, d->mixer_to_client_extension_id, size, audio_levels);
					ms_free(audio_levels);
				} else {
					header = rtp_session_create_packet_header_with_mixer_to_client_audio_level(
					    s, 0, d->mixer_to_client_extension_id, RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL, audio_levels);
					d->mtc_volumes = audio_levels;
					d->mtc_volumes_size = size;
					d->mtc_volumes_left_to_send = size - RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL;
				}
			}

			d->last_audio_level_sent_time = f->ticker->time;
		}
	}

	// Client to mixer volume
	if (d->client_to_mixer_extension_id > 0 && d->ctm_request_data_cb) {
		// If we have voice activity, we send the volume without interval
		if (d->voice_activity ||
		    f->ticker->time - d->last_audio_level_sent_time > (uint64_t)d->audio_level_send_interval) {
			int volume = d->ctm_request_data_cb(f, d->ctm_request_data_user_data);
			if (!header)
				header = rtp_session_create_packet_header(
				    s, 8); // allocate 8 more bytes than needed so we can add an extension
				           // header with client to mixer extension without reallocating
			rtp_add_client_to_mixer_audio_level(header, d->client_to_mixer_extension_id, d->voice_activity, volume);

			d->last_audio_level_sent_time = f->ticker->time;
		}
	}

	return header ? header : rtp_session_create_packet_header(s, 0);
}

static void process_cn(MSFilter *f, SenderData *d, uint32_t timestamp, mblk_t *im) {
	if (d->cng_data.datasize > 0) {
		rtp_header_t *rtp;
		/* get CN payload type number */
		int cn_pt = rtp_profile_find_payload_number(d->session->snd.profile, "CN", 8000, 1);

		/* create the packet, payload type number is the one used for current codec */
		mblk_t *m = create_packet_with_volume_data_at_intervals(f);
		m->b_cont = rtp_create_packet(d->cng_data.data, d->cng_data.datasize);
		mblk_meta_copy(im, m);
		/* replace payload type in RTP header */
		rtp = (rtp_header_t *)m->b_rptr;
		rtp->paytype = cn_pt;

		rtp_session_sendm_with_ts(d->session, m, timestamp);
		d->cng_data.datasize = 0;
	}
}

static void compute_processing_delay_stats(MSFilter *f, mblk_t *im) {
	SenderData *d = (SenderData *)f->data;
	uint64_t source_ts = (mblk_get_timestamp_info(im) * 1000ULL) / (uint64_t)d->rate;
	int delay = (int)(f->ticker->time - source_ts);
	ms_box_plot_add_value(&d->processing_delay_stats, delay);
}

static void print_processing_delay_stats(SenderData *d) {
	int payloadId = d->session->snd.pt;
	PayloadType *pt = d->session->snd.profile->payload[payloadId];
	if (pt && pt->type == PAYLOAD_VIDEO) {
		char *box_plot_str = ms_box_plot_to_string(&d->processing_delay_stats, "ms");
		ms_message("video processing delay = %s (RtpSession=%p)", box_plot_str, d->session);
		ms_box_plot_reset(&d->processing_delay_stats);
		bctbx_free(box_plot_str);
	}
}

static void add_frame_marking_extension(SenderData *d, mblk_t *header, mblk_t *im) {
	uint8_t marker = 0;

	if (d->frame_start) {
		marker |= RTP_FRAME_MARKER_START;
		d->frame_start = FALSE;
	}

	if (mblk_get_marker_info(im)) {
		marker |= RTP_FRAME_MARKER_END;
		d->frame_start = TRUE;
	}

	if (mblk_get_independent_flag(im)) marker |= RTP_FRAME_MARKER_INDEPENDENT;
	if (mblk_get_discardable_flag(im)) marker |= RTP_FRAME_MARKER_DISCARDABLE;

	rtp_add_frame_marker(header, d->frame_marking_extension_id, marker);
}

static void _sender_process(MSFilter *f) {
	SenderData *d = (SenderData *)f->data;
	RtpSession *s = d->session;
	mblk_t *im;
	uint32_t timestamp = 0;

	if (d->relay_session_id_size > 0 && ((f->ticker->time - d->last_rsi_time) > 5000 || d->last_rsi_time == 0)) {
		ms_message("relay session id sent in RTCP APP");
		rtp_session_send_rtcp_APP(s, 0, "RSID", (const uint8_t *)d->relay_session_id, d->relay_session_id_size);
		d->last_rsi_time = f->ticker->time;
	}

	ms_filter_lock(f);
	im = ms_queue_get(f->inputs[0]);
	do {
		mblk_t *header = NULL;
		timestamp = get_cur_timestamp(f, im);

		if (d->dtmf != 0 && !d->skip) {
			ms_debug("prepare to send RFC2833 dtmf.");
			d->skip_until = timestamp + d->dtmf_duration;
			d->dtmf_ts_cur = timestamp;
			d->skip = TRUE;
		}
		if (d->skip) {
			uint32_t origin_ts = d->skip_until - d->dtmf_duration;
			if (RTP_TIMESTAMP_IS_NEWER_THAN(timestamp, d->dtmf_ts_cur)) {
				ms_debug("Sending RFC2833 packet, start_timestamp=%u, dtmf_ts_cur=%u", origin_ts, d->dtmf_ts_cur);
				if (send_dtmf(f, origin_ts) == -1) {
					/* Abort the sending of dtmf if an error occured */
					d->dtmf = 0;
					d->skip = FALSE;
				}
			}
		}
		if (im) {
			compute_processing_delay_stats(f, im);

			if (d->rtp_transfer_mode) {
				rtp_session_sendm_with_ts(s, im, timestamp);
				continue;
			}

			if (d->skip == FALSE && d->mute == FALSE) {
				header = create_packet_with_volume_data_at_intervals(f);
				rtp_set_markbit(header, mblk_get_marker_info(im));
				if (d->frame_marking_extension_id > 0) add_frame_marking_extension(d, header, im);
				header->b_cont = im;
				mblk_meta_copy(im, header);
				rtp_session_sendm_with_ts(s, header, timestamp);
			} else if (d->mute == TRUE && d->skip == FALSE) {
				process_cn(f, d, timestamp, im);
				freemsg(im);
				// Send STUN packet as RTP keep alive
				check_stun_sending(f);
			} else {
				freemsg(im);
			}
		} else if (d->skip == FALSE) {
			// Send STUN packet as RTP keep alive even if there is no input
			check_stun_sending(f);
		}
	} while ((im = ms_queue_get(f->inputs[0])) != NULL);

	if (d->last_sent_time == -1) {
		check_stun_sending(f);
	}

	/*every second, compute output bandwidth*/
	if (f->ticker->time % 1000 == 0) rtp_session_compute_send_bandwidth(d->session);

	if (f->ticker->time % 5000 == 0) {
		print_processing_delay_stats(d);
		ms_box_plot_reset(&d->processing_delay_stats);
	}
	ms_filter_unlock(f);
}

static void sender_process(MSFilter *f) {
	SenderData *d = (SenderData *)f->data;
	RtpSession *s = d->session;
	if (s == NULL) {
		ms_queue_flush(f->inputs[0]);
		return;
	}
	if (d->use_task) ms_filter_postpone_task(f, _sender_process);
	else _sender_process(f);
}

static int sender_send_generic_cn(MSFilter *f, void *data) {
	SenderData *d = (SenderData *)f->data;
	ms_filter_lock(f);
	memcpy(&d->cng_data, data, sizeof(MSCngData));
	ms_filter_unlock(f);
	return 0;
}

static int sender_enable_stun(MSFilter *f, void *data) {
	SenderData *d = (SenderData *)f->data;
	d->stun_enabled = *((bool_t *)data);
	return 0;
}

static int sender_enable_stun_forced(MSFilter *f, void *data) {
	SenderData *d = (SenderData *)f->data;
	d->stun_forced_enabled = *((bool_t *)data);
	return 0;
}

static int get_sender_output_fmt(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	MSPinFormat *pinFmt = (MSPinFormat *)arg;
	PayloadType *pt =
	    rtp_profile_get_payload(rtp_session_get_profile(d->session), rtp_session_get_send_payload_type(d->session));
	pinFmt->fmt = ms_factory_get_audio_format(f->factory, pt->mime_type, pt->clock_rate, pt->channels, NULL);
	return 0;
}

static int enable_ts_adjustment(MSFilter *f, void *data) {
	SenderData *d = (SenderData *)f->data;
	d->enable_ts_adjustment = *(bool_t *)data;
	return 0;
}

static int sender_enable_rtp_transfer_mode(MSFilter *f, void *data) {
	SenderData *d = (SenderData *)f->data;
	d->rtp_transfer_mode = *(bool_t *)data;
	return 0;
}

static int sender_set_active_speaker_ssrc(MSFilter *f, void *data) {
	SenderData *d = (SenderData *)f->data;
	uint32_t csrc = *(uint32_t *)data;
	bool_t was_set = FALSE;

	ms_filter_lock(f);
	if (d->session) {
		rtp_session_clear_contributing_sources(d->session);
		if (csrc != 0) rtp_session_add_contributing_source(d->session, csrc, "", NULL, NULL, NULL, NULL, NULL, NULL);
		was_set = TRUE;
	}
	ms_filter_unlock(f);
	if (!was_set) {
		ms_error("sender_set_active_speaker_ssrc(): could not be set because no RtpSession was assigned yet.");
	}
	return 0;
}

static int sender_set_voice_activity(MSFilter *f, void *data) {
	SenderData *d = (SenderData *)f->data;
	bool_t detected = *(bool_t *)data;
	d->voice_activity = detected;
	return 0;
}

static MSFilterMethod sender_methods[] = {
    {MS_RTP_SEND_MUTE, sender_mute},
    {MS_RTP_SEND_UNMUTE, sender_unmute},
    {MS_RTP_SEND_SET_SESSION, sender_set_session},
    {MS_RTP_SEND_SEND_DTMF, sender_send_dtmf},
    {MS_RTP_SEND_SET_RELAY_SESSION_ID, sender_set_relay_session_id},
    {MS_RTP_SEND_SET_AUDIO_LEVEL_SEND_INTERVAL, sender_set_audio_level_send_interval},
    {MS_RTP_SEND_SET_MIXER_TO_CLIENT_EXTENSION_ID, sender_set_mixer_to_client_extension_id},
    {MS_RTP_SEND_SET_MIXER_TO_CLIENT_DATA_REQUEST_CB, sender_set_mixer_to_client_data_request_cb},
    {MS_RTP_SEND_SET_CLIENT_TO_MIXER_EXTENSION_ID, sender_set_client_to_mixer_extension_id},
    {MS_RTP_SEND_SET_CLIENT_TO_MIXER_DATA_REQUEST_CB, sender_set_client_to_mixer_data_request_cb},
    {MS_RTP_SEND_SET_FRAME_MARKING_EXTENSION_ID, sender_set_frame_marking_extension_id},
    {MS_FILTER_GET_SAMPLE_RATE, sender_get_sr},
    {MS_FILTER_GET_NCHANNELS, sender_get_ch},
    {MS_RTP_SEND_SET_DTMF_DURATION, sender_set_dtmf_duration},
    {MS_RTP_SEND_SEND_GENERIC_CN, sender_send_generic_cn},
    {MS_RTP_SEND_ENABLE_STUN, sender_enable_stun},
    {MS_FILTER_GET_OUTPUT_FMT, get_sender_output_fmt},
    {MS_RTP_SEND_ENABLE_TS_ADJUSTMENT, enable_ts_adjustment},
    {MS_RTP_SEND_ENABLE_STUN_FORCED, sender_enable_stun_forced},
    {MS_RTP_SEND_ENABLE_RTP_TRANSFER_MODE, sender_enable_rtp_transfer_mode},
    {MS_RTP_SEND_SET_ACTIVE_SPEAKER_SSRC, sender_set_active_speaker_ssrc},
    {MS_RTP_SEND_TELEPHONE_EVENT_SUPPORTED, sender_telephone_event_supported},
    {MS_RTP_SEND_SET_VOICE_ACTIVITY, sender_set_voice_activity},
    {0, NULL}};

#ifdef _MSC_VER

MSFilterDesc ms_rtp_send_desc = {MS_RTP_SEND_ID,
                                 "MSRtpSend",
                                 N_("RTP output filter"),
                                 MS_FILTER_OTHER,
                                 NULL,
                                 1,
                                 0,
                                 sender_init,
                                 NULL,
                                 sender_process,
                                 NULL,
                                 sender_uninit,
                                 sender_methods,
                                 MS_FILTER_IS_PUMP};

#else

MSFilterDesc ms_rtp_send_desc = {.id = MS_RTP_SEND_ID,
                                 .name = "MSRtpSend",
                                 .text = N_("RTP output filter"),
                                 .category = MS_FILTER_OTHER,
                                 .ninputs = 1,
                                 .noutputs = 0,
                                 .init = sender_init,
                                 .process = sender_process,
                                 .uninit = sender_uninit,
                                 .methods = sender_methods,
                                 .flags = MS_FILTER_IS_PUMP};

#endif

struct ReceiverData {
	RtpSession *session;
	int current_pt;
	int rate;
	bool_t starting;
	bool_t reset_jb;
	int mixer_to_client_extension_id;
	int client_to_mixer_extension_id;
	bool_t rtp_transfer_mode;
	bool_t csrc_events_enabled;
	uint32_t last_csrc;
};

typedef struct ReceiverData ReceiverData;

static void receiver_init(MSFilter *f) {
	ReceiverData *d = (ReceiverData *)ms_new0(ReceiverData, 1);
	d->session = NULL;
	d->rate = 8000;
	d->mixer_to_client_extension_id = 0;
	d->client_to_mixer_extension_id = 0;
	d->rtp_transfer_mode = FALSE;
	d->csrc_events_enabled = FALSE;
	d->last_csrc = 0;
	f->data = d;
}

static void receiver_postprocess(MSFilter *f) {
	ReceiverData *d = (ReceiverData *)f->data;
	rtp_session_reset_recvfrom(d->session);
}

static void receiver_uninit(MSFilter *f) {
	ReceiverData *d = (ReceiverData *)f->data;
	ms_free(d);
}

static int receiver_set_session(MSFilter *f, void *arg) {
	ReceiverData *d = (ReceiverData *)f->data;
	RtpSession *s = (RtpSession *)arg;
	PayloadType *pt;
	d->current_pt = rtp_session_get_recv_payload_type(s);
	pt = rtp_profile_get_payload(rtp_session_get_profile(s), d->current_pt);
	if (pt != NULL) {
		d->rate = pt->clock_rate;
	} else {
		ms_warning("receiver_set_session(): receiving undefined payload type %i ?",
		           rtp_session_get_recv_payload_type(s));
	}
	d->session = s;

	return 0;
}

static int receiver_set_mixer_to_client_extension_id(MSFilter *f, void *arg) {
	ReceiverData *d = (ReceiverData *)f->data;
	int *id = (int *)arg;
	d->mixer_to_client_extension_id = *id;
	return 0;
}

static int receiver_set_client_to_mixer_extension_id(MSFilter *f, void *arg) {
	ReceiverData *d = (ReceiverData *)f->data;
	int *id = (int *)arg;
	d->client_to_mixer_extension_id = *id;
	return 0;
}

static int receiver_get_sr(MSFilter *f, void *arg) {
	ReceiverData *d = (ReceiverData *)f->data;
	PayloadType *pt;
	if (d->session == NULL) {
		ms_warning("Could not obtain sample rate, session is not set.");
		return -1;
	}
	pt = rtp_profile_get_payload(rtp_session_get_profile(d->session), rtp_session_get_recv_payload_type(d->session));
	if (pt != NULL) {
		if (strcasecmp(pt->mime_type, "G722") == 0) *(int *)arg = 16000;
		else *(int *)arg = pt->clock_rate;
	} else {
		ms_warning("Could not obtain sample rate, payload type is unknown.");
		return -1;
	}
	return 0;
}

static int receiver_get_ch(MSFilter *f, void *arg) {
	ReceiverData *d = (ReceiverData *)f->data;
	PayloadType *pt;
	if (d->session == NULL) {
		ms_warning("MSRtpRecv: Could not obtain sample rate, session is not set.");
		return -1;
	}
	pt = rtp_profile_get_payload(rtp_session_get_profile(d->session), rtp_session_get_recv_payload_type(d->session));
	if (pt == NULL) {
		ms_warning("MSRtpRecv: could not obtain number of channels, payload type is unknown.");
		return -1;
	}
	*(int *)arg = pt->channels;
	return 0;
}

static int receiver_reset_jitter_buffer(MSFilter *f, BCTBX_UNUSED(void *arg)) {
	ReceiverData *d = (ReceiverData *)f->data;
	d->reset_jb = TRUE;
	return 0;
}

static void receiver_preprocess(MSFilter *f) {
	ReceiverData *d = (ReceiverData *)f->data;
	d->starting = TRUE;
}

/*returns TRUE if the packet is ok to be sent to output queue*/
static bool_t receiver_check_payload_type(MSFilter *f, ReceiverData *d, mblk_t *m) {
	int ptn = rtp_get_payload_type(m);
	PayloadType *pt;
	if (ptn == d->current_pt) return TRUE;
	pt = rtp_profile_get_payload(rtp_session_get_profile(d->session), ptn);
	if (pt == NULL) {
		ms_warning("Discarding packet with unknown payload type %i", ptn);
		return FALSE;
	}
	if (strcasecmp(pt->mime_type, "CN") == 0) {
		MSCngData cngdata;
		uint8_t *data = NULL;
		int datasize = rtp_get_payload(m, &data);
		if (data) {
			if (datasize <= (int)sizeof(cngdata.data)) {
				memcpy(cngdata.data, data, datasize);
				cngdata.datasize = datasize;
				ms_filter_notify(f, MS_RTP_RECV_GENERIC_CN_RECEIVED, &cngdata);
			} else {
				ms_warning("CN packet has unexpected size %i", datasize);
			}
		}
		return FALSE;
	}
	d->current_pt = ptn;
	return TRUE;
}

static void receiver_check_for_extensions(MSFilter *f, mblk_t *m) {
	ReceiverData *d = (ReceiverData *)f->data;
	rtp_audio_level_t mtc_levels[RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL] = {{0}};
	rtp_audio_level_t ctm_level;
	bool_t voice_activity;
	int ret;

	// Check the packet if it contains audio level extensions
	if (d->mixer_to_client_extension_id > 0) {
		if ((ret = rtp_get_mixer_to_client_audio_level(m, d->mixer_to_client_extension_id, mtc_levels)) != -1) {
			ms_filter_notify(f, MS_RTP_RECV_MIXER_TO_CLIENT_AUDIO_LEVEL_RECEIVED, mtc_levels);
		}
	}

	if (d->client_to_mixer_extension_id > 0) {
		if ((ret = rtp_get_client_to_mixer_audio_level(m, RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL,
		                                               &voice_activity)) != RTP_AUDIO_LEVEL_NO_VOLUME) {
			ctm_level.csrc = rtp_get_ssrc(m);
			ctm_level.dbov = ret;
			ms_filter_notify(f, MS_RTP_RECV_CLIENT_TO_MIXER_AUDIO_LEVEL_RECEIVED, &ctm_level);
		}
	}
}

static void receiver_check_for_csrc_change(MSFilter *f, mblk_t *m) {
	ReceiverData *d = (ReceiverData *)f->data;
	uint32_t csrc = 0;

	if (rtp_get_cc(m) > 0) {
		csrc = rtp_get_csrc(m, 0);
	}

	if (csrc != d->last_csrc) {
		ms_filter_notify(f, MS_RTP_RECV_CSRC_CHANGED, &csrc);
		d->last_csrc = csrc;
	}
}

static void receiver_process(MSFilter *f) {
	ReceiverData *d = (ReceiverData *)f->data;
	mblk_t *m;
	uint32_t timestamp;

	if (d->session == NULL) return;

	if (d->reset_jb) {
		ms_message("Reseting jitter buffer");
		rtp_session_resync(d->session);
		d->reset_jb = FALSE;
	}

	if (d->starting) {
		PayloadType *pt =
		    rtp_profile_get_payload(rtp_session_get_profile(d->session), rtp_session_get_recv_payload_type(d->session));
		if (pt && pt->type != PAYLOAD_VIDEO) rtp_session_flush_sockets(d->session);
		d->starting = FALSE;
	}

	timestamp = (uint32_t)(f->ticker->time * (d->rate / 1000));
	while ((m = rtp_session_recvm_with_ts(d->session, timestamp)) != NULL) {
		if (d->rtp_transfer_mode) {
			ms_queue_put(f->outputs[0], m);
			continue;
		}

		if (receiver_check_payload_type(f, d, m)) {
			mblk_set_timestamp_info(m, rtp_get_timestamp(m));
			mblk_set_marker_info(m, rtp_get_markbit(m));
			mblk_set_cseq(m, rtp_get_seqnumber(m));
			receiver_check_for_extensions(f, m);
			if (d->csrc_events_enabled) receiver_check_for_csrc_change(f, m);
			rtp_get_payload(m, &m->b_rptr);
			ms_queue_put(f->outputs[0], m);
		} else {
			freemsg(m);
		}
	}
}

static int get_receiver_output_fmt(MSFilter *f, void *arg) {
	ReceiverData *d = (ReceiverData *)f->data;
	MSPinFormat *pinFmt = (MSPinFormat *)arg;
	PayloadType *pt =
	    rtp_profile_get_payload(rtp_session_get_profile(d->session), rtp_session_get_send_payload_type(d->session));
	pinFmt->fmt = ms_factory_get_audio_format(f->factory, pt->mime_type, pt->clock_rate, pt->channels, NULL);
	return 0;
}

static int receiver_enable_rtp_transfer_mode(MSFilter *f, void *arg) {
	ReceiverData *d = (ReceiverData *)f->data;
	d->rtp_transfer_mode = *(bool_t *)arg;
	return 0;
}

static int receiver_enable_csrc_events(MSFilter *f, void *arg) {
	ReceiverData *d = (ReceiverData *)f->data;
	d->csrc_events_enabled = *(bool_t *)arg;
	return 0;
}

static MSFilterMethod receiver_methods[] = {
    {MS_RTP_RECV_SET_SESSION, receiver_set_session},
    {MS_RTP_RECV_RESET_JITTER_BUFFER, receiver_reset_jitter_buffer},
    {MS_RTP_RECV_SET_MIXER_TO_CLIENT_EXTENSION_ID, receiver_set_mixer_to_client_extension_id},
    {MS_RTP_RECV_SET_CLIENT_TO_MIXER_EXTENSION_ID, receiver_set_client_to_mixer_extension_id},
    {MS_RTP_RECV_ENABLE_RTP_TRANSFER_MODE, receiver_enable_rtp_transfer_mode},
    {MS_RTP_RECV_ENABLE_CSRC_EVENTS, receiver_enable_csrc_events},
    {MS_FILTER_GET_SAMPLE_RATE, receiver_get_sr},
    {MS_FILTER_GET_NCHANNELS, receiver_get_ch},
    {MS_FILTER_GET_OUTPUT_FMT, get_receiver_output_fmt},
    {0, NULL}};

#ifdef _MSC_VER

MSFilterDesc ms_rtp_recv_desc = {MS_RTP_RECV_ID,
                                 "MSRtpRecv",
                                 N_("RTP input filter"),
                                 MS_FILTER_OTHER,
                                 NULL,
                                 0,
                                 1,
                                 receiver_init,
                                 receiver_preprocess,
                                 receiver_process,
                                 receiver_postprocess,
                                 receiver_uninit,
                                 receiver_methods};

#else

MSFilterDesc ms_rtp_recv_desc = {.id = MS_RTP_RECV_ID,
                                 .name = "MSRtpRecv",
                                 .text = N_("RTP input filter"),
                                 .category = MS_FILTER_OTHER,
                                 .ninputs = 0,
                                 .noutputs = 1,
                                 .init = receiver_init,
                                 .preprocess = receiver_preprocess,
                                 .process = receiver_process,
                                 .postprocess = receiver_postprocess,
                                 .uninit = receiver_uninit,
                                 .methods = receiver_methods};

#endif

MS_FILTER_DESC_EXPORT(ms_rtp_send_desc)
MS_FILTER_DESC_EXPORT(ms_rtp_recv_desc)
