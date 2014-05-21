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

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>

#undef interface
#define interface (vpx_codec_vp8_cx())

#define VP8_PAYLOAD_DESC_X_MASK      0x80
#define VP8_PAYLOAD_DESC_RSV_MASK    0x40
#define VP8_PAYLOAD_DESC_N_MASK      0x20
#define VP8_PAYLOAD_DESC_S_MASK      0x10
#define VP8_PAYLOAD_DESC_PARTID_MASK 0x0F

#undef FRAGMENT_ON_PARTITIONS
/*#define FRAGMENT_ON_PARTITIONS*/

#define MS_VP8_CONF(required_bitrate, bitrate_limit, resolution, fps) \
	{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H }, fps, NULL }

static const MSVideoConfiguration vp8_conf_list[] = {
#if defined(ANDROID) || (TARGET_OS_IPHONE == 1) || defined(__arm__)
	MS_VP8_CONF(300000, 600000,    VGA, 12),
	MS_VP8_CONF(100000, 300000,   QVGA, 12),
	MS_VP8_CONF(  64000,  100000, QCIF, 12),
	MS_VP8_CONF(      0,   64000, QCIF,  5)
#else
	MS_VP8_CONF(1024000, 1536000,  VGA, 25),
	MS_VP8_CONF( 350000,  600000,  VGA, 15),
	MS_VP8_CONF( 200000,  350000,  CIF, 15),
	MS_VP8_CONF( 150000,  200000, QVGA, 15),
	MS_VP8_CONF( 100000,  150000, QVGA, 10),
	MS_VP8_CONF(  64000,  100000, QCIF, 12),
	MS_VP8_CONF(      0,   64000, QCIF,  5)
#endif
};

static const MSVideoConfiguration multicore_vp8_conf_list[] = {
#if defined(ANDROID) || (TARGET_OS_IPHONE == 1) || defined(__arm__)
	MS_VP8_CONF(2048000, 2560000,       UXGA, 12),
	MS_VP8_CONF(1024000, 1536000, SXGA_MINUS, 12),
	MS_VP8_CONF( 750000, 1024000,        XGA, 12),
	MS_VP8_CONF( 500000,  750000,       SVGA, 12),
	MS_VP8_CONF( 300000,  500000,        VGA, 12),
	MS_VP8_CONF( 100000,  300000,       QVGA, 12),
	MS_VP8_CONF(  64000,  100000,       QCIF, 12),
	MS_VP8_CONF(      0,   64000,       QCIF,  5)
#else
	MS_VP8_CONF(1536000,  2560000, SXGA_MINUS, 15),
	MS_VP8_CONF(1536000,  2560000,       720P, 15),
	MS_VP8_CONF(1024000,  1536000,        XGA, 15),
	MS_VP8_CONF( 600000,  1024000,       SVGA, 15),
	MS_VP8_CONF( 350000,   600000,        VGA, 15),
	MS_VP8_CONF( 200000,   350000,        CIF, 15),
	MS_VP8_CONF( 150000,   200000,       QVGA, 15),
	MS_VP8_CONF( 100000,   150000,       QVGA, 10),
	MS_VP8_CONF(  64000,   100000,       QCIF, 12),
	MS_VP8_CONF(      0,    64000,       QCIF,  5)
#endif
};

/* the goal of this small object is to tell when to send I frames at startup:
at 2 and 4 seconds*/
typedef struct VideoStarter{
	uint64_t next_time;
	int i_frame_count;
}VideoStarter;

static void video_starter_init(VideoStarter *vs){
	vs->next_time=0;
	vs->i_frame_count=0;
}

static void video_starter_first_frame(VideoStarter *vs, uint64_t curtime){
	vs->next_time=curtime+2000;
}

static bool_t video_starter_need_i_frame(VideoStarter *vs, uint64_t curtime){
	if (vs->next_time==0) return FALSE;
	if (curtime>=vs->next_time){
		vs->i_frame_count++;
		if (vs->i_frame_count==1){
			vs->next_time+=2000;
		}else{
			vs->next_time=0;
		}
		return TRUE;
	}
	return FALSE;
}

