 /*
 * vp8.c -VP8 encoder/decoder wrapper
 *
 *
 * Copyright (C) 2011  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/mscodecutils.h"
#include "vp8rtpfmt.h"

#define PICTURE_ID_ON_16_BITS
/*#define AVPF_DEBUG*/

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>


#define PICID_NEWER_THAN(s1,s2)	( (uint16_t)((uint16_t)s1-(uint16_t)s2) < 1<<15)

#define MS_VP8_CONF(required_bitrate, bitrate_limit, resolution, fps, cpus) \
	{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H }, fps, cpus, NULL }

static const MSVideoConfiguration vp8_conf_list[] = {
#if defined(ANDROID) || (TARGET_OS_IPHONE == 1) || defined(__arm__) || defined(_M_ARM)
	MS_VP8_CONF(2048000, 2560000,       UXGA, 12, 2),
	MS_VP8_CONF(1024000, 1536000, SXGA_MINUS, 12, 2),
	MS_VP8_CONF( 750000, 1024000,        XGA, 12, 2),
	MS_VP8_CONF( 500000,  750000,       SVGA, 12, 2),
	MS_VP8_CONF( 300000,  500000,        VGA, 12, 2),
	MS_VP8_CONF( 100000,  300000,       QVGA, 18, 2),
	MS_VP8_CONF(  64000,  100000,       QCIF, 12, 2),
	MS_VP8_CONF(300000, 600000,          VGA, 12, 1),
	MS_VP8_CONF(100000, 300000,         QVGA, 10, 1),
	MS_VP8_CONF( 64000, 100000,         QCIF, 10, 1),
	MS_VP8_CONF(      0,   64000,       QCIF,  5, 1)
#else
	MS_VP8_CONF(1536000,  2560000, SXGA_MINUS, 25, 4),
	MS_VP8_CONF(800000,  2000000,       720P, 25, 4),
	MS_VP8_CONF(800000,  1536000,        XGA, 25, 4),
	MS_VP8_CONF( 600000,  1024000,       SVGA, 25, 2),
	MS_VP8_CONF( 350000,   600000,        VGA, 25, 2),
	MS_VP8_CONF( 350000,   600000,        VGA, 15, 1),
	MS_VP8_CONF( 200000,   350000,        CIF, 18, 1),
	MS_VP8_CONF( 150000,   200000,       QVGA, 15, 1),
	MS_VP8_CONF( 100000,   150000,       QVGA, 10, 1),
	MS_VP8_CONF(  64000,   100000,       QCIF, 12, 1),
	MS_VP8_CONF(      0,    64000,       QCIF,  5 ,1)
#endif
};

typedef struct EncFrameState {
	vpx_ref_frame_type_t type;
	vpx_codec_pts_t count;
	uint16_t picture_id;
	bool_t acknowledged;
	bool_t is_independant;
} EncFrameState;

typedef struct EncFramesState {
	EncFrameState golden;
	EncFrameState altref;
	EncFrameState reconstruct;
	vpx_codec_pts_t last_independent_frame;
} EncFramesState;

typedef struct EncState {
	vpx_codec_ctx_t codec;
	vpx_codec_enc_cfg_t cfg;
	vpx_codec_pts_t frame_count;
	vpx_codec_iface_t *iface;
	vpx_codec_flags_t flags;
	EncFramesState frames_state;
	Vp8RtpFmtPackerCtx packer;
	MSVideoStarter starter;
	MSVideoConfiguration vconf;
	const MSVideoConfiguration *vconf_list;
	int last_fir_seq_nr;
	uint16_t picture_id;
	uint16_t last_sli_id;
	bool_t force_keyframe;
	bool_t invalid_frame_reported;
	bool_t avpf_enabled;
	bool_t ready;
} EncState;

#define MIN_KEY_FRAME_DIST 4 /*since one i-frame is allowed to be 4 times bigger of the target bitrate*/

static bool_t should_generate_key_frame(EncState *s, int min_interval);
static void enc_reset_frames_state(EncState *s);

static void enc_init(MSFilter *f) {
	EncState *s = (EncState *)ms_new0(EncState, 1);
	MSVideoSize vsize;

	s->iface = vpx_codec_vp8_cx();
	ms_message("Using %s", vpx_codec_iface_name(s->iface));

	s->vconf_list = &vp8_conf_list[0];
	MS_VIDEO_SIZE_ASSIGN(vsize, CIF);
	s->vconf = ms_video_find_best_configuration_for_size(s->vconf_list, vsize, ms_factory_get_cpu_count(f->factory));
	s->frame_count = 0;
	s->last_fir_seq_nr = -1;
#ifdef PICTURE_ID_ON_16_BITS
	s->picture_id = (ortp_random() & 0x7FFF) | 0x8000;
#else
	s->picture_id = ortp_random() & 0x007F;
#endif
	s->avpf_enabled = FALSE;
	enc_reset_frames_state(s);
	f->data = s;
}

static void enc_uninit(MSFilter *f) {
	EncState *s = (EncState *)f->data;
	ms_free(s);
}

static int enc_get_ref_frames_interval(EncState *s) {
	return (uint16_t)(s->vconf.fps * 3); /*s->vconf.fps is dynamic*/
}
static void enc_reset_frames_state(EncState *s){
	memset(&s->frames_state, 0, sizeof(s->frames_state));
	s->frames_state.altref.type=VP8_ALTR_FRAME;
	s->frames_state.golden.type=VP8_GOLD_FRAME;
	s->frames_state.reconstruct.type=VP8_LAST_FRAME;
}

