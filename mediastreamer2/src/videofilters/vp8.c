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
	int bitrate;
	int width, height;
	long long frame_count;
	unsigned int mtu;
	float fps;
	VideoStarter starter;
	bool_t req_vfu;
	bool_t ready;
#ifdef FRAGMENT_ON_PARTITIONS
	uint8_t token_partition_count;
#endif
} EncState;

static void vp8_fragment_and_send(MSFilter *f,EncState *s,mblk_t *frame, uint32_t timestamp, const vpx_codec_cx_pkt_t *pkt, bool_t lastPartition);

static void enc_init(MSFilter *f) {
	vpx_codec_err_t res;
	EncState *s=(EncState *)ms_new0(EncState,1);

	ms_message("Using %s\n",vpx_codec_iface_name(interface));

	/* Populate encoder configuration */
	res = vpx_codec_enc_config_default(interface, &s->cfg, 0);
	if(res) {
		ms_error("Failed to get config: %s\n", vpx_codec_err_to_string(res));
	}

	s->width = MS_VIDEO_SIZE_CIF_W;
	s->height = MS_VIDEO_SIZE_CIF_H;
	s->bitrate=256000;
	s->frame_count = 0;
	s->cfg.g_w = s->width;
	s->cfg.g_h = s->height;
	/* encoder automatically places keyframes */
	s->cfg.kf_mode = VPX_KF_AUTO;
	s->cfg.kf_max_dist = 300;
	s->cfg.rc_target_bitrate = ((float)s->bitrate)*0.92/1024.0; //0.9=take into account IP/UDP/RTP overhead, in average.
	s->cfg.g_pass = VPX_RC_ONE_PASS; /* -p 1 */
	s->fps=15;
	s->cfg.g_timebase.num = 1;
	s->cfg.g_timebase.den = s->fps;
	s->cfg.rc_end_usage = VPX_CBR; /* --end-usage=cbr */
	s->cfg.g_threads = ms_get_cpu_count();
	ms_message("VP8 g_threads=%d", s->cfg.g_threads);
	s->cfg.rc_undershoot_pct = 95; /* --undershoot-pct=95 */
	s->cfg.g_error_resilient = 1;
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

	s->cfg.g_w = s->width;
	s->cfg.g_h = s->height;
	s->cfg.g_timebase.den=s->fps;
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
	if (s->cfg.g_threads > 1) {
		if (vpx_codec_control(&s->codec, VP8E_SET_TOKEN_PARTITIONS, 2) != VPX_CODEC_OK) {
			ms_error("VP8: failed to set multiple token partition");
		} else {
			ms_message("VP8: multiple token partitions used");
		}
	}
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
		vpx_img_wrap(&img, VPX_IMG_FMT_I420, s->width, s->height, 1, yuv.planes[0]);
		
		if (video_starter_need_i_frame (&s->starter,f->ticker->time)){
			/*sends an I frame at 2 seconds and 4 seconds after the beginning of the call*/
			s->req_vfu=TRUE;
		}
		if (s->req_vfu){
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

static int enc_set_vsize(MSFilter *f, void*data){
	MSVideoSize *vs=(MSVideoSize*)data;
	EncState *s=(EncState*)f->data;
	s->width=vs->width;
	s->height=vs->height;
	return 0;
}

static int enc_get_vsize(MSFilter *f, void *data){
	EncState *s=(EncState*)f->data;
	MSVideoSize *vs=(MSVideoSize*)data;
	vs->width=s->width;
	vs->height=s->height;
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
	s->fps=*fps;
	return 0;
}

static int enc_get_fps(MSFilter *f, void *data){
	EncState *s=(EncState*)f->data;
	float *fps=(float*)data;
	*fps=s->fps;
	return 0;
}

static int enc_get_br(MSFilter *f, void*data){
	EncState *s=(EncState*)f->data;
	*(int*)data=s->bitrate;
	return 0;
}

static int enc_set_br(MSFilter *f, void*data){
	int br=*(int*)data;
	EncState *s=(EncState*)f->data;
	s->bitrate=br;
	s->cfg.rc_target_bitrate = ((float)s->bitrate)*0.92/1024.0; //0.9=take into account IP/UDP/RTP overhead, in average.
	if (s->ready){
		ms_filter_lock(f);
		enc_postprocess(f);
		enc_preprocess(f);
		ms_filter_unlock(f);
		return 0;
	}
	if (br>=1024000){
		s->width = MS_VIDEO_SIZE_VGA_W;
		s->height = MS_VIDEO_SIZE_VGA_H;
		s->fps=25;
	}else if (br>=350000){
		s->width = MS_VIDEO_SIZE_VGA_W;
		s->height = MS_VIDEO_SIZE_VGA_H;
		s->fps=15;
	} else if (br>=200000){
		s->width = MS_VIDEO_SIZE_CIF_W;
		s->height = MS_VIDEO_SIZE_CIF_H;
		s->fps=15;
	}else if (br>=150000){
		s->width=MS_VIDEO_SIZE_QVGA_W;
		s->height=MS_VIDEO_SIZE_QVGA_H;
		s->fps=15;
	}else if (br>=100000){
		s->width=MS_VIDEO_SIZE_QVGA_W;
		s->height=MS_VIDEO_SIZE_QVGA_H;
		s->fps=10;
	}else if (br>=64000){
		s->width=MS_VIDEO_SIZE_QCIF_W;
		s->height=MS_VIDEO_SIZE_QCIF_H;
		s->fps=12;
	}else{
		s->width=MS_VIDEO_SIZE_QCIF_W;
		s->height=MS_VIDEO_SIZE_QCIF_H;
		s->fps=5;
	}
#ifdef __arm__
	if (br >= 300000) {
		s->width = MS_VIDEO_SIZE_VGA_W;
		s->height = MS_VIDEO_SIZE_VGA_H;
	} else {
		s->width=MS_VIDEO_SIZE_QVGA_W;
		s->height=MS_VIDEO_SIZE_QVGA_H;
	}
	s->fps=12;
#endif

	ms_message("bitrate requested...: %d (%d x %d)\n", br, s->width, s->height);
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

static MSFilterMethod enc_methods[]={
	{	MS_FILTER_SET_VIDEO_SIZE, enc_set_vsize },
	{	MS_FILTER_SET_FPS,	  enc_set_fps	},
	{	MS_FILTER_GET_VIDEO_SIZE, enc_get_vsize },
	{	MS_FILTER_GET_FPS,	  enc_get_fps	},
	{	MS_FILTER_ADD_ATTR,       enc_add_attr	},
	{	MS_FILTER_SET_BITRATE,    enc_set_br	},
	{	MS_FILTER_GET_BITRATE,    enc_get_br	},
	{	MS_FILTER_SET_MTU,        enc_set_mtu	},
	{	MS_FILTER_REQ_VFU,        enc_req_vfu	},
	{	MS_VIDEO_ENCODER_REQ_VFU, enc_req_vfu	},
	{	0			, NULL }
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
	uint64_t last_error_reported_time;
	mblk_t *yuv_msg;
	MSPicture outbuf;
	int yuv_width, yuv_height;
	MSQueue q;
	MSAverageFPS fps;
	bool_t first_image_decoded;
} DecState;


static void dec_init(MSFilter *f) {
	DecState *s=(DecState *)ms_new(DecState,1);

	ms_message("Using %s\n",vpx_codec_iface_name(interface));

	/* Initialize codec */
	if(vpx_codec_dec_init(&s->codec, interface, NULL, 0))
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
		im->b_rptr += (ibit + lbit + (tbit | kbit) + mbit);
	}

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

			err = vpx_codec_decode(&s->codec, m->b_rptr, m->b_wptr - m->b_rptr, NULL, 0);
			if (err) {
				ms_warning("vpx_codec_decode failed : %d %s (%s)\n", err, vpx_codec_err_to_string(err), vpx_codec_error_detail(&s->codec));

				if ((f->ticker->time - s->last_error_reported_time)>5000 || s->last_error_reported_time==0) {
					s->last_error_reported_time=f->ticker->time;
					ms_filter_notify_no_arg(f,MS_VIDEO_DECODER_DECODING_ERRORS);
				}
                if (s->first_image_decoded == FALSE) {
                    /* if no frames have been decoded yet, do not try to browse decoded frames */
                    freemsg(m);
                    continue;
                }
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
					ms_filter_notify_no_arg(f,MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
					s->first_image_decoded = TRUE;
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

static MSFilterMethod dec_methods[]={
	{	MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION, reset_first_image },
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