typedef struct EncState {
	vpx_codec_ctx_t codec;
	vpx_codec_enc_cfg_t cfg;
	long long frame_count;
	unsigned int mtu;
	VideoStarter starter;
	bool_t req_vfu;
	bool_t ready;
#ifdef FRAGMENT_ON_PARTITIONS
	uint8_t token_partition_count;
#endif
	const MSVideoConfiguration *vconf_list;
	MSVideoConfiguration vconf;
} EncState;

static void vp8_fragment_and_send(MSFilter *f,EncState *s,mblk_t *frame, uint32_t timestamp, const vpx_codec_cx_pkt_t *pkt, bool_t lastPartition);

static void enc_init(MSFilter *f) {
	vpx_codec_err_t res;
	MSVideoSize vsize;
	EncState *s=(EncState *)ms_new0(EncState,1);

	ms_message("Using %s\n",vpx_codec_iface_name(interface));

	/* Populate encoder configuration */
	res = vpx_codec_enc_config_default(interface, &s->cfg, 0);
	if(res) {
		ms_error("Failed to get config: %s\n", vpx_codec_err_to_string(res));
	}

	if (ms_get_cpu_count() > 1) s->vconf_list = &multicore_vp8_conf_list[0];
	else s->vconf_list = &vp8_conf_list[0];
	MS_VIDEO_SIZE_ASSIGN(vsize, CIF);
	s->vconf = ms_video_find_best_configuration_for_size(s->vconf_list, vsize);
	s->frame_count = 0;
	s->cfg.g_w = s->vconf.vsize.width;
	s->cfg.g_h = s->vconf.vsize.height;
	/* encoder automatically places keyframes */
	s->cfg.kf_mode = VPX_KF_AUTO;
	s->cfg.kf_max_dist = 300;
	s->cfg.rc_target_bitrate = ((float)s->vconf.required_bitrate)*0.92/1024.0; //0.9=take into account IP/UDP/RTP overhead, in average.
	s->cfg.g_pass = VPX_RC_ONE_PASS; /* -p 1 */
	s->cfg.g_timebase.num = 1;
	s->cfg.g_timebase.den = s->vconf.fps;
	s->cfg.rc_end_usage = VPX_CBR; /* --end-usage=cbr */
#if TARGET_IPHONE_SIMULATOR
	s->cfg.g_threads = 1; /*workaround to remove crash on ipad simulator*/
#else
	s->cfg.g_threads = ms_get_cpu_count();
#endif
	ms_message("VP8 g_threads=%d", s->cfg.g_threads);
	s->cfg.rc_undershoot_pct = 95; /* --undershoot-pct=95 */
	s->cfg.g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT|VPX_ERROR_RESILIENT_PARTITIONS;
	s->cfg.g_lag_in_frames = 0;
	s->mtu=ms_get_payload_max_size()-1;/*-1 for the vp8 payload header*/

	f->data = s;
}

static void enc_uninit(MSFilter *f) {
	EncState *s=(EncState*)f->data;

	ms_free(s);
}

