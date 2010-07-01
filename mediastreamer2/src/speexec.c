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

static const int framesize=128;
static const int ref_max_delay=60;


typedef struct SpeexECState{
	SpeexEchoState *ecstate;
	SpeexPreprocessState *den;
	MSBufferizer ref;
	MSBufferizer delayed_ref;
	MSBufferizer echo;
	int ref_bytes_limit;
	int framesize;
	int filterlength;
	int samplerate;
	int delay_ms;
	int tail_length_ms;
#ifdef EC_DUMP
	FILE *echofile;
	FILE *reffile;
#endif
	bool_t using_silence;
	bool_t echostarted;
}SpeexECState;

static void speex_ec_init(MSFilter *f){
	SpeexECState *s=(SpeexECState *)ms_new(SpeexECState,1);

	s->samplerate=8000;
	ms_bufferizer_init(&s->ref);
	ms_bufferizer_init(&s->delayed_ref);
	ms_bufferizer_init(&s->echo);
	s->delay_ms=0;
	s->tail_length_ms=250;
	s->ecstate=NULL;
	s->framesize=framesize;
	s->den = NULL;
	s->using_silence=FALSE;
	s->echostarted=FALSE;

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
	ms_bufferizer_uninit(&s->ref);
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
}

/*	inputs[0]= reference signal (sent to soundcard)
 *	inputs[1]= near speech & echo signal	(read from soundcard)
 *	outputs[1]=  near end speech, echo removed - towards far end
*/
static void speex_ec_process(MSFilter *f){
	SpeexECState *s=(SpeexECState*)f->data;
	int nbytes=s->framesize*2;
	mblk_t *refm;
	uint8_t *ref,*echo;
	int size;
	
	if (f->inputs[1]!=NULL){
		int maxsize;
		ms_bufferizer_put_from_queue (&s->echo,f->inputs[1]);
		maxsize=ms_bufferizer_get_avail(&s->echo);
		if (s->echostarted==FALSE && maxsize>0){
			ms_message("speex_ec: starting receiving echo signal");
			s->echostarted=TRUE;
		}
		if (maxsize>=s->ref_bytes_limit){
			ms_message("ref_bytes_limit adjusted from %i to %i",s->ref_bytes_limit,maxsize);
			s->ref_bytes_limit=maxsize+nbytes;
		}
	}
	
	if (f->inputs[0]!=NULL){
		while((refm=ms_queue_get(f->inputs[0]))!=NULL){
			mblk_t *cp=dupmsg(refm);
			ms_bufferizer_put(&s->ref,refm);
			ms_bufferizer_put(&s->delayed_ref,cp);
		}
	}
	
/*
	ms_message("echo bytes=%i, ref bytes=%i",ms_bufferizer_get_avail(&s->echo),
	           ms_bufferizer_get_avail(&s->ref));
*/
	ref=(uint8_t*)alloca(nbytes);
	echo=(uint8_t*)alloca(nbytes);
	while (ms_bufferizer_read(&s->echo,echo,nbytes)>=nbytes){
		mblk_t *oref=allocb(nbytes,0);
		mblk_t *oecho=allocb(nbytes,0);
		if (ms_bufferizer_read(&s->ref,oref->b_wptr,nbytes)==0){
			memset(ref,0,nbytes);
			memset(oref->b_wptr,0,nbytes);
			/*missing data, use silence instead*/
			if (!s->using_silence){
				ms_warning("No ref samples, using silence instead");
				s->using_silence=TRUE;
			}
		}else{
			ms_bufferizer_read(&s->delayed_ref,ref,nbytes);
			if (s->using_silence){
				ms_warning("Reference stream is back.");
				s->using_silence=FALSE;
			}
		}
		oref->b_wptr+=nbytes;
		ms_queue_put(f->outputs[0],oref);
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
	if (!s->echostarted){
		/*if we have not yet receive anything from the soundcard, bypass the reference signal*/
		while (ms_bufferizer_get_avail(&s->ref)>=nbytes){
			mblk_t *oref=allocb(nbytes,0);
			ms_bufferizer_read(&s->ref,oref->b_wptr,nbytes);
			oref->b_wptr+=nbytes;
			ms_bufferizer_skip_bytes(&s->delayed_ref,nbytes);
			ms_queue_put(f->outputs[0],oref);
		}
	}

	/* do not accumulate too much reference signal */
	if ((size=ms_bufferizer_get_avail(&s->ref))> (s->ref_bytes_limit+nbytes)) {
		/* remove nbytes bytes */
		ms_warning("purging %i bytes from ref signal, size=%i, limit=%i",nbytes,size,s->ref_bytes_limit);
		ms_bufferizer_skip_bytes(&s->ref,nbytes);
		ms_bufferizer_skip_bytes(&s->delayed_ref,nbytes);
	}
}

static void speex_ec_postprocess(MSFilter *f){
	SpeexECState *s=(SpeexECState*)f->data;
	ms_bufferizer_flush (&s->ref);
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


static MSFilterMethod speex_ec_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE, speex_ec_set_sr },
	{	MS_ECHO_CANCELLER_SET_TAIL_LENGTH	,	speex_ec_set_tail_length	},
	{	MS_ECHO_CANCELLER_SET_DELAY		,	speex_ec_set_delay		},
	{	MS_ECHO_CANCELLER_SET_FRAMESIZE	,	speex_ec_set_framesize		},
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