static void enc_preprocess(MSFilter *f) {
	EncState *s = (EncState *)f->data;
	vpx_codec_err_t res;
	vpx_codec_caps_t caps;
	int cpuused=0;

	/* Populate encoder configuration */
	s->flags = 0;
	caps = vpx_codec_get_caps(s->iface);
	if ((s->avpf_enabled == TRUE) && (caps & VPX_CODEC_CAP_OUTPUT_PARTITION)) {
		s->flags |= VPX_CODEC_USE_OUTPUT_PARTITION;
	}
	res = vpx_codec_enc_config_default(s->iface, &s->cfg, 0);
	if (res) {
		ms_error("Failed to get config: %s", vpx_codec_err_to_string(res));
		return;
	}
	s->cfg.rc_target_bitrate = (unsigned int)(((float)s->vconf.required_bitrate) * 0.92f / 1024.0f); //0.92=take into account IP/UDP/RTP overhead, in average.
	s->cfg.g_pass = VPX_RC_ONE_PASS; /* -p 1 */
	s->cfg.g_timebase.num = 1;
	s->cfg.g_timebase.den = (int)s->vconf.fps;
	s->cfg.rc_end_usage = VPX_CBR; /* --end-usage=cbr */
	if (s->avpf_enabled == TRUE) {
		s->cfg.kf_mode = VPX_KF_DISABLED;
	} else {
		s->cfg.kf_mode = VPX_KF_AUTO; /* encoder automatically places keyframes */
		s->cfg.kf_max_dist = 10 * s->cfg.g_timebase.den; /* 1 keyframe each 10s. */
	}
#if TARGET_IPHONE_SIMULATOR
	s->cfg.g_threads = 1; /*workaround to remove crash on ipad simulator*/
#else
	s->cfg.g_threads = ms_factory_get_cpu_count(f->factory);
#endif
	ms_message("VP8 g_threads=%d", s->cfg.g_threads);
	s->cfg.rc_undershoot_pct = 95; /* --undershoot-pct=95 */
	s->cfg.g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT|VPX_ERROR_RESILIENT_PARTITIONS;
	s->cfg.g_lag_in_frames = 0;


#if defined(ANDROID) || (TARGET_OS_IPHONE == 1) || defined(__arm__) || defined(_M_ARM)
	cpuused = 10 - s->cfg.g_threads; /*cpu/quality tradeoff: positive values decrease CPU usage at the expense of quality*/
	if (cpuused < 7) cpuused = 7; /*values beneath 7 consume too much CPU*/
	if( s->cfg.g_threads == 1 ){
		/* on mono-core iOS devices, we reduce the quality a bit more due to VP8 being slower with new Clang compilers */
		cpuused = 16;
	}
#endif

	s->cfg.g_w = s->vconf.vsize.width;
	s->cfg.g_h = s->vconf.vsize.height;

	/* Initialize codec */
	res =  vpx_codec_enc_init(&s->codec, s->iface, &s->cfg, s->flags);
	if (res) {
		ms_error("vpx_codec_enc_init failed: %s (%s)", vpx_codec_err_to_string(res), vpx_codec_error_detail(&s->codec));
		return;
	}
	vpx_codec_control(&s->codec, VP8E_SET_CPUUSED, cpuused);
	vpx_codec_control(&s->codec, VP8E_SET_STATIC_THRESHOLD, 0);
	vpx_codec_control(&s->codec, VP8E_SET_ENABLEAUTOALTREF, !s->avpf_enabled);
	vpx_codec_control(&s->codec, VP8E_SET_MAX_INTRA_BITRATE_PCT, 400); /*limite iFrame size to 4 pframe*/
	if (s->flags & VPX_CODEC_USE_OUTPUT_PARTITION) {
		vpx_codec_control(&s->codec, VP8E_SET_TOKEN_PARTITIONS, 2); /* Output 4 partitions per frame */
	} else {
		vpx_codec_control(&s->codec, VP8E_SET_TOKEN_PARTITIONS, 0);
	}

	s->invalid_frame_reported = FALSE;
	vp8rtpfmt_packer_init(&s->packer);
	if (s->avpf_enabled == TRUE) {
		s->force_keyframe = TRUE;
	} else if (s->frame_count == 0) {
		ms_video_starter_init(&s->starter);
	}
	s->ready = TRUE;
}

static vpx_codec_pts_t enc_last_reference_frame_count(EncState *s) {
	return MAX(s->frames_state.golden.count, s->frames_state.altref.count);
}

static bool_t enc_should_generate_reference_frame(EncState *s) {
	return ((s->frame_count - enc_last_reference_frame_count(s)) ==enc_get_ref_frames_interval(s)) ? TRUE : FALSE;
}

static uint8_t enc_get_type_of_reference_frame_to_generate(EncState *s) {
	if ((s->frames_state.golden.acknowledged == FALSE) && (s->frames_state.altref.acknowledged == FALSE)) {
		/* Generate a keyframe again. */
		return VP8_GOLD_FRAME | VP8_ALTR_FRAME;
	} else if (s->frames_state.golden.acknowledged == FALSE) {
		/* Last golden frame has not been acknowledged, send a new one, so that the encoder can continue to refer to the altref */
		return VP8_GOLD_FRAME;
	} else if (s->frames_state.altref.acknowledged == FALSE) {
		/* Last altref frame has not been acknowledged, send a new one, so that the encoder can continue to refer to the golden  */
		return VP8_ALTR_FRAME;
	} else {
		/* Both golden and altref frames have been acknowledged. */
		if (s->frames_state.golden.count < s->frames_state.altref.count)
			return VP8_GOLD_FRAME;
		if (s->frames_state.golden.count > s->frames_state.altref.count)
			return VP8_ALTR_FRAME;
		return VP8_GOLD_FRAME; /* Send a golden frame after a keyframe. */
	}
}

static void check_most_recent(EncFrameState **found, vpx_codec_pts_t *most_recent, vpx_ref_frame_type_t type,
			      EncFrameState *state, bool_t is_acknoledged){
	if (is_acknoledged && !state->acknowledged) return;
	if (*most_recent<state->count){
		*most_recent=state->count;
		*found=state;
	}
}

static EncFrameState * enc_get_most_recent_reference_frame(EncState *s, bool_t only_acknoledged){
	EncFrameState *found=NULL;
	vpx_codec_pts_t most_recent=0;
	check_most_recent(&found,&most_recent,VP8_GOLD_FRAME,&s->frames_state.golden,only_acknoledged);
	check_most_recent(&found,&most_recent,VP8_ALTR_FRAME,&s->frames_state.altref,only_acknoledged);
	check_most_recent(&found,&most_recent,VP8_LAST_FRAME,&s->frames_state.reconstruct,only_acknoledged);
	return found;
}