static void enc_preprocess(MSFilter *f) {
	vpx_codec_err_t res;
	EncState *s=(EncState*)f->data;

	s->cfg.g_w = s->vconf.vsize.width;
	s->cfg.g_h = s->vconf.vsize.height;
	s->cfg.g_timebase.den=s->vconf.fps;
	/* Initialize codec */
	#ifdef FRAGMENT_ON_PARTITIONS
	/* VPX_CODEC_USE_OUTPUT_PARTITION: output 1 frame per partition */
	res =  vpx_codec_enc_init(&s->codec, interface, &s->cfg, VPX_CODEC_USE_OUTPUT_PARTITION);
	#else
	res =  vpx_codec_enc_init(&s->codec, interface, &s->cfg, 0);
	#endif
	if (res) {
		ms_error("vpx_codec_enc_init failed: %s (%s)n", vpx_codec_err_to_string(res), vpx_codec_error_detail(&s->codec));
	}
	/*cpu/quality tradeoff: positive values decrease CPU usage at the expense of quality*/
	vpx_codec_control(&s->codec, VP8E_SET_CPUUSED, (s->cfg.g_threads > 1) ? 10 : 10);
	vpx_codec_control(&s->codec, VP8E_SET_STATIC_THRESHOLD, 0);
	vpx_codec_control(&s->codec, VP8E_SET_ENABLEAUTOALTREF, 1);
	vpx_codec_control(&s->codec, VP8E_SET_MAX_INTRA_BITRATE_PCT, 400); /*limite iFrame size to 4 pframe*/
	#ifndef FRAGMENT_ON_PARTITIONS
	if (s->cfg.g_threads > 1) {
		if (vpx_codec_control(&s->codec, VP8E_SET_TOKEN_PARTITIONS, 2) != VPX_CODEC_OK) {
			ms_error("VP8: failed to set multiple token partition");
		} else {
			ms_message("VP8: multiple token partitions used");
		}
	}
	#endif
	#ifdef FRAGMENT_ON_PARTITIONS
	vpx_codec_control(&s->codec, VP8E_SET_TOKEN_PARTITIONS, 0x3);
	s->token_partition_count = 8;
	#endif
	/* vpx_codec_control(&s->codec, VP8E_SET_CPUUSED, 0);*/ /* -16 (quality) .. 16 (speed) */

	video_starter_init(&s->starter);
	s->ready=TRUE;
}

static void enc_process(MSFilter *f) {
	mblk_t *im,*om;
	uint64_t timems=f->ticker->time;
	uint32_t timestamp=timems*90;
	EncState *s=(EncState*)f->data;
	unsigned int flags = 0;
	vpx_codec_err_t err;
	YuvBuf yuv;

	ms_filter_lock(f);
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		vpx_image_t img;

		om = NULL;
		flags = 0;

		ms_yuv_buf_init_from_mblk(&yuv, im);
		vpx_img_wrap(&img, VPX_IMG_FMT_I420, s->vconf.vsize.width, s->vconf.vsize.height, 1, yuv.planes[0]);

		if (video_starter_need_i_frame (&s->starter,f->ticker->time)){
			/*sends an I frame at 2 seconds and 4 seconds after the beginning of the call*/
			s->req_vfu=TRUE;
		}
		if (s->req_vfu){
			ms_message("Forcing vp8 key frame for filter [%p]",f);
			flags = VPX_EFLAG_FORCE_KF;
			s->req_vfu=FALSE;
		}

		err = vpx_codec_encode(&s->codec, &img, s->frame_count, 1, flags, VPX_DL_REALTIME);

		if (err) {
			ms_error("vpx_codec_encode failed : %d %s (%s)\n", err, vpx_codec_err_to_string(err), vpx_codec_error_detail(&s->codec));
		} else {
			vpx_codec_iter_t iter = NULL;
			const vpx_codec_cx_pkt_t *pkt;

			s->frame_count++;
			if (s->frame_count==1){
				video_starter_first_frame (&s->starter,f->ticker->time);
			}

			while( (pkt = vpx_codec_get_cx_data(&s->codec, &iter)) ) {
				if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
					if (pkt->data.frame.sz > 0) {
						om = allocb(pkt->data.frame.sz,0);
						memcpy(om->b_wptr, pkt->data.frame.buf, pkt->data.frame.sz);
						om->b_wptr += pkt->data.frame.sz;
						#ifdef FRAGMENT_ON_PARTITIONS
						vp8_fragment_and_send(f, s, om, timestamp, pkt, (pkt->data.frame.partition_id == s->token_partition_count));
						#else
						vp8_fragment_and_send(f, s, om, timestamp, pkt, 1);
						#endif
					}
				}
			}
		}
		freemsg(im);
	}
	ms_filter_unlock(f);
}

static void enc_postprocess(MSFilter *f) {
	EncState *s=(EncState*)f->data;
	if (s->ready) vpx_codec_destroy(&s->codec);
	s->ready=FALSE;
}

