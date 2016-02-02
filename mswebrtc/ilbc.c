/*
 ilbc.c
 Copyright (C) 2015 Belledonne Communications, Grenoble, France

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

#include "ilbc.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msticker.h"

#ifdef HAVE_ms_bufferizer_fill_current_metas
#define ms_bufferizer_fill_current_metas(b,m) ms_bufferizer_fill_current_metas(b,m)
#else
#define ms_bufferizer_fill_current_metas(b,m)
#endif

#define BLOCKL_20MS         160
#define BLOCKL_30MS         240
#define BLOCKL_MAX          240
#define NO_OF_BYTES_20MS     38
#define NO_OF_BYTES_30MS     50

typedef struct EncState{
	int nsamples;
	int nbytes;
	int ms_per_frame;
	int ptime;
	uint32_t ts;
	MSBufferizer *bufferizer;
	IlbcEncoderInstance *ilbc_enc;
}EncState;

static void enc_init(MSFilter *f){
	EncState *s=ms_new0(EncState,1);
#ifdef USE_20MS_FRAMES
	s->nsamples=BLOCKL_20MS;
	s->nbytes=NO_OF_BYTES_20MS;
	s->ms_per_frame=20;
#else
	s->nsamples=BLOCKL_30MS;
	s->nbytes=NO_OF_BYTES_30MS;
	s->ms_per_frame=30;
#endif
	s->ts = 0;
	s->bufferizer=ms_bufferizer_new();
	WebRtcIlbcfix_EncoderCreate(&s->ilbc_enc);
	f->data=s;
}

static void enc_uninit(MSFilter *f){
	EncState *s=(EncState*)f->data;
	ms_bufferizer_destroy(s->bufferizer);
	WebRtcIlbcfix_EncoderFree(s->ilbc_enc);
	ms_free(f->data);
}

static void enc_preprocess(MSFilter *f){
	EncState *s=(EncState*)f->data;
	WebRtcIlbcfix_EncoderInit(s->ilbc_enc, s->ms_per_frame);
}

static int enc_add_fmtp(MSFilter *f, void *arg){
	char buf[64];
	const char *fmtp=(const char *)arg;
	EncState *s=(EncState*)f->data;

	memset(buf, '\0', sizeof(buf));
	if (fmtp_get_value(fmtp, "mode", buf, sizeof(buf))){
		if (buf[0]=='\0'){
			ms_warning("unsupported fmtp parameter (%s)!", fmtp);
			return 0;
		}
		ms_message("iLBC encoder got mode=%s",buf);
		if (strstr(buf,"20")!=NULL){
			s->nsamples=BLOCKL_20MS;
			s->nbytes=NO_OF_BYTES_20MS;
			s->ms_per_frame=20;
		}else if (strstr(buf,"30")!=NULL){
			s->nsamples=BLOCKL_30MS;
			s->nbytes=NO_OF_BYTES_30MS;
			s->ms_per_frame=30;
		}
	}
	if (fmtp_get_value(fmtp,"ptime",buf,sizeof(buf))){
		int ptime;
		if (buf[0]=='\0'){
			ms_warning("unsupported fmtp parameter (%s)!", fmtp);
			return 0;
		}
		ms_message("iLBC encoder got ptime=%s",buf);
		ptime=atoi(buf);
		if (ptime>=20 && ptime<=140){
			s->ptime=ptime;
		}
	}
	return 0;
}

static int enc_add_attr(MSFilter *f, void *arg){
	const char *fmtp=(const char *)arg;
	EncState *s=(EncState*)f->data;
	if (strstr(fmtp,"ptime:20")!=NULL){
		s->ptime=20;
	}else if (strstr(fmtp,"ptime:30")!=NULL){
		s->ptime=30;
	}else if (strstr(fmtp,"ptime:40")!=NULL){
		s->ptime=40;
	}else if (strstr(fmtp,"ptime:60")!=NULL){
		s->ptime=60;
	}else if (strstr(fmtp,"ptime:80")!=NULL){
		s->ptime=80;
	}else if (strstr(fmtp,"ptime:90")!=NULL){
		s->ptime=90;
	}else if (strstr(fmtp,"ptime:100")!=NULL){
		s->ptime=100;
	}else if (strstr(fmtp,"ptime:120")!=NULL){
		s->ptime=120;
	}else if (strstr(fmtp,"ptime:140")!=NULL){
		s->ptime=140;
	}
	return 0;
}

static void enc_process(MSFilter *f){
	EncState *s=(EncState*)f->data;
	mblk_t *im,*om;
	int size=s->nsamples*2;
	int16_t samples[BLOCKL_MAX * 7]; /* BLOCKL_MAX * 7 is the largest size for ptime == 140 */
	int16_t *samples_ptr;
	int frame_per_packet=1;
	int k;

	if (s->ptime>=20 && s->ms_per_frame>0 && s->ptime%s->ms_per_frame==0)
	{
		frame_per_packet = s->ptime/s->ms_per_frame;
	}

	if (frame_per_packet<=0)
		frame_per_packet=1;
	if (frame_per_packet>7) /* 7*20 == 140 ms max */
		frame_per_packet=7;

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		ms_bufferizer_put(s->bufferizer,im);
	}
	while(ms_bufferizer_read(s->bufferizer,(uint8_t*)samples,size*frame_per_packet)==(size*frame_per_packet)){
		om=allocb(s->nbytes*frame_per_packet,0);
		for (k=0;k<frame_per_packet;k++) {
			samples_ptr = samples + k * s->nsamples;
			WebRtcIlbcfix_Encode(s->ilbc_enc, samples_ptr, s->nsamples, om->b_wptr);
			om->b_wptr+=s->nbytes;
		}
		s->ts+=s->nsamples*frame_per_packet;
		mblk_set_timestamp_info(om,s->ts);
		ms_bufferizer_fill_current_metas(s->bufferizer,om);
		ms_queue_put(f->outputs[0],om);
	}
}

