/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"

#include <speex/speex.h>

#ifdef ANDROID
#include "cpu-features.h"
#endif

#ifdef _WIN32
#include <malloc.h> /* for alloca */
#endif

typedef struct SpeexEncState{
	int rate;
	int bitrate;
	int maxbitrate; /*ip bitrate*/
	int ip_bitrate; /*effective ip bitrate */
	int ptime;
	int vbr;
	int cng;
	int mode;
	int frame_size;
	void *state;
	uint32_t ts;
	MSBufferizer *bufferizer;
} SpeexEncState;

static void enc_init(MSFilter *f){
	SpeexEncState *s=ms_new0(SpeexEncState,1);
#ifdef SPEEX_LIB_SET_CPU_FEATURES
	int cpuFeatures = 0;
#endif
	s->rate=8000;
	s->bitrate=-1;
	s->maxbitrate=-1;
	s->ip_bitrate=-1;
	s->ptime=20;
	s->mode=-1;
	s->vbr=0;
	s->cng=0;
	s->frame_size=0;
	s->state=0;
	s->ts=0;
	s->bufferizer=ms_bufferizer_new();
	f->data=s;

#ifdef SPEEX_LIB_SET_CPU_FEATURES
#if MS_HAS_ARM_NEON
	#ifdef ANDROID
	if (((android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM) && ((android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0))
		|| (android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM64)) {
		cpuFeatures = SPEEX_LIB_CPU_FEATURE_NEON;
	}
	#else
	cpuFeatures = SPEEX_LIB_CPU_FEATURE_NEON;
	#endif
#endif
	ms_message("speex_lib_ctl init %s neon", (cpuFeatures == SPEEX_LIB_CPU_FEATURE_NEON)?"with":"without");
	speex_lib_ctl(SPEEX_LIB_SET_CPU_FEATURES, &cpuFeatures);
#else
	ms_message("speex_lib_ctl does not support SPEEX_LIB_CPU_FEATURE_NEON");
#endif
}

static void enc_uninit(MSFilter *f){
	SpeexEncState *s=(SpeexEncState*)f->data;
	if (s==NULL)
		return;
	ms_bufferizer_destroy(s->bufferizer);
	if (s->state!=NULL)
		speex_encoder_destroy(s->state);
	ms_free(s);
}

static void apply_max_bitrate(SpeexEncState *s){
	int pps=1000/s->ptime;

	if (s->maxbitrate>0){
		/* convert from network bitrate to codec bitrate:*/
		/* ((nbr/(pps*8)) -20-12-8)*pps*8*/
		int cbr=(int)( ((((float)s->maxbitrate)/(pps*8))-20-12-8)*pps*8);
		ms_message("Setting maxbitrate=%i to speex encoder.",cbr);
		if (speex_encoder_ctl(s->state,SPEEX_SET_BITRATE,&cbr)!=0){
			ms_error("Could not set maxbitrate %i to speex encoder.",s->bitrate);
		}
	}
	if (speex_encoder_ctl(s->state,SPEEX_GET_BITRATE,&s->bitrate)!=0){
			ms_error("Could not get bitrate %i to speex encoder.",s->bitrate);
	}else{
		/*convert from codec bitrate to network bitrate */
		s->ip_bitrate=( (s->bitrate/(pps*8))+20+12+8)*8*pps;
		ms_message("Using bitrate %i for speex encoder, ip bitrate is %i",s->bitrate,s->ip_bitrate);
	}
}

static void enc_preprocess(MSFilter *f){
	SpeexEncState *s=(SpeexEncState*)f->data;
	const SpeexMode *mode=NULL;
	int _mode=0;

	switch(s->rate){
		case 8000:
	        _mode = SPEEX_MODEID_NB;    /* rate = 8000Hz */
			break;
		case 16000:
	        _mode = SPEEX_MODEID_WB;    /* rate = 16000Hz */
			break;
			/* should be supported in the future */
		case 32000:
	        _mode = SPEEX_MODEID_UWB;   /* rate = 32000Hz */
			break;
		default:
			ms_error("Unsupported rate for speex encoder (back to default rate=8000).");
			s->rate=8000;
	}
	/* warning: speex_lib_get_mode() is not available on speex<1.1.12 */
	mode = speex_lib_get_mode(_mode);

	if (mode==NULL)
		return;
	s->state=speex_encoder_init(mode);

	if (s->vbr==1)
	{
		if (speex_encoder_ctl(s->state,SPEEX_SET_VBR,&s->vbr)!=0){
			ms_error("Could not set vbr mode to speex encoder.");
		}
		/* implicit VAD */
		speex_encoder_ctl (s->state, SPEEX_SET_DTX, &s->vbr);
	}
	else if (s->vbr==2)
	{
		int vad=1;
		/* VAD */
		speex_encoder_ctl (s->state, SPEEX_SET_VAD, &vad);
		speex_encoder_ctl (s->state, SPEEX_SET_DTX, &vad);
	}
	else if (s->cng==1)
	{
		speex_encoder_ctl (s->state, SPEEX_SET_VAD, &s->cng);
	}

	if (s->rate==8000){
		//+------+---------------+-------------+
		//| mode | Speex quality |   bit-rate  |
		//+------+---------------+-------------+
		//|   1  |       0       | 2.15 kbit/s |
		//|   2  |       2       | 5.95 kbit/s |
		//|   3  |     3 or 4    | 8.00 kbit/s |
		//|   4  |     5 or 6    | 11.0 kbit/s |
		//|   5  |     7 or 8    | 15.0 kbit/s |
		//|   6  |       9       | 18.2 kbit/s |
		//|   7  |      10       | 24.6 kbit/s |
		//|   8  |       1       | 3.95 kbit/s |
		//+------+---------------+-------------+
		if (s->mode<=0 || s->mode>8)
			s->mode = 3; /* default mode */

		if (s->mode==1)
			s->bitrate = 2150;
		else if (s->mode==2)
			s->bitrate = 5950;
		else if (s->mode==3)
			s->bitrate = 8000;
		else if (s->mode==4)
			s->bitrate = 11000;
		else if (s->mode==5)
			s->bitrate = 15000;
		else if (s->mode==6)
			s->bitrate = 18200;
		else if (s->mode==7)
			s->bitrate = 24600;
		else if (s->mode==8)
			s->bitrate = 3950;

		if (s->bitrate!=-1){
			if (speex_encoder_ctl(s->state,SPEEX_SET_BITRATE,&s->bitrate)!=0){
				ms_error("Could not set bitrate %i to speex encoder.",s->bitrate);
			}
		}
	}
	else if (s->rate==16000 || s->rate==32000){
		//+------+---------------+-------------------+------------------------+
		//| mode | Speex quality | wideband bit-rate |     ultra wideband     |
		//|      |               |                   |        bit-rate        |
		//+------+---------------+-------------------+------------------------+
		//|   0  |       0       |    3.95 kbit/s    |       5.75 kbit/s      |
		//|   1  |       1       |    5.75 kbit/s    |       7.55 kbit/s      |
		//|   2  |       2       |    7.75 kbit/s    |       9.55 kbit/s      |
		//|   3  |       3       |    9.80 kbit/s    |       11.6 kbit/s      |
		//|   4  |       4       |    12.8 kbit/s    |       14.6 kbit/s      |
		//|   5  |       5       |    16.8 kbit/s    |       18.6 kbit/s      |
		//|   6  |       6       |    20.6 kbit/s    |       22.4 kbit/s      |
		//|   7  |       7       |    23.8 kbit/s    |       25.6 kbit/s      |
		//|   8  |       8       |    27.8 kbit/s    |       29.6 kbit/s      |
		//|   9  |       9       |    34.2 kbit/s    |       36.0 kbit/s      |
		//|  10  |       10      |    42.2 kbit/s    |       44.0 kbit/s      |
		//+------+---------------+-------------------+------------------------+
		int q=0;
		if (s->mode<0 || s->mode>10)
			s->mode = 8; /* default mode */
		q=s->mode;
		if (speex_encoder_ctl(s->state,SPEEX_SET_QUALITY,&q)!=0){
			ms_error("Could not set quality %i to speex encoder.",q);
		}
	}
	apply_max_bitrate(s);


	speex_mode_query(mode,SPEEX_MODE_FRAME_SIZE,&s->frame_size);
}

static void enc_process(MSFilter *f){
	SpeexEncState *s=(SpeexEncState*)f->data;
	mblk_t *im;
	int nbytes;
	uint8_t *buf;
	int frame_per_packet=1;

	if (s->frame_size<=0)
		return;

	ms_filter_lock(f);

	if (s->ptime>=20)
	{
		frame_per_packet = s->ptime/20;
	}

	if (frame_per_packet<=0)
		frame_per_packet=1;
	if (frame_per_packet>7) /* 7*20 == 140 ms max */
		frame_per_packet=7;

	nbytes=s->frame_size*2;
	buf=(uint8_t*)alloca(nbytes*frame_per_packet);

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		ms_bufferizer_put(s->bufferizer,im);
	}
	while(ms_bufferizer_read(s->bufferizer,buf,nbytes*frame_per_packet)==(size_t)(nbytes*frame_per_packet)){
		mblk_t *om=allocb(nbytes*frame_per_packet,0);//too large...
		int k;
		SpeexBits bits;
		speex_bits_init(&bits);
		for (k=0;k<frame_per_packet;k++)
		{
			speex_encode_int(s->state,(int16_t*)(buf + (k*s->frame_size*2)),&bits);
			s->ts+=s->frame_size;
		}
		speex_bits_insert_terminator(&bits);
		k=speex_bits_write(&bits, (char*)om->b_wptr, nbytes*frame_per_packet);
		om->b_wptr+=k;

		mblk_set_timestamp_info(om,s->ts-s->frame_size);
		ms_bufferizer_fill_current_metas(s->bufferizer, om);
		ms_queue_put(f->outputs[0],om);
		speex_bits_destroy(&bits);
	}
	ms_filter_unlock(f);
}