static int enc_set_configuration(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	const MSVideoConfiguration *vconf = (const MSVideoConfiguration *)data;
	if (vconf != &s->vconf) memcpy(&s->vconf, vconf, sizeof(MSVideoConfiguration));

	if (s->vconf.required_bitrate > s->vconf.bitrate_limit)
		s->vconf.required_bitrate = s->vconf.bitrate_limit;
	s->cfg.rc_target_bitrate = ((float)s->vconf.required_bitrate) * 0.92 / 1024.0; //0.9=take into account IP/UDP/RTP overhead, in average.
	if (s->ready) {
		ms_filter_lock(f);
		enc_postprocess(f);
		enc_preprocess(f);
		ms_filter_unlock(f);
		return 0;
	}

	ms_message("Video configuration set: bitrate=%dbits/s, fps=%f, vsize=%dx%d", s->vconf.required_bitrate, s->vconf.fps, s->vconf.vsize.width, s->vconf.vsize.height);
	return 0;
}

static int enc_set_vsize(MSFilter *f, void *data) {
	MSVideoConfiguration best_vconf;
	MSVideoSize *vs = (MSVideoSize *)data;
	EncState *s = (EncState *)f->data;
	best_vconf = ms_video_find_best_configuration_for_size(s->vconf_list, *vs);
	s->vconf.vsize = *vs;
	s->vconf.fps = best_vconf.fps;
	s->vconf.bitrate_limit = best_vconf.bitrate_limit;
	enc_set_configuration(f, &s->vconf);
	return 0;
}

static int enc_get_vsize(MSFilter *f, void *data){
	EncState *s=(EncState*)f->data;
	MSVideoSize *vs=(MSVideoSize*)data;
	*vs = s->vconf.vsize;
	return 0;
}

static int enc_add_attr(MSFilter *f, void*data){
	/*const char *attr=(const char*)data;
	EncState *s=(EncState*)f->data;*/
	return 0;
}

static int enc_set_fps(MSFilter *f, void *data){
	float *fps=(float*)data;
	EncState *s=(EncState*)f->data;
	s->vconf.fps=*fps;
	enc_set_configuration(f, &s->vconf);
	return 0;
}

static int enc_get_fps(MSFilter *f, void *data){
	EncState *s=(EncState*)f->data;
	float *fps=(float*)data;
	*fps=s->vconf.fps;
	return 0;
}

static int enc_get_br(MSFilter *f, void*data){
	EncState *s=(EncState*)f->data;
	*(int*)data=s->vconf.required_bitrate;
	return 0;
}

static int enc_set_br(MSFilter *f, void*data) {
	EncState *s = (EncState *)f->data;
	int br = *(int *)data;
	if (s->ready) {
		/* Encoding is already ongoing, do not change video size, only bitrate. */
		s->vconf.required_bitrate = br;
		enc_set_configuration(f, &s->vconf);
	} else {
		MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_bitrate(s->vconf_list, br);
		enc_set_configuration(f, &best_vconf);
	}
	return 0;
}

static int enc_set_mtu(MSFilter *f, void*data){
	EncState *s=(EncState*)f->data;
	s->mtu=*(int*)data;
	return 0;
}

static int enc_req_vfu(MSFilter *f, void *unused){
	EncState *s=(EncState*)f->data;
	s->req_vfu=TRUE;
	return 0;
}

static int enc_get_configuration_list(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	const MSVideoConfiguration **vconf_list = (const MSVideoConfiguration **)data;
	*vconf_list = s->vconf_list;
	return 0;
}