static MSFilterMethod enc_methods[]={
	{	MS_FILTER_ADD_FMTP,		enc_add_fmtp },
	{	MS_FILTER_ADD_ATTR,		enc_add_attr},
	{	0		,		NULL	}
};

#ifdef _MSC_VER

MSFilterDesc ms_webrtc_ilbc_enc_desc={
	MS_FILTER_PLUGIN_ID,
	"MSWebRtcIlbcEnc",
	"WebRtc's iLBC encoder",
	MS_FILTER_ENCODER,
	"iLBC",
	1,
	1,
	enc_init,
	enc_preprocess,
	enc_process,
	NULL,
	enc_uninit,
	enc_methods
};

#else

MSFilterDesc ms_webrtc_ilbc_enc_desc={
	.id=MS_FILTER_PLUGIN_ID,
	.name="MSWebRtcIlbcEnc",
	.text="WebRtc's iLBC encoder",
	.category=MS_FILTER_ENCODER,
	.enc_fmt="iLBC",
	.ninputs=1,
	.noutputs=1,
	.init=enc_init,
	.preprocess=enc_preprocess,
	.process=enc_process,
	.uninit=enc_uninit,
	.methods=enc_methods
};

#endif

typedef struct DecState{
	int nsamples;
	int nbytes;
	int ms_per_frame;
	IlbcDecoderInstance *ilbc_dec;
	bool_t ready;
	MSConcealerContext *plcctx;
}DecState;


static void dec_init(MSFilter *f){
	DecState *s=ms_new0(DecState,1);
	s->nsamples=0;
	s->nbytes=0;
	s->ms_per_frame=0;
	WebRtcIlbcfix_DecoderCreate(&s->ilbc_dec);
	s->ready=FALSE;
	s->plcctx=ms_concealer_context_new(200);
	f->data=s;
}

static void dec_uninit(MSFilter *f){
	DecState *s=(DecState*)f->data;
	WebRtcIlbcfix_DecoderFree(s->ilbc_dec);
	if (s->plcctx) ms_concealer_context_destroy(s->plcctx);
	ms_free(s);
}

