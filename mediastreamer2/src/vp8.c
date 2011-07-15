#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>

#include <libswscale/swscale.h>

#define interface (vpx_codec_vp8_cx())

#define VP8_PAYLOAD_DESC_RSV_MASK 0xE0
#define VP8_PAYLOAD_DESC_I_MASK   0x10
#define VP8_PAYLOAD_DESC_N_MASK   0x08
#define VP8_PAYLOAD_DESC_FI_MASK  0x06
#define VP8_PAYLOAD_DESC_B_MASK   0x01

#define VP8_PAYLOAD_DESC_FI_BEG_PARTITION 0x02
#define VP8_PAYLOAD_DESC_FI_END_PARTITION 0x04
#define VP8_PAYLOAD_DESC_FI_MID_PARTITION 0x06

typedef struct EncState {
	vpx_codec_ctx_t codec;
	vpx_codec_enc_cfg_t cfg;
	int width, height;
	long long frame_count;
	unsigned int mtu;
	float fps;
} EncState;

static void vp8_fragment_and_send(MSFilter *f,EncState *s,mblk_t *frame, uint32_t timestamp, const vpx_codec_cx_pkt_t *pkt);

static void enc_init(MSFilter *f) {
	vpx_codec_err_t res;
	EncState *s=(EncState *)ms_new(EncState,1);

	ms_message("Using %s\n",vpx_codec_iface_name(interface));

	/* Populate encoder configuration */
	res = vpx_codec_enc_config_default(interface, &s->cfg, 0);
	if(res) {
		ms_error("Failed to get config: %s\n", vpx_codec_err_to_string(res));
	}

	s->width = MS_VIDEO_SIZE_CIF_W;
	s->height = MS_VIDEO_SIZE_CIF_H;
	s->frame_count = 0;
    s->cfg.g_w = s->width;
    s->cfg.g_h = s->height;
	/* encoder automatically places keyframes */
	s->cfg.kf_mode = VPX_KF_AUTO;
	s->cfg.kf_max_dist = 360;
	s->cfg.rc_target_bitrate = 250;
	s->cfg.g_pass = VPX_RC_ONE_PASS; /* -p 1 */
	s->fps=15;
	s->cfg.g_timebase.num = 1;
	s->cfg.g_timebase.den = s->fps;
	s->cfg.rc_end_usage = VPX_CBR; /* --end-usage=cbr */
	s->cfg.g_threads = 4; /* -t 4 */
	s->cfg.rc_undershoot_pct = 95; /* --undershoot-pct=95 */
#if 0
	s->cfg.rc_buf_sz = 6; /* --buf-sz=6 */
	s->cfg.rc_buf_initial_sz = 4; /* --buf-initial=4 */
	s->cfg.rc_buf_optimal_sz = 5; /* --buf-optimal=5 */
	s->cfg.rc_min_quantizer = 0; //30;
	s->cfg.rc_max_quantizer = 63;
	s->cfg.rc_dropframe_thresh = 70;
	s->cfg.rc_resize_allowed = 1;
#endif
	s->mtu=ms_get_payload_max_size()-1;/*-1 for the vp8 payload header*/

	f->data = s;
}

static void enc_uninit(MSFilter *f) {
	EncState *s=(EncState*)f->data;
	vpx_codec_destroy(&s->codec);
	ms_free(s);
}

static void enc_preprocess(MSFilter *f) {
	vpx_codec_err_t res;
	EncState *s=(EncState*)f->data;

	s->cfg.rc_target_bitrate = MS_VIDEO_SIZE_CIF_W * MS_VIDEO_SIZE_CIF_H * s->cfg.rc_target_bitrate
                            / s->cfg.g_w / s->cfg.g_h;
	s->cfg.g_w = s->width;
	s->cfg.g_h = s->height;

	/* Initialize codec */
	res =  vpx_codec_enc_init(&s->codec, interface, &s->cfg, 0);
	if (res) {
		ms_error("vpx_codec_enc_init failed: %s (%s)n", vpx_codec_err_to_string(res), vpx_codec_error_detail(&s->codec));
	}

	vpx_codec_control(&s->codec, VP8E_SET_CPUUSED, 4); /* --cpu-used=4 */
	vpx_codec_control(&s->codec, VP8E_SET_STATIC_THRESHOLD, 0);
	vpx_codec_control(&s->codec, VP8E_SET_ENABLEAUTOALTREF, 1);
	/* vpx_codec_control(&s->codec, VP8E_SET_CPUUSED, 0);*/ /* -16 (quality) .. 16 (speed) */
	/* scaling mode
	vpx_scaling_mode_t scaling;
	scaling.h_scaling_mode = VP8E_ONETWO;
	scaling.v_scaling_mode = VP8E_ONETWO;
	res = vpx_codec_control(&s->codec, VP8E_SET_SCALEMODE, &scaling);
	if (res) {
		ms_error("vpx_codec_control(VP8E_SET_SCALEMODE) failed: %s (%s)n", vpx_codec_err_to_string(res), vpx_codec_error_detail(&s->codec));
	}
	*/
}