static MSFilterMethod enc_methods[] = {
	{ MS_FILTER_SET_VIDEO_SIZE,                enc_set_vsize              },
	{ MS_FILTER_SET_FPS,                       enc_set_fps                },
	{ MS_FILTER_GET_VIDEO_SIZE,                enc_get_vsize              },
	{ MS_FILTER_GET_FPS,                       enc_get_fps                },
	{ MS_FILTER_ADD_ATTR,                      enc_add_attr               },
	{ MS_FILTER_SET_BITRATE,                   enc_set_br                 },
	{ MS_FILTER_GET_BITRATE,                   enc_get_br                 },
	{ MS_FILTER_SET_MTU,                       enc_set_mtu                },
	{ MS_FILTER_REQ_VFU,                       enc_req_vfu                },
	{ MS_VIDEO_ENCODER_REQ_VFU,                enc_req_vfu                },
	{ MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST, enc_get_configuration_list },
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION,      enc_set_configuration      },
	{ 0,                                       NULL                       }
};

#ifdef _MSC_VER
MSFilterDesc ms_vp8_enc_desc={
	MS_VP8_ENC_ID,
	"MSVp8Enc",
	N_("A video VP8 encoder using libvpx library."),
	MS_FILTER_ENCODER,
	"VP8",
	1, /*MS_YUV420P is assumed on this input */
	1,
	enc_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	enc_methods
};
#else
MSFilterDesc ms_vp8_enc_desc={
	.id=MS_VP8_ENC_ID,
	.name="MSVp8Enc",
	.text=N_("A video VP8 encoder using libvpx library."),
	.category=MS_FILTER_ENCODER,
	.enc_fmt="VP8",
	.ninputs=1, /*MS_YUV420P is assumed on this input */
	.noutputs=1,
	.init=enc_init,
	.preprocess=enc_preprocess,
	.process=enc_process,
	.postprocess=enc_postprocess,
	.uninit=enc_uninit,
	.methods=enc_methods
};
#endif

MS_FILTER_DESC_EXPORT(ms_vp8_enc_desc)


static void vp8_fragment_and_send(MSFilter *f,EncState *s,mblk_t *frame, uint32_t timestamp, const vpx_codec_cx_pkt_t *pkt, bool_t lastPartition){
	uint8_t *rptr;
	mblk_t *packet=NULL;
	mblk_t* vp8_payload_desc = NULL;
	int len;

#if 0
	if ((pkt->data.frame.flags & VPX_FRAME_IS_KEY) == 0) {
		ms_debug("P-FRAME: %u\n", pkt->data.frame.sz);
	} else {
		ms_debug("I-FRAME: %u\n", pkt->data.frame.sz);
	}
#endif

	for (rptr=frame->b_rptr;rptr<frame->b_wptr;){
		vp8_payload_desc = allocb(1, 0);
		vp8_payload_desc->b_wptr=vp8_payload_desc->b_rptr+1;

		len=MIN(s->mtu,(frame->b_wptr-rptr));
		packet=dupb(frame);
		packet->b_rptr=rptr;
		packet->b_wptr=rptr+len;
		mblk_set_timestamp_info(packet,timestamp);
		mblk_set_timestamp_info(vp8_payload_desc,timestamp);

		/* insert 1 byte vp8 payload descriptor */
		(*vp8_payload_desc->b_rptr) = 0;
		/* X (extended) field, 0 */
		(*vp8_payload_desc->b_rptr) &= ~VP8_PAYLOAD_DESC_X_MASK;
		/* RSV field, always 0 */
		(*vp8_payload_desc->b_rptr) &= ~VP8_PAYLOAD_DESC_RSV_MASK;
		/* N : set to 1 if non reference frame */
		if ((pkt->data.frame.flags & VPX_FRAME_IS_KEY) == 0)
			(*vp8_payload_desc->b_rptr) |= VP8_PAYLOAD_DESC_N_MASK;
		/* S : partition start */
		if (rptr == frame->b_rptr) {
			(*vp8_payload_desc->b_rptr) |= VP8_PAYLOAD_DESC_S_MASK;
		}
		/* PartID : partition id */
		#ifdef FRAGMENT_ON_PARTITIONS
		(*vp8_payload_desc->b_rptr) |= (pkt->data.frame.partition_id & VP8_PAYLOAD_DESC_PARTID_MASK);
		#endif

		vp8_payload_desc->b_cont = packet;

		ms_queue_put(f->outputs[0], vp8_payload_desc);
		rptr+=len;
	}

	freeb(frame);

	/*set marker bit on last packet*/
	if (lastPartition) {
		mblk_set_marker_info(packet,TRUE);
		mblk_set_marker_info(vp8_payload_desc,TRUE);
	}
}