static EncFrameState *enc_get_reference_frame(EncState *s, vpx_ref_frame_type_t ft){
	switch(ft){
		case VP8_LAST_FRAME:
			return &s->frames_state.reconstruct;
		break;
		case VP8_GOLD_FRAME:
			return &s->frames_state.golden;
		break;
		case VP8_ALTR_FRAME:
			return &s->frames_state.altref;
		break;
	}
	return NULL;
}

static void enc_mark_reference_frame_as_sent(EncState *s, vpx_ref_frame_type_t ft) {
	switch (ft) {
		case VP8_GOLD_FRAME:
			s->frames_state.golden.count = s->frame_count;
			s->frames_state.golden.picture_id = s->picture_id;
			s->frames_state.golden.acknowledged = FALSE;
			break;
		case VP8_ALTR_FRAME:
			s->frames_state.altref.count = s->frame_count;
			s->frames_state.altref.picture_id = s->picture_id;
			s->frames_state.altref.acknowledged = FALSE;
			break;
		case VP8_LAST_FRAME:
			s->frames_state.reconstruct.count = s->frame_count;
			s->frames_state.reconstruct.picture_id = s->picture_id;
			s->frames_state.reconstruct.acknowledged = FALSE;
			break;
	}
}

static bool_t is_reference_sane(EncState *s, EncFrameState *fs){
	int diff=fs->picture_id-s->last_sli_id;
	bool_t ret;

	if (!fs->is_independant && diff<enc_get_ref_frames_interval(s)){
		ret=FALSE;
	}else ret=TRUE;
	return ret;
}

static bool_t is_reconstruction_frame_sane(EncState *s, unsigned int flags){
	if ( !(flags & VP8_EFLAG_NO_REF_ARF) && !(flags & VP8_EFLAG_NO_REF_GF)){
		return s->frames_state.altref.acknowledged && s->frames_state.golden.acknowledged;
	}else if ((flags & VP8_EFLAG_NO_REF_GF) && !(flags & VP8_EFLAG_NO_REF_ARF)){
		return s->frames_state.altref.acknowledged;
	}else if ((flags & VP8_EFLAG_NO_REF_ARF) && !(flags & VP8_EFLAG_NO_REF_GF)){
		return s->frames_state.golden.acknowledged;
	}
	return FALSE;
}

static void enc_acknowledge_reference_frame(EncState *s, uint16_t picture_id) {
	if (s->frames_state.golden.picture_id == picture_id && is_reference_sane(s,&s->frames_state.golden) ) {
		s->frames_state.golden.acknowledged = TRUE;
	}
	if (s->frames_state.altref.picture_id == picture_id && is_reference_sane(s,&s->frames_state.altref)) {
		s->frames_state.altref.acknowledged = TRUE;
	}
	/*if not gold or altref, it is a "last frame"*/
	if (s->frames_state.reconstruct.picture_id == picture_id ) {
		s->frames_state.reconstruct.acknowledged = TRUE;
	}else if (PICID_NEWER_THAN(s->frames_state.reconstruct.picture_id,picture_id)){
		s->frames_state.reconstruct.acknowledged = TRUE;
	}
}

static bool_t enc_is_reference_frame_acknowledged(EncState *s, vpx_ref_frame_type_t ft) {
	switch (ft) {
		case VP8_GOLD_FRAME:
			return s->frames_state.golden.acknowledged;
		case VP8_ALTR_FRAME:
			return s->frames_state.altref.acknowledged;
		case VP8_LAST_FRAME:
			return s->frames_state.reconstruct.acknowledged;
	}
	return FALSE;
}

static void enc_fill_encoder_flags(EncState *s, unsigned int *flags) {
	uint8_t frame_type;

	if (s->force_keyframe == TRUE) {
		*flags = VPX_EFLAG_FORCE_KF;
		s->invalid_frame_reported = FALSE;
		return;
	}

	if (s->invalid_frame_reported == TRUE) {
		s->invalid_frame_reported = FALSE;
		/* If an invalid frame has been reported. */
		if ((enc_is_reference_frame_acknowledged(s, VP8_GOLD_FRAME) != TRUE)
			&& (enc_is_reference_frame_acknowledged(s, VP8_ALTR_FRAME) != TRUE && should_generate_key_frame(s,MIN_KEY_FRAME_DIST))) {
			/* No reference frame has been acknowledged and last frame can not be used
				as reference. Therefore, generate a new keyframe. */
			*flags = VPX_EFLAG_FORCE_KF;
			return;
		}
		/* Do not reference the last frame. */
		*flags = VP8_EFLAG_NO_REF_LAST;
	} else {
		*flags = 0;
	}
	if (enc_should_generate_reference_frame(s) == TRUE) {
		EncFrameState *last;
		EncFrameState *newref;
		frame_type = enc_get_type_of_reference_frame_to_generate(s);
		if ((frame_type & VP8_GOLD_FRAME) && (frame_type & VP8_ALTR_FRAME)) {
			*flags = VPX_EFLAG_FORCE_KF;
		} else if (frame_type & VP8_GOLD_FRAME) {
			*flags |= (VP8_EFLAG_FORCE_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF);
		} else if (frame_type & VP8_ALTR_FRAME) {
			*flags |= (VP8_EFLAG_FORCE_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_REF_ARF);
			if (s->frame_count > s->frames_state.last_independent_frame + 5*enc_get_ref_frames_interval(s)){
				/*force an independant alt ref frame to force picture to be refreshed completely, otherwise
				 * pixel color saturation appears due to accumulation of small predictive errors*/
				*flags |= VP8_EFLAG_NO_REF_LAST | VP8_EFLAG_NO_REF_GF;
				ms_message("Forcing independant altref frame.");
			}
		}
		if (!(*flags & VPX_EFLAG_FORCE_KF)){
			last=enc_get_most_recent_reference_frame(s,FALSE);
			newref=enc_get_reference_frame(s,frame_type);
			if (last && last->type==VP8_LAST_FRAME){
				/* if the last reference frame wasn't gold or altref, don't reference it*/
				*flags |= VP8_EFLAG_NO_REF_LAST;
			}
			if (newref) {
				newref->is_independant=!!(*flags & VP8_EFLAG_NO_REF_LAST);
			}
		}
	}

	if (s->frames_state.golden.count > s->frames_state.altref.count){
		if (!s->frames_state.golden.acknowledged){
			*flags |= VP8_EFLAG_NO_REF_GF;
		}
	}else if (s->frames_state.golden.count < s->frames_state.altref.count){
		if (!s->frames_state.altref.acknowledged){
			*flags |= VP8_EFLAG_NO_REF_ARF;
		}
	}
}