static void enc_process(MSFilter *f) {
	mblk_t *im,*om;
	uint64_t timems=f->ticker->time;
	uint32_t timestamp=timems*90;
	EncState *s=(EncState*)f->data;
	unsigned int flags = 0;
	vpx_codec_err_t err;

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		vpx_image_t img;

		om = NULL;
		flags = 0;

		vpx_img_wrap(&img, VPX_IMG_FMT_I420, s->width, s->height, 1, im->b_rptr);

		err = vpx_codec_encode(&s->codec, &img, s->frame_count, 1, flags, VPX_DL_REALTIME);
		s->frame_count++;
		if (err) {
			ms_error("vpx_codec_encode failed : %d %s (%s)\n", err, vpx_codec_err_to_string(err), vpx_codec_error_detail(&s->codec));
		} else {
			vpx_codec_iter_t iter = NULL;
			const vpx_codec_cx_pkt_t *pkt;

			while( (pkt = vpx_codec_get_cx_data(&s->codec, &iter)) ) {
				if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
					if (pkt->data.frame.sz > 0) {
#if 0
					om = esballoc((uint8_t*) pkt->data.frame.buf, pkt->data.frame.sz, 0, NULL);
#else
					om = allocb(pkt->data.frame.sz,0);
					memcpy(om->b_wptr, pkt->data.frame.buf, pkt->data.frame.sz);
					om->b_wptr += pkt->data.frame.sz;
#endif
					vp8_fragment_and_send(f, s, om, timestamp, pkt);
					}
				}
			}
		}
		freemsg(im);
	}
}