#undef interface
#include <assert.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
#define interface (vpx_codec_vp8_dx())

typedef struct DecState {
	vpx_codec_ctx_t codec;
	mblk_t *curframe;
	long last_cseq; /*last receive sequence number, used to locate missing partition fragment*/
	int current_partition_id; /*current partition id*/
	uint64_t last_error_reported_time;
	mblk_t *yuv_msg;
	MSPicture outbuf;
	int yuv_width, yuv_height;
	MSQueue q;
	MSAverageFPS fps;
	bool_t first_image_decoded;
	bool_t on_error; /*a decoding error occurs*/
} DecState;


static void dec_init(MSFilter *f) {
	DecState *s=(DecState *)ms_new(DecState,1);
	vpx_codec_flags_t  flags = 0/*VPX_CODEC_USE_ERROR_CONCEALMENT*/;
#ifdef FRAGMENT_ON_PARTITIONS
	flags |= VPX_CODEC_USE_INPUT_FRAGMENTS;
#endif

	ms_message("Using %s\n",vpx_codec_iface_name(interface));

	/* Initialize codec */
	if(vpx_codec_dec_init(&s->codec, interface, NULL, flags))
		ms_error("Failed to initialize decoder");

	s->curframe = NULL;
	s->last_error_reported_time = 0;
	s->yuv_width = 0;
	s->yuv_height = 0;
	s->yuv_msg = 0;
	ms_queue_init(&s->q);
	s->first_image_decoded = FALSE;
	f->data = s;
	ms_video_init_average_fps(&s->fps, "VP8 decoder: FPS: %f");
	s->on_error=FALSE;
}

static void dec_preprocess(MSFilter* f) {
	DecState *s=(DecState*)f->data;
	s->first_image_decoded = FALSE;
}

static void dec_uninit(MSFilter *f) {
	DecState *s=(DecState*)f->data;
	vpx_codec_destroy(&s->codec);

	if (s->curframe!=NULL)
		freemsg(s->curframe);
	if (s->yuv_msg)
		freemsg(s->yuv_msg);

	ms_queue_flush(&s->q);

	ms_free(s);
}

