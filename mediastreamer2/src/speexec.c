/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL 
Author: Simon Morlat <simon.morlat@linphone.org>

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

#include "mediastreamer2/msfilter.h"
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#ifdef WIN32
#include <malloc.h> /* for alloca */
#endif

//#define EC_DUMP 1

#define EC_DUMP_PREFIX "/sdcard"

static const float smooth_factor=0.05;
static const int framesize=128;
static const int ref_max_delay=60;


typedef struct SpeexECState{
	SpeexEchoState *ecstate;
	SpeexPreprocessState *den;
	MSBufferizer delayed_ref;
	MSBufferizer echo;
	int ref_bytes_limit;
	int framesize;
	int filterlength;
	int samplerate;
	int delay_ms;
	int tail_length_ms;
	float ref_bufsize_ms;
#ifdef EC_DUMP
	FILE *echofile;
	FILE *reffile;
#endif
	bool_t using_silence;
	bool_t echostarted;
	bool_t bypass_mode;
}SpeexECState;

static void speex_ec_init(MSFilter *f){
	SpeexECState *s=(SpeexECState *)ms_new(SpeexECState,1);

	s->samplerate=8000;
	ms_bufferizer_init(&s->delayed_ref);
	ms_bufferizer_init(&s->echo);
	s->delay_ms=0;
	s->tail_length_ms=250;
	s->ecstate=NULL;
	s->framesize=framesize;
	s->den = NULL;
	s->using_silence=FALSE;
	s->echostarted=FALSE;
	s->bypass_mode=FALSE;

#ifdef EC_DUMP
	{
		char *fname=ms_strdup_printf("%s/msspeexec-%p-echo.raw", EC_DUMP_PREFIX,f);
		s->echofile=fopen(fname,"w");
		ms_free(fname);
		fname=ms_strdup_printf("%s/msspeexec-%p-ref.raw", EC_DUMP_PREFIX,f);
		s->reffile=fopen(fname,"w");
		ms_free(fname);
	}
#endif
	
	f->data=s;
}

static void speex_ec_uninit(MSFilter *f){
	SpeexECState *s=(SpeexECState*)f->data;
	ms_bufferizer_uninit(&s->delayed_ref);
#ifdef EC_DUMP
	if (s->echofile)
		fclose(s->echofile);
	if (s->reffile)
		fclose(s->reffile);
#endif
	ms_free(s);
}


static void speex_ec_preprocess(MSFilter *f){
	SpeexECState *s=(SpeexECState*)f->data;
	int delay_samples=0;
	mblk_t *m;

	s->echostarted=FALSE;
	s->filterlength=(s->tail_length_ms*s->samplerate)/1000;
	delay_samples=s->delay_ms*s->samplerate/1000;
	ms_message("Initializing speex echo canceler with framesize=%i, filterlength=%i, delay_samples=%i",
		s->framesize,s->filterlength,delay_samples);
	s->ref_bytes_limit=(2*ref_max_delay*s->samplerate)/1000;
	s->ecstate=speex_echo_state_init(s->framesize,s->filterlength);
	s->den = speex_preprocess_state_init(s->framesize, s->samplerate);
	speex_echo_ctl(s->ecstate, SPEEX_ECHO_SET_SAMPLING_RATE, &s->samplerate);
	speex_preprocess_ctl(s->den, SPEEX_PREPROCESS_SET_ECHO_STATE, s->ecstate);
	/* fill with zeroes for the time of the delay*/
	m=allocb(delay_samples*2,0);
	m->b_wptr+=delay_samples*2;
	ms_bufferizer_put (&s->delayed_ref,m);
	s->ref_bufsize_ms=s->delay_ms;
}