static void enc_postprocess(MSFilter *f){
	SpeexEncState *s=(SpeexEncState*)f->data;
	speex_encoder_destroy(s->state);
	s->state=NULL;
}

static int enc_set_sr(MSFilter *f, void *arg){
	SpeexEncState *s=(SpeexEncState*)f->data;
	s->rate=((int*)arg)[0];
	return 0;
}

static int enc_get_sr(MSFilter *f, void *arg){
	SpeexEncState *s=(SpeexEncState*)f->data;
	((int*)arg)[0]=s->rate;
	return 0;
}

static int enc_set_br(MSFilter *f, void *arg){
	SpeexEncState *s=(SpeexEncState*)f->data;
	ms_filter_lock(f);
	s->maxbitrate=((int*)arg)[0];
	if (s->state) apply_max_bitrate(s);
	ms_filter_unlock(f);
	return 0;
}

static int enc_get_br(MSFilter *f, void *arg){
	SpeexEncState *s=(SpeexEncState*)f->data;
	((int*)arg)[0]=s->ip_bitrate;
	return 0;
}

static int enc_set_ptime(MSFilter *f, void *arg){
	SpeexEncState *s=(SpeexEncState*)f->data;
	s->ptime=*(int*)arg;
	/*if the ptime is not a mulptiple of 20, go to the next multiple*/
	if (s->ptime%20)
		s->ptime = s->ptime - s->ptime%20 + 20;
	ms_message("MSSpeexEnc: got ptime=%i",s->ptime);
	return 0;
}