static bool_t is_frame_independent(unsigned int flags){
	if (flags & VPX_EFLAG_FORCE_KF) return TRUE;

	if ((flags & VP8_EFLAG_FORCE_GF) || (flags & VP8_EFLAG_FORCE_ARF)){
		if ((flags & VP8_EFLAG_NO_REF_ARF) && (flags & VP8_EFLAG_NO_REF_LAST) && (flags & VP8_EFLAG_NO_REF_GF))
			return TRUE;
	}
	return FALSE;
}

static void enc_process(MSFilter *f) {
	mblk_t *im;
	uint64_t timems = f->ticker->time;
	uint32_t timestamp = (uint32_t)(timems*90);
	EncState *s = (EncState *)f->data;
	unsigned int flags = 0;
	vpx_codec_err_t err;
	MSPicture yuv;
	bool_t is_ref_frame=FALSE;

	ms_filter_lock(f);

#ifdef AVPF_DEBUG
	ms_message("VP8 enc_process:");
#endif

	if (!s->ready) {
		ms_queue_flush(f->inputs[0]);
		ms_filter_unlock(f);
		return;
	}

	if ((im = ms_queue_peek_last(f->inputs[0])) != NULL) {
		vpx_image_t img;

		flags = 0;
		ms_yuv_buf_init_from_mblk(&yuv, im);
		vpx_img_wrap(&img, VPX_IMG_FMT_I420, s->vconf.vsize.width, s->vconf.vsize.height, 1, yuv.planes[0]);

		if ((s->avpf_enabled != TRUE) && ms_video_starter_need_i_frame(&s->starter, f->ticker->time)) {
			s->force_keyframe = TRUE;
		}
		if (s->force_keyframe == TRUE) {
			ms_message("Forcing vp8 key frame for filter [%p]", f);
			flags = VPX_EFLAG_FORCE_KF;
		} else if (s->avpf_enabled == TRUE) {
			if (s->frame_count == 0) s->force_keyframe = TRUE;
			enc_fill_encoder_flags(s, &flags);
		}

#ifdef AVPF_DEBUG
		ms_message("VP8 encoder frames state:");
		ms_message("\tgolden: count=%" PRIi64 ", picture_id=0x%04x, ack=%s",
			s->frames_state.golden.count, s->frames_state.golden.picture_id, (s->frames_state.golden.acknowledged == TRUE) ? "Y" : "N");
		ms_message("\taltref: count=%" PRIi64 ", picture_id=0x%04x, ack=%s",
			s->frames_state.altref.count, s->frames_state.altref.picture_id, (s->frames_state.altref.acknowledged == TRUE) ? "Y" : "N");
#endif
		err = vpx_codec_encode(&s->codec, &img, s->frame_count, 1, flags, 1000000LL/(2*(int)s->vconf.fps)); /*encoder has half a framerate interval to encode*/
		if (err) {
			ms_error("vpx_codec_encode failed : %d %s (%s)\n", err, vpx_codec_err_to_string(err), vpx_codec_error_detail(&s->codec));
		} else {
			vpx_codec_iter_t iter = NULL;
			const vpx_codec_cx_pkt_t *pkt;
			bctbx_list_t *list = NULL;

			/* Update the frames state. */
			is_ref_frame=FALSE;
			if (flags & VPX_EFLAG_FORCE_KF) {
				enc_mark_reference_frame_as_sent(s, VP8_GOLD_FRAME);
				enc_mark_reference_frame_as_sent(s, VP8_ALTR_FRAME);
				s->frames_state.golden.is_independant=TRUE;
				s->frames_state.altref.is_independant=TRUE;
				s->frames_state.last_independent_frame=s->frame_count;
				s->force_keyframe = FALSE;
				is_ref_frame=TRUE;
			}else if (flags & VP8_EFLAG_FORCE_GF) {
				enc_mark_reference_frame_as_sent(s, VP8_GOLD_FRAME);
				is_ref_frame=TRUE;
			}else if (flags & VP8_EFLAG_FORCE_ARF) {
				enc_mark_reference_frame_as_sent(s, VP8_ALTR_FRAME);
				is_ref_frame=TRUE;
			}else if (flags & VP8_EFLAG_NO_REF_LAST) {
				enc_mark_reference_frame_as_sent(s, VP8_LAST_FRAME);
				is_ref_frame=is_reconstruction_frame_sane(s,flags);
			}
			if (is_frame_independent(flags)){
				s->frames_state.last_independent_frame=s->frame_count;
			}

			/* Pack the encoded frame. */
			while( (pkt = vpx_codec_get_cx_data(&s->codec, &iter)) ) {
				if ((pkt->kind == VPX_CODEC_CX_FRAME_PKT) && (pkt->data.frame.sz > 0)) {
					Vp8RtpFmtPacket *packet = ms_new0(Vp8RtpFmtPacket, 1);

					packet->m = allocb(pkt->data.frame.sz, 0);
					memcpy(packet->m->b_wptr, pkt->data.frame.buf, pkt->data.frame.sz);
					packet->m->b_wptr += pkt->data.frame.sz;
					mblk_set_timestamp_info(packet->m, timestamp);
					packet->pd = ms_new0(Vp8RtpFmtPayloadDescriptor, 1);
					packet->pd->start_of_partition = TRUE;
					packet->pd->non_reference_frame = s->avpf_enabled && !is_ref_frame;
					if (s->avpf_enabled == TRUE) {
						packet->pd->extended_control_bits_present = TRUE;
						packet->pd->pictureid_present = TRUE;
						packet->pd->pictureid = s->picture_id;
					} else {
						packet->pd->extended_control_bits_present = FALSE;
						packet->pd->pictureid_present = FALSE;
					}
					if (s->flags & VPX_CODEC_USE_OUTPUT_PARTITION) {
						packet->pd->pid = (uint8_t)pkt->data.frame.partition_id;
						if (!(pkt->data.frame.flags & VPX_FRAME_IS_FRAGMENT)) {
							mblk_set_marker_info(packet->m, TRUE);
						}
					} else {
						packet->pd->pid = 0;
						mblk_set_marker_info(packet->m, TRUE);
					}
					list = bctbx_list_append(list, packet);
				}
			}

#ifdef AVPF_DEBUG
			ms_message("VP8 encoder picture_id=%i ***| %s | %s | %s | %s", (int)s->picture_id,
				(flags & VPX_EFLAG_FORCE_KF) ? "KF " : (flags & VP8_EFLAG_FORCE_GF) ? "GF " :  (flags & VP8_EFLAG_FORCE_ARF) ? "ARF" : "   ",
				(flags & VP8_EFLAG_NO_REF_GF) ? "NOREFGF" : "       ",
				(flags & VP8_EFLAG_NO_REF_ARF) ? "NOREFARF" : "        ",
				(flags & VP8_EFLAG_NO_REF_LAST) ? "NOREFLAST" : "         ");
#endif

			vp8rtpfmt_packer_process(&s->packer, list, f->outputs[0], f->factory);

			/* Handle video starter if AVPF is not enabled. */
			s->frame_count++;
			if ((s->avpf_enabled != TRUE) && (s->frame_count == 1)) {
				ms_video_starter_first_frame(&s->starter, f->ticker->time);
			}

			/* Increment the pictureID. */
			s->picture_id++;
#ifdef PICTURE_ID_ON_16_BITS
			if (s->picture_id == 0)
				s->picture_id = 0x8000;
#else
			if (s->picture_id == 0x0080)
				s->picture_id = 0;
#endif
		}
	}
	ms_filter_unlock(f);
	ms_queue_flush(f->inputs[0]);
}