/*	inputs[0]= reference signal (sent to soundcard)
 *	inputs[1]= near speech & echo signal	(read from soundcard)
 *	outputs[0]=  far end speech
 *	outputs[1]=  near end speech, echo removed - towards far end
*/
static void speex_ec_process(MSFilter *f){
	SpeexECState *s=(SpeexECState*)f->data;
	int nbytes=s->framesize*2;
	mblk_t *refm;
	uint8_t *ref,*echo;
	int size;
	float ref_bufsize_ms;
	int diff;
	int idiff;
	int drift;
	int threshold;
	
	if (s->bypass_mode) {
		while((refm=ms_queue_get(f->inputs[0]))!=NULL){
			ms_queue_put(f->outputs[0],refm);
		}
		while((refm=ms_queue_get(f->inputs[1]))!=NULL){
			ms_queue_put(f->outputs[1],refm);
		}
		return;
	}
	
	if (f->inputs[0]!=NULL){
		while((refm=ms_queue_get(f->inputs[0]))!=NULL){
			mblk_t *cp=dupmsg(refm);
			ms_bufferizer_put(&s->delayed_ref,cp);
			ms_queue_put(f->outputs[0],refm);
		}
	}

	ms_bufferizer_put_from_queue(&s->echo,f->inputs[1]);
	
	ref=(uint8_t*)alloca(nbytes);
	echo=(uint8_t*)alloca(nbytes);
	while (ms_bufferizer_read(&s->echo,echo,nbytes)>=nbytes){
		mblk_t *oecho=allocb(nbytes,0);
		
		if (ms_bufferizer_read(&s->delayed_ref,ref,nbytes)==0){
			/* if we don't have enough ref samples, use silence instead */
			ms_warning("Not enough ref samples, using zeroes");
			memset(ref,0,nbytes);
		}
		
#ifdef EC_DUMP
		if (s->reffile)
			fwrite(ref,nbytes,1,s->reffile);
		if (s->echofile)
			fwrite(echo,nbytes,1,s->echofile);
#endif
		speex_echo_cancellation(s->ecstate,(short*)echo,(short*)ref,(short*)oecho->b_wptr);
		speex_preprocess_run(s->den, (short*)oecho->b_wptr);
		oecho->b_wptr+=nbytes;
		ms_queue_put(f->outputs[1],oecho);
	}

	/* control the size of the delayed_ref bufferizer, we should always have in the long term a size
	 that corresponds to the echo delay*/
	size=ms_bufferizer_get_avail(&s->delayed_ref);
	ref_bufsize_ms=((size/2)*1000.0)/(float)s->samplerate;
	s->ref_bufsize_ms=smooth_factor*ref_bufsize_ms + (1-smooth_factor)*s->ref_bufsize_ms;
	//ms_message("Averaged reference bufsize is %f, instant value is %f",s->ref_bufsize_ms,ref_bufsize_ms);
	diff=((int)s->ref_bufsize_ms) - s->delay_ms;
	idiff=((int)ref_bufsize_ms) - s->delay_ms;
	threshold=s->tail_length_ms/4;
	drift=diff;
	if (diff > threshold && ref_bufsize_ms>idiff) {
		nbytes=2*(int)((drift*s->samplerate)/1000.0);
		ms_warning("Averaged reference bufsize is %f while expected delay is %i, diff=%i, need to drop %i bytes",s->ref_bufsize_ms,s->delay_ms,diff,nbytes);
		ms_bufferizer_skip_bytes(&s->delayed_ref,nbytes);
		s->ref_bufsize_ms=s->delay_ms;
	}else if ((diff < (-threshold)) && (idiff < (-threshold))){
		nbytes=2*(int)((-drift*s->samplerate)/1000.0);		
		ms_warning("Averaged reference bufsize is %f while expected delay is %i, diff=%i, need to add %i bytes",s->ref_bufsize_ms,s->delay_ms,diff,nbytes);
		refm=allocb(nbytes,0);
		memset(refm->b_wptr,0,nbytes);
		refm->b_wptr+=nbytes;
		ms_bufferizer_put(&s->delayed_ref,refm);
		s->ref_bufsize_ms=s->delay_ms;
	}
}

static void speex_ec_postprocess(MSFilter *f){
	SpeexECState *s=(SpeexECState*)f->data;
	ms_bufferizer_flush (&s->delayed_ref);
	ms_bufferizer_flush (&s->echo);
	if (s->ecstate!=NULL){
		speex_echo_state_destroy(s->ecstate);
		s->ecstate=NULL;
	}
	if (s->den!=NULL){
		speex_preprocess_state_destroy(s->den);
		s->den=NULL;
	}
}

static int speex_ec_set_sr(MSFilter *f, void *arg){
	SpeexECState *s=(SpeexECState*)f->data;
	s->samplerate = *(int*)arg;
	return 0;
}

static int speex_ec_set_framesize(MSFilter *f, void *arg){
	SpeexECState *s=(SpeexECState*)f->data;
	s->framesize = *(int*)arg;
	return 0;
}

static int speex_ec_set_delay(MSFilter *f, void *arg){
	SpeexECState *s=(SpeexECState*)f->data;
	s->delay_ms = *(int*)arg;
	return 0;
}

static int speex_ec_set_tail_length(MSFilter *f, void *arg){
	SpeexECState *s=(SpeexECState*)f->data;
	s->tail_length_ms=*(int*)arg;
	return 0;
}
static int speex_ec_set_bypass_mode(MSFilter *f, void *arg) {
	SpeexECState *s=(SpeexECState*)f->data;
	s->bypass_mode=*(bool_t*)arg;
	ms_message("set EC bypass mode to [%i]",s->bypass_mode);
	return 0;
}
static int speex_ec_get_bypass_mode(MSFilter *f, void *arg) {
	SpeexECState *s=(SpeexECState*)f->data;
	*(bool_t*)arg=s->bypass_mode;
	return 0;
}

static MSFilterMethod speex_ec_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE, speex_ec_set_sr },
	{	MS_ECHO_CANCELLER_SET_TAIL_LENGTH	,	speex_ec_set_tail_length	},
	{	MS_ECHO_CANCELLER_SET_DELAY		,	speex_ec_set_delay		},
	{	MS_ECHO_CANCELLER_SET_FRAMESIZE	,	speex_ec_set_framesize		},
	{	MS_ECHO_CANCELLER_SET_BYPASS_MODE	,	speex_ec_set_bypass_mode		},
	{	MS_ECHO_CANCELLER_GET_BYPASS_MODE	,	speex_ec_get_bypass_mode		},
};

#ifdef _MSC_VER

MSFilterDesc ms_speex_ec_desc={
	MS_SPEEX_EC_ID,
	"MSSpeexEC",
	N_("Echo canceller using speex library"),
	MS_FILTER_OTHER,
	NULL,
	2,
	2,
	speex_ec_init,
	speex_ec_preprocess,
	speex_ec_process,
	speex_ec_postprocess,
	speex_ec_uninit,
	speex_ec_methods
};

#else

MSFilterDesc ms_speex_ec_desc={
	.id=MS_SPEEX_EC_ID,
	.name="MSSpeexEC",
	.text=N_("Echo canceller using speex library"),
	.category=MS_FILTER_OTHER,
	.ninputs=2,
	.noutputs=2,
	.init=speex_ec_init,
	.preprocess=speex_ec_preprocess,
	.process=speex_ec_process,
	.postprocess=speex_ec_postprocess,
	.uninit=speex_ec_uninit,
	.methods=speex_ec_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_speex_ec_desc)