static void enc_postprocess(MSFilter *f) {

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

static int enc_set_br(MSFilter *f, void*data){
	int br=*(int*)data;
	EncState *s=(EncState*)f->data;
	s->cfg.rc_target_bitrate = br / 1024;
	return 0;
}

static int enc_set_mtu(MSFilter *f, void*data){
	EncState *s=(EncState*)f->data;
	s->mtu=*(int*)data;
	return 0;
}

static MSFilterMethod enc_methods[]={
	{	MS_FILTER_SET_VIDEO_SIZE, enc_set_vsize },
	{	MS_FILTER_SET_FPS,	  enc_set_fps	},
	{	MS_FILTER_GET_VIDEO_SIZE, enc_get_vsize },
	{	MS_FILTER_GET_FPS,	  enc_get_fps	},
	{	MS_FILTER_ADD_ATTR,       enc_add_attr	},
	{	MS_FILTER_SET_BITRATE,    enc_set_br	},
	{	MS_FILTER_SET_MTU,        enc_set_mtu	},
	{	0			, NULL }
};

#ifdef _MSC_VER
MSFilterDesc ms_vp8_enc_desc={
	MS_VP8_ENC_ID,
	"MSVp8Enc",
	N_("A video VP8 encoder using libvpx library."),
	MS_FILTER_ENCODER,
	"VP8-DRAFT-0-3-2",
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
	.enc_fmt="VP8-DRAFT-0-3-2",
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


static void vp8_fragment_and_send(MSFilter *f,EncState *s,mblk_t *frame, uint32_t timestamp, const vpx_codec_cx_pkt_t *pkt){
	uint8_t *rptr;
	mblk_t *packet=NULL;
	mblk_t* vp8_payload_desc = NULL;
	int len;
	bool_t fragmented = (frame->b_wptr-frame->b_rptr > s->mtu);

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
		/* RSV field, always 0 */
		(*vp8_payload_desc->b_rptr) &= ~VP8_PAYLOAD_DESC_RSV_MASK;
		/* I (1 if picture ID is present) */
		/* (*vp8_payload_desc->b_rptr) |= 0x10; */
		/* N : set to 1 if non reference frame */
		if ((pkt->data.frame.flags & VPX_FRAME_IS_KEY) == 0)
			(*vp8_payload_desc->b_rptr) |= VP8_PAYLOAD_DESC_I_MASK;
		/* FI : partition fragmentation information */
		/* (currently frame frag) */
		if (fragmented) {
			if (rptr == frame->b_rptr) {
				/* first fragment */
				(*vp8_payload_desc->b_rptr) |= VP8_PAYLOAD_DESC_FI_BEG_PARTITION;
			} else if ((rptr+len)>=frame->b_wptr) {
				/* last one */
				(*vp8_payload_desc->b_rptr) |= VP8_PAYLOAD_DESC_FI_END_PARTITION;
			} else {
				(*vp8_payload_desc->b_rptr) |= VP8_PAYLOAD_DESC_FI_MID_PARTITION;
			}
		}
		/* B : frame beginning */
		if (rptr == frame->b_rptr) {
			(*vp8_payload_desc->b_rptr) |= VP8_PAYLOAD_DESC_B_MASK;
		}

		vp8_payload_desc->b_cont = packet;

		ms_queue_put(f->outputs[0], vp8_payload_desc);
		rptr+=len;
	}

	freeb(frame);

	/*set marker bit on last packet*/
	mblk_set_marker_info(packet,TRUE);
	mblk_set_marker_info(vp8_payload_desc,TRUE);
}

#undef interface
#include <assert.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
#define interface (vpx_codec_vp8_dx())

typedef struct DecState {
	vpx_codec_ctx_t codec;
	mblk_t *curframe;
	struct SwsContext * scale_ctx;
	int scale_from_w, scale_from_h;
} DecState;


static void dec_init(MSFilter *f) {
	DecState *s=(DecState *)ms_new(DecState,1);

	ms_message("Using %s\n",vpx_codec_iface_name(interface));

	/* Initialize codec */
	if(vpx_codec_dec_init(&s->codec, interface, NULL, 0))
		ms_error("Failed to initialize decoder");

	s->curframe = NULL;
	s->scale_ctx = NULL;
	s->scale_from_w = s->scale_from_h = 0;
	f->data = s;
}

static void dec_preprocess(MSFilter* f) {

}

static void dec_uninit(MSFilter *f) {
	DecState *s=(DecState*)f->data;
	vpx_codec_destroy(&s->codec);

	if (s->curframe!=NULL)
		freemsg(s->curframe);

	if (s->scale_ctx != NULL)
		sws_freeContext(s->scale_ctx);

	ms_free(s);
}

/* remove payload header and agregates fragmented packets */
static mblk_t *dec_unpacketize(MSFilter *f, DecState *s, mblk_t *im, bool_t* is_key_frame){
	uint8_t vp8_payload_desc = *im->b_rptr++;
	(*is_key_frame) = !(vp8_payload_desc & VP8_PAYLOAD_DESC_I_MASK);

	if ((vp8_payload_desc & VP8_PAYLOAD_DESC_FI_MASK) == 0) {
		return im;
	}

	if ((vp8_payload_desc & VP8_PAYLOAD_DESC_FI_MASK)
			== VP8_PAYLOAD_DESC_FI_BEG_PARTITION) {
		if (s->curframe!=NULL)
			freemsg(s->curframe);
		s->curframe=im;
	} else if ((vp8_payload_desc & VP8_PAYLOAD_DESC_FI_MASK)
			== VP8_PAYLOAD_DESC_FI_MID_PARTITION) {
			if (s->curframe!=NULL)
				concatb(s->curframe,im);
			else
				freemsg(im);
	} else {
		assert((vp8_payload_desc & VP8_PAYLOAD_DESC_FI_MASK)
			== VP8_PAYLOAD_DESC_FI_END_PARTITION);
		if (s->curframe!=NULL){
			mblk_t *ret;
			concatb(s->curframe,im);
			msgpullup(s->curframe,-1);
			ret=s->curframe;
			s->curframe=NULL;
			return ret;
		}else
			freemsg(im);
	}
	return NULL;
}

static unsigned int compute_scaled_size(int size, VPX_SCALING_MODE scale) {
	switch (scale) {
		case VP8E_FOURFIVE:
			return (size * 4) / 5;
		case VP8E_THREEFIVE:
			return (size * 3) / 5;
		case VP8E_ONETWO:
			return (size * 1) / 2;
		case VP8E_NORMAL:
		default:
			return size;
	}
}

static bool_t check_swscale_init(DecState *s, mblk_t *m, int w, int h, bool_t is_key_frame) {
	unsigned char h_scaling_mode, v_scaling_mode;
	unsigned int input_w, input_h;

	if (!is_key_frame) {
		return (s->scale_ctx != NULL);
	}

	/* see decodframe.c or vp8 bitstream guide */
	h_scaling_mode = m->b_rptr[3+4] >> 6;
	v_scaling_mode = m->b_rptr[3+6] >> 6;

	input_w = compute_scaled_size(w, h_scaling_mode);
	input_h = compute_scaled_size(h, v_scaling_mode);

	/* check if sws_context is properly initialized */
	if (s->scale_from_w != input_w || s->scale_from_h != input_h) {
		/* we need a key frame */
		if (!is_key_frame) {
			freemsg(m);
			return FALSE;
		}

		if (s->scale_ctx != NULL)
			sws_freeContext(s->scale_ctx);
		s->scale_ctx = sws_getContext(
				input_w, input_h, PIX_FMT_YUV420P,
				MS_VIDEO_SIZE_CIF_W, MS_VIDEO_SIZE_CIF_H, PIX_FMT_YUV420P,
				SWS_FAST_BILINEAR|SWS_CPU_CAPS_MMX2,
				NULL, NULL, NULL);
		s->scale_from_w = input_w;
		s->scale_from_h = input_h;
	}
	return TRUE;
}

static void dec_process(MSFilter *f) {
	mblk_t *im;
	mblk_t *m;
	vpx_codec_err_t err;
	DecState *s=(DecState*)f->data;
	bool_t is_key_frame = FALSE;
	const int size = (MS_VIDEO_SIZE_CIF_W * MS_VIDEO_SIZE_CIF_H * 3) / 2;

	while( (im=ms_queue_get(f->inputs[0]))!=0) {
		m = dec_unpacketize(f, s, im, &is_key_frame);
		if (m!=NULL){
			vpx_codec_iter_t  iter = NULL;
			vpx_image_t      *img;

			err = vpx_codec_decode(&s->codec, m->b_rptr, m->b_wptr - m->b_rptr, NULL, 0);
			if (err) {
				ms_warning("vpx_codec_decode failed : %d %s (%s)\n", err, vpx_codec_err_to_string(err), vpx_codec_error_detail(&s->codec));
			}

			/* browse decoded frame */
			while((img = vpx_codec_get_frame(&s->codec, &iter))) {
				mblk_t* om;
				vpx_image_t out_img;

				if (!check_swscale_init(s, m, img->d_w, img->d_h, is_key_frame)) {
					continue;
				}

				/* scale/copy frame to destination mblk_t */
				om = allocb(size, 0);
				vpx_img_wrap(&out_img, VPX_IMG_FMT_I420, MS_VIDEO_SIZE_CIF_W, MS_VIDEO_SIZE_CIF_H, 1, om->b_wptr);
				assert(sws_scale(s->scale_ctx, img->planes, img->stride, 0, MS_VIDEO_SIZE_CIF_H, out_img.planes, out_img.stride) == MS_VIDEO_SIZE_CIF_H);
				om->b_wptr += size;

				ms_queue_put(f->outputs[0],om);
			}

			freemsg(m);
		}
	}
}

#ifdef _MSC_VER
MSFilterDesc ms_vp8_dec_desc={
	MS_VP8_DEC_ID,
	"MSVp8Dec",
	"A VP8 decoder using libvpx library",
	MS_FILTER_DECODER,
	"VP8-DRAFT-0-3-2",
	1,
	1,
	dec_init,
	dec_preprocess,
	dec_process,
	NULL,
	dec_uninit,
	NULL
};
#else
MSFilterDesc ms_vp8_dec_desc={
	.id=MS_VP8_DEC_ID,
	.name="MSVp8Dec",
	.text="A VP8 decoder using libvpx library",
	.category=MS_FILTER_DECODER,
	.enc_fmt="VP8-DRAFT-0-3-2",
	.ninputs=1,
	.noutputs=1,
	.init=dec_init,
	.preprocess=dec_preprocess,
	.process=dec_process,
	.postprocess=NULL,
	.uninit=dec_uninit,
	.methods=NULL
};
#endif
MS_FILTER_DESC_EXPORT(ms_vp8_dec_desc)