static void enc_postprocess(MSFilter *f) {
	EncState *s = (EncState *)f->data;
	if (s->ready) vpx_codec_destroy(&s->codec);
	vp8rtpfmt_packer_uninit(&s->packer);
	s->ready = FALSE;
}

static int enc_set_configuration(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	const MSVideoConfiguration *vconf = (const MSVideoConfiguration *)data;
	if (vconf != &s->vconf) memcpy(&s->vconf, vconf, sizeof(MSVideoConfiguration));

	if (s->vconf.required_bitrate > s->vconf.bitrate_limit)
		s->vconf.required_bitrate = s->vconf.bitrate_limit;
	s->cfg.rc_target_bitrate = (unsigned int)(((float)s->vconf.required_bitrate) * 0.92f / 1024.0f); //0.92=take into account IP/UDP/RTP overhead, in average.
	if (s->ready) {
		ms_filter_lock(f);
		enc_postprocess(f);
		enc_preprocess(f);
		ms_filter_unlock(f);
		return 0;
	}

	ms_message("Video configuration set: bitrate=%dbits/s, fps=%f, vsize=%dx%d for encoder [%p]"	, s->vconf.required_bitrate,
		   s->vconf.fps, s->vconf.vsize.width, s->vconf.vsize.height, f);
	return 0;
}

static int enc_set_vsize(MSFilter *f, void *data) {
	MSVideoConfiguration best_vconf;
	MSVideoSize *vs = (MSVideoSize *)data;
	EncState *s = (EncState *)f->data;
	best_vconf = ms_video_find_best_configuration_for_size(s->vconf_list, *vs, ms_factory_get_cpu_count(f->factory));
	s->vconf.vsize = *vs;
	s->vconf.fps = best_vconf.fps;
	s->vconf.bitrate_limit = best_vconf.bitrate_limit;
	enc_set_configuration(f, &s->vconf);
	return 0;
}

static int enc_get_vsize(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	MSVideoSize *vs = (MSVideoSize *)data;
	*vs = s->vconf.vsize;
	return 0;
}

static int enc_set_fps(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	float *fps = (float *)data;
	s->vconf.fps = *fps;
	enc_set_configuration(f, &s->vconf);
	return 0;
}

static int enc_get_fps(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	float *fps = (float *)data;
	*fps = s->vconf.fps;
	return 0;
}

static int enc_get_br(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	*(int *)data = s->vconf.required_bitrate;
	return 0;
}

static int enc_set_br(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	int br = *(int *)data;
	if (s->ready) {
		/* Encoding is already ongoing, do not change video size, only bitrate. */
		s->vconf.required_bitrate = br;
		enc_set_configuration(f, &s->vconf);
	} else {
		MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_bitrate(s->vconf_list, br, ms_factory_get_cpu_count(f->factory));
		enc_set_configuration(f, &best_vconf);
	}
	return 0;
}

static int enc_req_vfu(MSFilter *f, void *unused) {
	EncState *s = (EncState *)f->data;
	s->force_keyframe = TRUE;
	return 0;
}

static bool_t should_generate_key_frame(EncState *s, int min_interval){
	/*as the PLI has no picture id reference, we don't know if it is still current.
	 * Thus we need to avoid to send too many keyframes. Send one and wait for rpsi to arrive first.
	 * The receiver sends PLI for each new frame it receives when it does not have the keyframe*/
	if (s->frame_count > s->frames_state.golden.count + min_interval &&
		s->frame_count > s->frames_state.altref.count + min_interval){
		return TRUE;
	}
	return FALSE;
}

static int enc_notify_pli(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	ms_message("VP8: PLI requested");
	if (should_generate_key_frame(s,MIN_KEY_FRAME_DIST)){
		ms_message("VP8: PLI accepted");
		if (s->avpf_enabled == TRUE) {
			s->invalid_frame_reported = TRUE;
		} else {
			s->force_keyframe = TRUE;
		}
	}
	return 0;
}

static int enc_notify_fir(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	uint8_t seq_nr = *((uint8_t *)data);
	if (seq_nr != s->last_fir_seq_nr) {
		s->force_keyframe = TRUE;
		s->last_fir_seq_nr = seq_nr;
	}
	return 0;
}