static int enc_get_ptime(MSFilter *f, void *arg){
	SpeexEncState *s=(SpeexEncState*)f->data;
	*(int*)arg=s->ptime;
	return 0;
}

static int enc_add_fmtp(MSFilter *f, void *arg){
	char buf[64];
	const char *fmtp=(const char *)arg;
	SpeexEncState *s=(SpeexEncState*)f->data;

	memset(buf, '\0', sizeof(buf));
	fmtp_get_value(fmtp, "vbr", buf, sizeof(buf));
	if (buf[0]=='\0'){
	}
	else if (strstr(buf,"off")!=NULL){
		s->vbr=0;
	}
	else if (strstr(buf,"on")!=NULL){
		s->vbr=1;
	}
	else if (strstr(buf,"vad")!=NULL){
		s->vbr=2;
	}

	memset(buf, '\0', sizeof(buf));
	fmtp_get_value(fmtp, "cng", buf, sizeof(buf));
	if (buf[0]=='\0'){
	}
	else if (strstr(buf,"off")!=NULL){
		s->cng=0;
	}
	else if (strstr(buf,"on")!=NULL){
		s->cng=1;
	}

	memset(buf, '\0', sizeof(buf));
	fmtp_get_value(fmtp, "mode", buf, sizeof(buf));
	if (buf[0]=='\0' || buf[1]=='\0'){
	}
	else if (buf[0]=='0' || (buf[0]=='"' && buf[1]=='0')){
		s->mode=0;
	}
	else if (buf[0]=='"' && atoi(buf+1)>=0){
		s->mode=atoi(buf+1);
	}
	else if (buf[0]!='"' && atoi(buf)>=0){
		s->mode=atoi(buf);
	}
	else {
		s->mode = -1; /* default mode */
	}
	memset(buf, '\0', sizeof(buf));
	if (fmtp_get_value(fmtp,"ptime",buf,sizeof(buf))){
		int val=atoi(buf);
		enc_set_ptime(f,&val);
	}

	return 0;
}