static void dec_process(MSFilter *f){
	DecState *s=(DecState*)f->data;
	mblk_t *im,*om;
	int nbytes;

	while ((im=ms_queue_get(f->inputs[0]))!=NULL){
		nbytes=msgdsize(im);
		if (nbytes==0  || (nbytes%NO_OF_BYTES_20MS!=0 && nbytes%NO_OF_BYTES_30MS!=0)){
			freemsg(im);
			continue;
		}
		if (nbytes%NO_OF_BYTES_20MS==0 && s->nbytes!=NO_OF_BYTES_20MS) {
			/* not yet configured, or misconfigured */
			s->ms_per_frame=20;
			s->nbytes=NO_OF_BYTES_20MS;
			s->nsamples=BLOCKL_20MS;
			s->ready=TRUE;
			WebRtcIlbcfix_DecoderInit(s->ilbc_dec, s->ms_per_frame);
		}
		else if (nbytes%NO_OF_BYTES_30MS==0 && s->nbytes!=NO_OF_BYTES_30MS) {
			/* not yet configured, or misconfigured */
			s->ms_per_frame=30;
			s->nbytes=NO_OF_BYTES_30MS;
			s->nsamples=BLOCKL_30MS;
			s->ready=TRUE;
			WebRtcIlbcfix_DecoderInit(s->ilbc_dec, s->ms_per_frame);
		}
		if (s->nbytes>0 && nbytes>=s->nbytes){
			int frame_per_packet = nbytes/s->nbytes;
			int k;
			int plctime;

			for (k=0;k<frame_per_packet;k++) {
				int16_t speech_type;
				om=allocb(s->nsamples*2,0);
				WebRtcIlbcfix_Decode(s->ilbc_dec, im->b_rptr+k*s->nbytes, s->nbytes, (int16_t *)om->b_wptr, &speech_type);
				om->b_wptr += s->nsamples * 2;
				mblk_meta_copy(im,om);
				ms_queue_put(f->outputs[0],om);
			}
			if (s->plcctx){
				plctime=ms_concealer_inc_sample_time(s->plcctx,f->ticker->time,frame_per_packet*s->ms_per_frame,1);
				if (plctime>0){
					ms_warning("ilbc: did plc during %i ms",plctime);
				}
			}
		}else{
			ms_warning("bad iLBC frame !");
		}
		freemsg(im);
	}
	if (s->plcctx && s->ready && ms_concealer_context_is_concealement_required(s->plcctx,f->ticker->time)){
		om=allocb(s->nsamples*2,0);
		WebRtcIlbcfix_DecodePlc(s->ilbc_dec, (int16_t *)om->b_wptr, 1);
		om->b_wptr += s->nsamples * 2;
		mblk_set_plc_flag(om,TRUE);
		ms_queue_put(f->outputs[0],om);
		ms_concealer_inc_sample_time(s->plcctx,f->ticker->time,s->ms_per_frame,0);
	}
}

static int dec_have_plc(MSFilter *f, void *arg){
	DecState *s=(DecState*)f->data;
	*((int *)arg) = (s->plcctx!=NULL);
	return 0;
}

static MSFilterMethod dec_methods[]={
	{	MS_DECODER_HAVE_PLC,		dec_have_plc },
	{	0		,		NULL	}
};

#ifdef _MSC_VER

MSFilterDesc ms_webrtc_ilbc_dec_desc={
	MS_FILTER_PLUGIN_ID,
	"MSWebRtcIlbcDec",
	"WebRtc's iLBC decoder",
	MS_FILTER_DECODER,
	"iLBC",
	1,
	1,
	dec_init,
	NULL,
	dec_process,
	NULL,
	dec_uninit,
	dec_methods,
	MS_FILTER_IS_PUMP
};

#else

MSFilterDesc ms_webrtc_ilbc_dec_desc={
	.id=MS_FILTER_PLUGIN_ID,
	.name="MSWebRtcIlbcDec",
	.text="WebRtc's iLBC decoder",
	.category=MS_FILTER_DECODER,
	.enc_fmt="iLBC",
	.ninputs=1,
	.noutputs=1,
	.init=dec_init,
	.process=dec_process,
	.uninit=dec_uninit,
	.methods=dec_methods,
	.flags=MS_FILTER_IS_PUMP
};

#endif