static int enc_notify_sli(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	MSVideoCodecSLI *sli = (MSVideoCodecSLI *)data;
	bool_t golden_lost = FALSE;
	bool_t altref_lost = FALSE;
	EncFrameState *fs;
	int diff;
	int most_recent;

	ms_filter_lock(f);
	/* extend the SLI received picture-id (6 bits) to a normal picture id*/
	most_recent=s->picture_id;

	diff=(64 + (int)(most_recent & 0x3F) - (int)(sli->picture_id & 0x3F)) % 64;
	s->last_sli_id=most_recent-diff;
	fs=enc_get_most_recent_reference_frame(s,FALSE);
	ms_message("VP8: receiving SLI with pic id [%i], last-ref=[%i], most recent pic id=[%i]",s->last_sli_id, fs ? (int)fs->picture_id : 0, most_recent);
	if (s->frames_state.golden.picture_id == s->last_sli_id) {
		/* Last golden frame has been lost. */
		golden_lost = TRUE;
	}
	if (s->frames_state.altref.picture_id == s->last_sli_id) {
		/* Last altref frame has been lost. */
		altref_lost = TRUE;
	}
	if ((golden_lost == TRUE) && (altref_lost == TRUE) && should_generate_key_frame(s,MIN_KEY_FRAME_DIST)) {
		/* Last key frame has been lost. */
		s->force_keyframe = TRUE;
	} else {
		if (!fs || PICID_NEWER_THAN(s->last_sli_id,fs->picture_id)) {
			s->invalid_frame_reported = TRUE;
		} else {
#ifdef AVPF_DEBUG
			/* The reported loss is older than the last reference frame, so ignore it. */
			ms_message("VP8: Ignored SLI with picture_id [%i] last-ref=[%i]",(int)s->last_sli_id, (int)fs->picture_id);
#endif
		}
	}
	ms_filter_unlock(f);
	return 0;
}

static int enc_notify_rpsi(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	MSVideoCodecRPSI *rpsi = (MSVideoCodecRPSI *)data;
	uint16_t picture_id;

	if (rpsi->bit_string_len == 8) {
		picture_id = *((uint8_t *)rpsi->bit_string);
	} else if (rpsi->bit_string_len == 16) {
		picture_id = ntohs(*((uint16_t *)rpsi->bit_string));
	} else {
		ms_warning("VP8 invalid RPSI received");
		return -1;
	}
	ms_message("VP8: receiving RPSI for picture_id %u",(unsigned)picture_id);
	enc_acknowledge_reference_frame(s, picture_id);
	return 0;
}

static int enc_get_configuration_list(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	const MSVideoConfiguration **vconf_list = (const MSVideoConfiguration **)data;
	*vconf_list = s->vconf_list;
	return 0;
}

static int enc_set_configuration_list(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	const MSVideoConfiguration **vconf_list = (const MSVideoConfiguration **)data;
	if (*vconf_list == NULL) {
		s->vconf_list = &vp8_conf_list[0];
	} else {
		s->vconf_list = *vconf_list;
	}
	return 0;
}

static int enc_enable_avpf(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	s->avpf_enabled = *((bool_t *)data) ? TRUE : FALSE;
	return 0;
}

static MSFilterMethod enc_methods[] = {
	{ MS_FILTER_SET_VIDEO_SIZE,                enc_set_vsize              },
	{ MS_FILTER_SET_FPS,                       enc_set_fps                },
	{ MS_FILTER_GET_VIDEO_SIZE,                enc_get_vsize              },
	{ MS_FILTER_GET_FPS,                       enc_get_fps                },
	{ MS_FILTER_SET_BITRATE,                   enc_set_br                 },
	{ MS_FILTER_GET_BITRATE,                   enc_get_br                 },
	{ MS_FILTER_REQ_VFU,                       enc_req_vfu                },
	{ MS_VIDEO_ENCODER_REQ_VFU,                enc_req_vfu                },
	{ MS_VIDEO_ENCODER_NOTIFY_PLI,             enc_notify_pli             },
	{ MS_VIDEO_ENCODER_NOTIFY_FIR,             enc_notify_fir             },
	{ MS_VIDEO_ENCODER_NOTIFY_SLI,             enc_notify_sli             },
	{ MS_VIDEO_ENCODER_NOTIFY_RPSI,            enc_notify_rpsi            },
	{ MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST, enc_get_configuration_list },
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION_LIST, enc_set_configuration_list },
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION,      enc_set_configuration      },
	{ MS_VIDEO_ENCODER_ENABLE_AVPF,            enc_enable_avpf            },
	{ 0,                                       NULL                       }
};

#define MS_VP8_ENC_NAME        "MSVp8Enc"
#define MS_VP8_ENC_DESCRIPTION "A VP8 video encoder using libvpx library."
#define MS_VP8_ENC_CATEGORY    MS_FILTER_ENCODER
#define MS_VP8_ENC_ENC_FMT     "VP8"
#define MS_VP8_ENC_NINPUTS     1 /*MS_YUV420P is assumed on this input */
#define MS_VP8_ENC_NOUTPUTS    1
#define MS_VP8_ENC_FLAGS       0

#ifdef _MSC_VER

MSFilterDesc ms_vp8_enc_desc = {
	MS_VP8_ENC_ID,
	MS_VP8_ENC_NAME,
	MS_VP8_ENC_DESCRIPTION,
	MS_VP8_ENC_CATEGORY,
	MS_VP8_ENC_ENC_FMT,
	MS_VP8_ENC_NINPUTS,
	MS_VP8_ENC_NOUTPUTS,
	enc_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	enc_methods,
	MS_VP8_ENC_FLAGS
};

#else