static int enc_add_attr(MSFilter *f, void *arg){
	const char *fmtp=(const char *)arg;
	SpeexEncState *s=(SpeexEncState*)f->data;
	if (strstr(fmtp,"ptime:10")!=NULL){
		s->ptime=20;
	}else if (strstr(fmtp,"ptime:20")!=NULL){
		s->ptime=20;
	}else if (strstr(fmtp,"ptime:30")!=NULL){
		s->ptime=40;
	}else if (strstr(fmtp,"ptime:40")!=NULL){
		s->ptime=40;
	}else if (strstr(fmtp,"ptime:50")!=NULL){
		s->ptime=60;
	}else if (strstr(fmtp,"ptime:60")!=NULL){
		s->ptime=60;
	}else if (strstr(fmtp,"ptime:70")!=NULL){
		s->ptime=80;
	}else if (strstr(fmtp,"ptime:80")!=NULL){
		s->ptime=80;
	}else if (strstr(fmtp,"ptime:90")!=NULL){
		s->ptime=100; /* not allowed */
	}else if (strstr(fmtp,"ptime:100")!=NULL){
		s->ptime=100;
	}else if (strstr(fmtp,"ptime:110")!=NULL){
		s->ptime=120;
	}else if (strstr(fmtp,"ptime:120")!=NULL){
		s->ptime=120;
	}else if (strstr(fmtp,"ptime:130")!=NULL){
		s->ptime=140;
	}else if (strstr(fmtp,"ptime:140")!=NULL){
		s->ptime=140;
	}
	return 0;
}

static int get_channels(MSFilter *f, void *arg) {
	*((int *)arg) = 1;
	return 0;
}