/* remove payload header and aggregates fragmented packets */
static void dec_unpacketize(MSFilter *f, DecState *s, mblk_t *im, MSQueue *out){
	int xbit = (im->b_rptr[0] & 0x80) >> 7;
#ifdef FRAGMENT_ON_PARTITIONS
	int sbit = (im->b_rptr[0] & VP8_PAYLOAD_DESC_S_MASK) >> 4;
	int partition_id = im->b_rptr[0] & 0b00000111;
	int current_cseq=mblk_get_cseq(im);
	int discontinuity_detected=(current_cseq-s->last_cseq)-1;
	bool_t end_of_frame=mblk_get_marker_info(im);
#endif

	im->b_rptr++;
	if (xbit) {
		/* Ignore extensions if some are present */
		int ibit = (im->b_rptr[0] & 0x80) >> 7;
		int lbit = (im->b_rptr[0] & 0x40) >> 6;
		int tbit = (im->b_rptr[0] & 0x20) >> 5;
		int kbit = (im->b_rptr[0] & 0x10) >> 4;
		int mbit = 0;
		if (ibit) {
			mbit = (im->b_rptr[1] & 0x80) >> 7;
		}
		im->b_rptr += (xbit + ibit + lbit + (tbit | kbit) + mbit);
	}

#ifdef FRAGMENT_ON_PARTITIONS
	/*first, check for discontinuity*/
	if (discontinuity_detected) {
		ms_warning("vp8 detect discontinuity at cseq [%i] on filter [%p]",current_cseq,f);
		if (s->curframe) {
			ms_warning("vp8 discarding previous partition on filter [%p]",f);
			/*reset frame size to tell decoder there is a missing partition*/
			/*s->curframe->b_wptr=s->curframe->b_rptr;
			ms_queue_put(out, s->curframe);*/
			freemsg(s->curframe);
			s->curframe=NULL;
		}
	}

	/*it it a partition start ?*/
	if (sbit) {
		if (s->curframe) {
			/*previous partition is over*/
			if (mblk_get_timestamp_info(im) != mblk_get_timestamp_info(s->curframe)) {
				/*previous partition is over but last partition is probably lost*/
				mblk_set_marker_info(s->curframe,1);
			}
			ms_queue_put(out, s->curframe);
		}
		s->curframe=im;
	} else {
		if (s->current_partition_id == partition_id && s->curframe){
			/*good, this is a continuation*/
			concatb(s->curframe,im);
			msgpullup(s->curframe,-1);
		} else {
			/*well, this is probably a tailing partition segment*/
			ms_warning("vp8 trashing tailing partitions on filter [%p]",f);
			freemsg(im);
		}

	}

	s->current_partition_id=partition_id;
	s->last_cseq=current_cseq;
	if (end_of_frame) {
		/*end of frame*/
		if (s->curframe) {
			/*copy marker bit*/
			mblk_set_marker_info(s->curframe,1);
			/*push last partition*/
			ms_queue_put(out, s->curframe);
			s->curframe=NULL;
		}
	}

#else /*FRAGMENT_ON_PARTITIONS*/
	/* end of frame bit ? */
	if (mblk_get_marker_info(im)) {
		/* should be aggregated with previous packet ? */
		if (s->curframe!=NULL){
			/* same timestamp ? */
			if (mblk_get_timestamp_info(im) == mblk_get_timestamp_info(s->curframe)) {
				concatb(s->curframe,im);
				msgpullup(s->curframe,-1);
				/* transmit complete frame */
				ms_queue_put(out, s->curframe);
				s->curframe=NULL;
			} else {
				/* transmit partial frame */
				ms_queue_put(out, s->curframe);
				s->curframe = NULL;
				/* transmit new one (be it complete or not) */
				ms_queue_put(out, im);
			}
		} else {
			/* transmit new one (be it complete or not) */
			ms_queue_put(out, im);
		}
	} else {
		if (s->curframe!=NULL) {
			/* append if same timestamp */
			if (mblk_get_timestamp_info(im) == mblk_get_timestamp_info(s->curframe)) {
				concatb(s->curframe,im);
			} else {
				/* transmit partial frame */
				ms_queue_put(out, s->curframe);
				s->curframe = im;
			}
		}
		else {
			s->curframe = im;
		}
	}
#endif /*FRAGMENT_ON_PARTITIONS*/
}