MSFilterDesc ms_vp8_enc_desc = {
	.id = MS_VP8_ENC_ID,
	.name = MS_VP8_ENC_NAME,
	.text = MS_VP8_ENC_DESCRIPTION,
	.category = MS_VP8_ENC_CATEGORY,
	.enc_fmt = MS_VP8_ENC_ENC_FMT,
	.ninputs = MS_VP8_ENC_NINPUTS,
	.noutputs = MS_VP8_ENC_NOUTPUTS,
	.init = enc_init,
	.preprocess = enc_preprocess,
	.process = enc_process,
	.postprocess = enc_postprocess,
	.uninit = enc_uninit,
	.methods = enc_methods,
	.flags = MS_VP8_ENC_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_vp8_enc_desc)


#include <assert.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

typedef struct DecState {
	vpx_codec_ctx_t codec;
	vpx_codec_iface_t *iface;
	vpx_codec_flags_t flags;
	Vp8RtpFmtUnpackerCtx unpacker;
	long last_cseq; /*last receive sequence number, used to locate missing partition fragment*/
	int current_partition_id; /*current partition id*/
	uint64_t last_error_reported_time;
	mblk_t *yuv_msg;
	MSPicture outbuf;
	unsigned int yuv_width, yuv_height;
	MSQueue q;
	MSAverageFPS fps;
	bool_t first_image_decoded;
	bool_t avpf_enabled;
	bool_t freeze_on_error;
	bool_t ready;
} DecState;


static void dec_init(MSFilter *f) {
	DecState *s = (DecState *)ms_new0(DecState, 1);

	s->iface = vpx_codec_vp8_dx();
	ms_message("Using %s", vpx_codec_iface_name(s->iface));

	s->last_error_reported_time = 0;
	s->yuv_width = 0;
	s->yuv_height = 0;
	s->yuv_msg = 0;
	ms_queue_init(&s->q);
	s->first_image_decoded = FALSE;
	s->avpf_enabled = FALSE;
	s->freeze_on_error = TRUE;
	f->data = s;
	ms_average_fps_init(&s->fps, "VP8 decoder: FPS: %f");
}

static int dec_initialize_impl(MSFilter *f){
	DecState *s = (DecState *)f->data;
	vpx_codec_dec_cfg_t cfg;
		
	memset(&cfg, 0, sizeof(cfg));
	cfg.threads = ms_factory_get_cpu_count(f->factory);
	if (vpx_codec_dec_init(&s->codec, s->iface, &cfg, s->flags)){
		ms_error("Failed to initialize VP8 decoder");
		return -1;
	}
	return 0;
}

static void dec_preprocess(MSFilter* f) {
	DecState *s = (DecState *)f->data;
	vpx_codec_caps_t caps = vpx_codec_get_caps(s->iface);

	/* Initialize codec */
	if (!s->ready){
		
		s->flags = 0;
#if 0
		/* Deactivate fragments input for the vpx decoder because it has been broken in libvpx 1.4.
		 * This will have the effect to output complete frames. */
		if ((s->avpf_enabled == TRUE) && (caps & VPX_CODEC_CAP_INPUT_FRAGMENTS)) {
			s->flags |= VPX_CODEC_USE_INPUT_FRAGMENTS;
		}
#endif
		if (caps & VPX_CODEC_CAP_ERROR_CONCEALMENT) {
			s->flags |= VPX_CODEC_USE_ERROR_CONCEALMENT;
		}

		if (dec_initialize_impl(f) != 0) return;
		ms_message("VP8: initializing decoder context: avpf=[%i] freeze_on_error=[%i]",s->avpf_enabled,s->freeze_on_error);
		vp8rtpfmt_unpacker_init(&s->unpacker, f, s->avpf_enabled, s->freeze_on_error, (s->flags & VPX_CODEC_USE_INPUT_FRAGMENTS) ? TRUE : FALSE);
		s->first_image_decoded = FALSE;
		s->ready=TRUE;
	}

}

static void dec_uninit(MSFilter *f) {
	DecState *s = (DecState *)f->data;
	vp8rtpfmt_unpacker_uninit(&s->unpacker);
	vpx_codec_destroy(&s->codec);
	if (s->yuv_msg) freemsg(s->yuv_msg);
	ms_queue_flush(&s->q);
	ms_free(s);
}