static MSFilterMethod enc_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	,	enc_set_sr	},
	{	MS_FILTER_GET_SAMPLE_RATE	,	enc_get_sr	},
	{	MS_FILTER_SET_BITRATE		,	enc_set_br	},
	{	MS_FILTER_GET_BITRATE		,	enc_get_br	},
	{	MS_FILTER_ADD_FMTP		,	enc_add_fmtp 	},
	{	MS_FILTER_ADD_ATTR		,	enc_add_attr	},
	{	MS_AUDIO_ENCODER_SET_PTIME	,	enc_set_ptime	},
	{	MS_AUDIO_ENCODER_GET_PTIME	,	enc_get_ptime	},
	{	MS_FILTER_GET_NCHANNELS		,	get_channels},
	{	0				,	NULL		}
};

#ifdef _MSC_VER

MSFilterDesc ms_speex_enc_desc={
	MS_SPEEX_ENC_ID,
	"MSSpeexEnc",
	N_("The free and wonderful speex codec"),
	MS_FILTER_ENCODER,
	"speex",
	1,
	1,
	enc_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	enc_methods
};

#else

MSFilterDesc ms_speex_enc_desc={
	.id=MS_SPEEX_ENC_ID,
	.name="MSSpeexEnc",
	.text=N_("The free and wonderful speex codec"),
	.category=MS_FILTER_ENCODER,
	.enc_fmt="speex",
	.ninputs=1,
	.noutputs=1,
	.init=enc_init,
	.preprocess=enc_preprocess,
	.postprocess=enc_postprocess,
	.process=enc_process,
	.uninit=enc_uninit,
	.methods=enc_methods
};

#endif

static const int plc_max=10;/*10 frames of plc, no more*/

typedef struct DecState{
	int rate;
	int penh;
	int frsz;
	uint64_t sample_time;
	void *state;
	int plc_count;
	bool_t plc;
} DecState;

static void dec_init(MSFilter *f){
	DecState *s=ms_new0(DecState,1);
	s->rate=8000;
	s->frsz=0;
	s->state=NULL;
	s->penh=1;
	s->sample_time=0;
	s->plc=1;
	s->plc_count=0;
	f->data=s;
}

static void dec_uninit(MSFilter *f){
	DecState *s=(DecState*)f->data;
	if (s->state!=NULL)
		speex_decoder_destroy(s->state);
	ms_free(s);
}

static void dec_preprocess(MSFilter *f){
	DecState *s=(DecState*)f->data;
	const SpeexMode *mode=NULL;
	int modeid;
	switch(s->rate){
		case 8000:
	        modeid = SPEEX_MODEID_NB;    /* rate = 8000Hz */
			break;
		case 16000:
	        modeid = SPEEX_MODEID_WB;    /* rate = 16000Hz */
			break;
			/* should be supported in the future */
		case 32000:
	        modeid = SPEEX_MODEID_UWB;   /* rate = 32000Hz */
			break;
		default:
			ms_error("Unsupported rate for speex decoder (back to default rate=8000).");
			modeid=SPEEX_MODEID_NB;
	}
	/* warning: speex_lib_get_mode() is not available on speex<1.1.12 */
	mode = speex_lib_get_mode(modeid);
	s->state=speex_decoder_init(mode);
	speex_mode_query(mode,SPEEX_MODE_FRAME_SIZE,&s->frsz);
	if (s->penh==1)
		speex_decoder_ctl (s->state, SPEEX_SET_ENH, &s->penh);
	s->sample_time=0;
}

static void dec_postprocess(MSFilter *f){
	DecState *s=(DecState*)f->data;
	speex_decoder_destroy(s->state);
	s->state=NULL;
}

static int dec_get_sr(MSFilter *f, void *arg){
	DecState *s=(DecState*)f->data;
	((int*)arg)[0]=s->rate;
	return 0;
}

static int dec_set_sr(MSFilter *f, void *arg){
	DecState *s=(DecState*)f->data;
	s->rate=((int*)arg)[0];
	return 0;
}

static int dec_add_fmtp(MSFilter *f, void *arg){
	DecState *s=(DecState*)f->data;
	const char *fmtp=(const char *)arg;
	char buf[32];
	if (fmtp_get_value(fmtp, "plc", buf, sizeof(buf))){
		s->plc=atoi(buf);
	}
	return 0;
}