static void dec_process(MSFilter *f) {
	mblk_t *im;
	DecState *s=(DecState*)f->data;

	while( (im=ms_queue_get(f->inputs[0]))!=0) {
		mblk_t *m;

		dec_unpacketize(f, s, im, &s->q);

		while((m=ms_queue_get(&s->q))!=NULL){
			vpx_codec_err_t err;
			vpx_codec_iter_t  iter = NULL;
			vpx_image_t *img;

			/*
			 * 4.3.  VP8 Payload Header
			 *
			 *  0 1 2 3 4 5 6 7
			 *  +-+-+-+-+-+-+-+-+
			 *  |Size0|H| VER |P|
			 *  +-+-+-+-+-+-+-+-+
			 *  ...
			 *
			 * P: Inverse key frame flag.  When set to 0 the current frame is a key
			 * frame.  When set to 1 the current frame is an interframe.  Defined
			 * in [RFC6386]
			 */
			if (m->b_rptr[0] & 0x01) {
				/*not a key frame, freezing image until next keyframe*/
				/*if (s->first_image_decoded && s->on_error) {
					freemsg(m);
					continue;
				}*/
			} else
				ms_message("vp8 key frame received on filter [%p]",f);

			err = vpx_codec_decode(&s->codec, m->b_rptr, m->b_wptr - m->b_rptr, NULL, 0);
			if (err) {
				/*ms_warning("vp8 decode failed : %d %s (%s)\n", err, vpx_codec_err_to_string(err), vpx_codec_error_detail(&s->codec));*/
				s->on_error=TRUE;
				if ((f->ticker->time - s->last_error_reported_time)>5000 || s->last_error_reported_time==0) {
					s->last_error_reported_time=f->ticker->time;
					ms_filter_notify_no_arg(f,MS_VIDEO_DECODER_DECODING_ERRORS);
				}
				if (s->first_image_decoded == FALSE) {
					/* if no frames have been decoded yet, do not try to browse decoded frames */
					freemsg(m);
					continue;
				}
			} else {
#ifdef FRAGMENT_ON_PARTITIONS
				if (mblk_get_marker_info(m)) {
					/*end of partitions*/
					err=vpx_codec_decode(&s->codec, NULL,0, NULL, 0);
					if (0 /*err*/) {
						ms_warning("vp8 decode failed at end of partitions : %d %s (%s)\n", err, vpx_codec_err_to_string(err), vpx_codec_error_detail(&s->codec));
					}
				}
#endif /*FRAGMENT_ON_PARTITIONS*/
				s->on_error=FALSE;
			}


			/* browse decoded frames */
			while((img = vpx_codec_get_frame(&s->codec, &iter))) {
				int i,j;

				if (s->yuv_width != img->d_w || s->yuv_height != img->d_h) {
					if (s->yuv_msg)
						freemsg(s->yuv_msg);
					s->yuv_msg = ms_yuv_buf_alloc(&s->outbuf, img->d_w, img->d_h);
					s->yuv_width = img->d_w;
					s->yuv_height = img->d_h;
				}

				/* scale/copy frame to destination mblk_t */
				for(i=0; i<3; i++) {
					uint8_t* dest = s->outbuf.planes[i];
					uint8_t* src = img->planes[i];
					int h = img->d_h >> ((i>0)?1:0);

					for(j=0; j<h; j++) {
						memcpy(dest, src, s->outbuf.strides[i]);

						dest += s->outbuf.strides[i];
						src += img->stride[i];
					}
				}
				ms_queue_put(f->outputs[0], dupmsg(s->yuv_msg));

				if (ms_video_update_average_fps(&s->fps, f->ticker->time)) {
					ms_message("VP8 decoder: Frame size: %dx%d", s->yuv_width, s->yuv_height);
				}
				if (!s->first_image_decoded) {
					s->first_image_decoded = TRUE;
					ms_filter_notify_no_arg(f,MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
				}
			}
			freemsg(m);
		}
	}
}

static int reset_first_image(MSFilter* f, void *data) {
	DecState *s=(DecState*)f->data;
	s->first_image_decoded = FALSE;
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

static MSFilterMethod dec_methods[]={
	{	MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION, reset_first_image },
	{	MS_FILTER_GET_VIDEO_SIZE, dec_get_vsize	},
	{		0		,		NULL			}
};

#ifdef _MSC_VER
MSFilterDesc ms_vp8_dec_desc={
	MS_VP8_DEC_ID,
	"MSVp8Dec",
	"A VP8 decoder using libvpx library",
	MS_FILTER_DECODER,
	"VP8",
	1,
	1,
	dec_init,
	dec_preprocess,
	dec_process,
	NULL,
	dec_uninit,
	dec_methods
};
#else
MSFilterDesc ms_vp8_dec_desc={
	.id=MS_VP8_DEC_ID,
	.name="MSVp8Dec",
	.text="A VP8 decoder using libvpx library",
	.category=MS_FILTER_DECODER,
	.enc_fmt="VP8",
	.ninputs=1,
	.noutputs=1,
	.init=dec_init,
	.preprocess=dec_preprocess,
	.process=dec_process,
	.postprocess=NULL,
	.uninit=dec_uninit,
	.methods=dec_methods
};
#endif
MS_FILTER_DESC_EXPORT(ms_vp8_dec_desc)