static void dec_process(MSFilter *f) {
	DecState *s = (DecState *)f->data;
	mblk_t *im;
	vpx_codec_err_t err;
	vpx_image_t *img;
	vpx_codec_iter_t iter = NULL;
	MSQueue frame;
	MSQueue mtofree_queue;
	Vp8RtpFmtFrameInfo frame_info;
	
	if (!s->ready){
		ms_queue_flush(f->inputs[0]);
		return;
	}
	
	ms_filter_lock(f);

	ms_queue_init(&frame);
	ms_queue_init(&mtofree_queue);

	/* Unpack RTP payload format for VP8. */
	vp8rtpfmt_unpacker_feed(&s->unpacker, f->inputs[0]);

	/* Decode unpacked VP8 frames. */
	while (vp8rtpfmt_unpacker_get_frame(&s->unpacker, &frame, &frame_info) == 0) {
		while ((im = ms_queue_get(&frame)) != NULL) {
			err = vpx_codec_decode(&s->codec, im->b_rptr, (unsigned int)(im->b_wptr - im->b_rptr), NULL, 0);
			if ((s->flags & VPX_CODEC_USE_INPUT_FRAGMENTS) && mblk_get_marker_info(im)) {
				err = vpx_codec_decode(&s->codec, NULL, 0, NULL, 0);
			}
			if (err) {
				ms_warning("vp8 decode failed : %d %s (%s)\n", err, vpx_codec_err_to_string(err), vpx_codec_error_detail(&s->codec)?vpx_codec_error_detail(&s->codec):"no details");
			}
			ms_queue_put(&mtofree_queue, im);
		}

		/* Get decoded frame */
		if ((img = vpx_codec_get_frame(&s->codec, &iter))) {
			int i, j;
			int reference_updates = 0;

			if (vpx_codec_control(&s->codec, VP8D_GET_LAST_REF_UPDATES, &reference_updates) == 0) {
				if (frame_info.pictureid_present && ((reference_updates & VP8_GOLD_FRAME) || (reference_updates & VP8_ALTR_FRAME))) {
					vp8rtpfmt_send_rpsi(&s->unpacker, frame_info.pictureid);
				}
			}

			if (s->yuv_width != img->d_w || s->yuv_height != img->d_h) {
				if (s->yuv_msg) freemsg(s->yuv_msg);
				s->yuv_msg = ms_yuv_buf_alloc(&s->outbuf, img->d_w, img->d_h);
				ms_message("MSVp8Dec: video is %ix%i", img->d_w, img->d_h);
				s->yuv_width = img->d_w;
				s->yuv_height = img->d_h;
				ms_filter_notify_no_arg(f, MS_FILTER_OUTPUT_FMT_CHANGED);
			}

			/* scale/copy frame to destination mblk_t */
			for (i = 0; i < 3; i++) {
				uint8_t *dest = s->outbuf.planes[i];
				uint8_t *src = img->planes[i];
				int h = img->d_h >> ((i > 0) ? 1 : 0);

				for (j = 0; j < h; j++) {
					memcpy(dest, src, s->outbuf.strides[i]);
					dest += s->outbuf.strides[i];
					src += img->stride[i];
				}
			}
			ms_queue_put(f->outputs[0], dupmsg(s->yuv_msg));

			ms_average_fps_update(&s->fps, (uint32_t)f->ticker->time);
			if (!s->first_image_decoded) {
				s->first_image_decoded = TRUE;
				ms_filter_notify_no_arg(f, MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
			}
		}

		while ((im = ms_queue_get(&mtofree_queue)) != NULL) {
			freemsg(im);
		}
	}
	
	ms_filter_unlock(f);
}

static void dec_postprocess(MSFilter *f) {
}

static int dec_reset_first_image(MSFilter* f, void *data) {
	DecState *s = (DecState *)f->data;
	s->first_image_decoded = FALSE;
	return 0;
}

static int dec_enable_avpf(MSFilter *f, void *data) {
	DecState *s = (DecState *)f->data;
	s->avpf_enabled = *((bool_t *)data) ? TRUE : FALSE;
	return 0;
}

static int dec_freeze_on_error(MSFilter *f, void *data) {
	DecState *s = (DecState *)f->data;
	s->freeze_on_error = *((bool_t *)data) ? TRUE : FALSE;
	return 0;
}

static int dec_reset(MSFilter *f, void *data) {
	DecState *s = (DecState *)f->data;
	ms_message("Reseting VP8 decoder");
	ms_filter_lock(f);
	vpx_codec_destroy(&s->codec);
	if (dec_initialize_impl(f) != 0){
		ms_error("Failed to reinitialize VP8 decoder");
	}
	s->first_image_decoded = FALSE;
	ms_filter_unlock(f);
	return 0;
}

static int dec_get_vsize(MSFilter *f, void *data) {
	DecState *s = (DecState *)f->data;
	MSVideoSize *vsize = (MSVideoSize *)data;
	if (s->first_image_decoded == TRUE) {
		vsize->width = s->yuv_width;
		vsize->height = s->yuv_height;
	} else {
		vsize->width = MS_VIDEO_SIZE_UNKNOWN_W;
		vsize->height = MS_VIDEO_SIZE_UNKNOWN_H;
	}
	return 0;
}

static int dec_get_fps(MSFilter *f, void *data){
	DecState *s = (DecState *)f->data;
	*(float*)data= ms_average_fps_get(&s->fps);
	return 0;
}

static int dec_get_out_fmt(MSFilter *f, void *data){
	DecState *s = (DecState *)f->data;
	MSPinFormat *pf=(MSPinFormat*)data;
	pf->fmt=ms_factory_get_video_format(f->factory,"YUV420P",ms_video_size_make(s->yuv_width,s->yuv_height),0,NULL);
	return 0;
}

static MSFilterMethod dec_methods[] = {
	{ MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION, dec_reset_first_image },
	{ MS_VIDEO_DECODER_ENABLE_AVPF,                    dec_enable_avpf       },
	{ MS_VIDEO_DECODER_FREEZE_ON_ERROR,                dec_freeze_on_error   },
	{ MS_VIDEO_DECODER_RESET,                          dec_reset             },
	{ MS_FILTER_GET_VIDEO_SIZE,                        dec_get_vsize         },
	{ MS_FILTER_GET_FPS,                               dec_get_fps           },
	{ MS_FILTER_GET_OUTPUT_FMT,                        dec_get_out_fmt       },
	{ 0,                                               NULL                  }
};

#define MS_VP8_DEC_NAME        "MSVp8Dec"
#define MS_VP8_DEC_DESCRIPTION "A VP8 video decoder using libvpx library."
#define MS_VP8_DEC_CATEGORY    MS_FILTER_DECODER
#define MS_VP8_DEC_ENC_FMT     "VP8"
#define MS_VP8_DEC_NINPUTS     1
#define MS_VP8_DEC_NOUTPUTS    1
#define MS_VP8_DEC_FLAGS       0

#ifdef _MSC_VER

MSFilterDesc ms_vp8_dec_desc = {
	MS_VP8_DEC_ID,
	MS_VP8_DEC_NAME,
	MS_VP8_DEC_DESCRIPTION,
	MS_VP8_DEC_CATEGORY,
	MS_VP8_DEC_ENC_FMT,
	MS_VP8_DEC_NINPUTS,
	MS_VP8_DEC_NOUTPUTS,
	dec_init,
	dec_preprocess,
	dec_process,
	dec_postprocess,
	dec_uninit,
	dec_methods,
	MS_VP8_DEC_FLAGS
};

#else

MSFilterDesc ms_vp8_dec_desc = {
	.id = MS_VP8_DEC_ID,
	.name = MS_VP8_DEC_NAME,
	.text = MS_VP8_DEC_DESCRIPTION,
	.category = MS_VP8_DEC_CATEGORY,
	.enc_fmt = MS_VP8_DEC_ENC_FMT,
	.ninputs = MS_VP8_DEC_NINPUTS,
	.noutputs = MS_VP8_DEC_NOUTPUTS,
	.init = dec_init,
	.preprocess = dec_preprocess,
	.process = dec_process,
	.postprocess = dec_postprocess,
	.uninit = dec_uninit,
	.methods = dec_methods,
	.flags = MS_VP8_DEC_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_vp8_dec_desc)