static void dec_process(MSFilter *f){
	DecState *s=(DecState*)f->data;
	mblk_t *im;
	mblk_t *om;
	int err=-2;
	SpeexBits bits;
	int bytes=s->frsz*2;
	bool_t bits_initd=FALSE;

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		int rem_bits=(int)((im->b_wptr-im->b_rptr)*8);

		if (!bits_initd) {
			speex_bits_init(&bits);
			bits_initd=TRUE;
		}else speex_bits_reset(&bits);

		speex_bits_read_from(&bits,(char*)im->b_rptr,(int)(im->b_wptr-im->b_rptr));

		/* support for multiple frame  in one RTP packet */
 		do{
			om=allocb(bytes,0);
			mblk_meta_copy(im, om);
			err=speex_decode_int(s->state,&bits,(int16_t*)om->b_wptr);
			om->b_wptr+=bytes;

			if (err==0){
				ms_queue_put(f->outputs[0],om);
				if (s->sample_time==0) s->sample_time=f->ticker->time;
				s->sample_time+=20;
				if (s->plc_count>0){
					// ms_warning("Did speex packet loss concealment during %i ms",s->plc_count*20);
					s->plc_count=0;
				}

			}else {
				if (err==-1)
					ms_warning("speex end of stream");
				else if (err==-2)
					ms_warning("speex corrupted stream");
				freemsg(om);
			}
		}while((rem_bits= speex_bits_remaining(&bits))>10);
		freemsg(im);
	}
	if (s->plc && s->sample_time!=0 && f->ticker->time>=s->sample_time){
		/* we should output a frame but no packet were decoded
		 thus do packet loss concealment*/
		om=allocb(bytes,0);
		err=speex_decode_int(s->state,NULL,(int16_t*)om->b_wptr);
		om->b_wptr+=bytes;
		mblk_set_plc_flag(om, 1);
		ms_queue_put(f->outputs[0],om);

		s->sample_time+=20;
		s->plc_count++;
		if (s->plc_count>=plc_max){
			s->sample_time=0;
		}
	}
	if (bits_initd) speex_bits_destroy(&bits);
}

static int dec_have_plc(MSFilter *f, void *arg)
{
	*((int *)arg) = 1;
	return 0;
}

static MSFilterMethod dec_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	,	dec_set_sr	},
	{	MS_FILTER_GET_SAMPLE_RATE	,	dec_get_sr	},
	{	MS_FILTER_ADD_FMTP		,	dec_add_fmtp	},
	{ 	MS_DECODER_HAVE_PLC		, 	dec_have_plc	},
	{	MS_FILTER_GET_NCHANNELS		,	get_channels},
	{	0				,	NULL		}
};

#ifdef _MSC_VER

MSFilterDesc ms_speex_dec_desc={
	MS_SPEEX_DEC_ID,
	"MSSpeexDec",
	N_("The free and wonderful speex codec"),
	MS_FILTER_DECODER,
	"speex",
	1,
	1,
	dec_init,
	dec_preprocess,
	dec_process,
	dec_postprocess,
	dec_uninit,
	dec_methods,
	MS_FILTER_IS_PUMP
};

#else

MSFilterDesc ms_speex_dec_desc={
	.id=MS_SPEEX_DEC_ID,
	.name="MSSpeexDec",
	.text=N_("The free and wonderful speex codec"),
	.category=MS_FILTER_DECODER,
	.enc_fmt="speex",
	.ninputs=1,
	.noutputs=1,
	.init=dec_init,
	.preprocess=dec_preprocess,
	.postprocess=dec_postprocess,
	.process=dec_process,
	.uninit=dec_uninit,
	.methods=dec_methods,
	.flags=MS_FILTER_IS_PUMP
};

#endif

MS_FILTER_DESC_EXPORT(ms_speex_dec_desc)
MS_FILTER_DESC_EXPORT(ms_speex_enc_desc)

