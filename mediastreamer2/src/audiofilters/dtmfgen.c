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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msticker.h"


#include <math.h>

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif


#define NO_SAMPLES_THRESHOLD 100 /*ms*/

#if defined(ANDROID) || defined(__ios) /* because android and iOS don't deal well with audio stop and restarts at each dtmf.*/
#define TRAILLING_SILENCE 10000 /*ms*/
#else
#define TRAILLING_SILENCE 500 /*ms*/
#endif


struct DtmfGenState{
	int rate;
	int nchannels;
	int dur;
	int pos;
	float highfreq;
	float lowfreq;
	int nosamples_time;
	int silence;
	int amplitude;
	float default_amplitude;
	int repeat_count;
	MSDtmfGenCustomTone current_tone;
	bool_t playing;
};

typedef struct DtmfGenState DtmfGenState;

static void dtmfgen_init(MSFilter *f){
	DtmfGenState *s=(DtmfGenState *)ms_new0(DtmfGenState,1);
	s->rate=8000;
	s->nchannels = 1;
	s->dur=s->rate/10;
	s->pos=0;
	s->nosamples_time=0;
	s->silence=0;
	s->default_amplitude=0.2;
	s->amplitude=(s->default_amplitude*0.7*32767);
	s->repeat_count=0;
	f->data=s;
}

static void dtmfgen_uninit(MSFilter *f){
	ms_free(f->data);
}

static int dtmfgen_put(MSFilter *f, void *arg){
	DtmfGenState *s=(DtmfGenState*)f->data;
	const char *dtmf=(char*)arg;
	
	switch(dtmf[0]){
		case '0':
			s->lowfreq=941;
			s->highfreq=1336;
			break;
		case '1':
			s->lowfreq=697;
			s->highfreq=1209;
			break;
		case '2':
			s->lowfreq=697;
			s->highfreq=1336;
			break;
		case '3':
			s->lowfreq=697;
			s->highfreq=1477;
			break;
		case '4':
			s->lowfreq=770;
			s->highfreq=1209;
			break;
		case '5':
			s->lowfreq=770;
			s->highfreq=1336;
			break;
		case '6':
			s->lowfreq=770;
			s->highfreq=1477;
			break;
		case '7':
			s->lowfreq=852;
			s->highfreq=1209;
			break;
		case '8':
			s->lowfreq=852;
			s->highfreq=1336;
			break;
		case '9':
			s->lowfreq=852;
			s->highfreq=1477;
			break;
		case '*':
			s->lowfreq=941;
			s->highfreq=1209;
			break;
		case '#':
			s->lowfreq=941;
			s->highfreq=1477;
			break;
		case 'A':
			s->lowfreq=697;
			s->highfreq=1633;
			break;
		case 'B':
			s->lowfreq=770;
			s->highfreq=1633;
			break;
		case 'C':
			s->lowfreq=852;
			s->highfreq=1633;
			break;
		case 'D':
			s->lowfreq=941;
			s->highfreq=1633;
			break;
		case '!':
			ms_message("flash dtmf");
			return 0;
		case ' ':
			/*ignore*/
			return 0;
			break;
		default:
			ms_warning("Not a dtmf key.");
			return -1;
	}
	ms_filter_lock(f);
	s->pos=0;
	s->lowfreq=s->lowfreq/s->rate;
	s->highfreq=s->highfreq/s->rate;
	s->dur=s->rate/10; /*100 ms duration */
	s->silence=0;
	s->amplitude=s->default_amplitude*32767*0.7;
	s->current_tone.tone_name[0]=dtmf[0];
	s->current_tone.tone_name[1]=0;
	s->current_tone.interval=0;
	s->playing=TRUE;
	ms_filter_unlock(f);
	return 0;
}

static int dtmfgen_play_tone(MSFilter *f, void *arg){
	DtmfGenState *s=(DtmfGenState*)f->data;
	MSDtmfGenCustomTone *def=(MSDtmfGenCustomTone*)arg;
	
	ms_message("Playing tones of frequencies %i,%i Hz, duration=%i, amplitude=%f interval=%i, repeat_count=%i",def->frequencies[0],
			   def->frequencies[1],def->duration,def->amplitude, def->interval, def->repeat_count);
	ms_filter_lock(f);
	s->current_tone=*def;
	s->pos=0;
	s->dur=(s->rate*def->duration)/1000;
	s->lowfreq=((float)def->frequencies[0])/(float)s->rate;
	s->highfreq=((float)def->frequencies[1])/(float)s->rate;;
	s->silence=0;
	s->amplitude=((float)def->amplitude)* 0.7*32767.0;
	s->repeat_count=0;
	s->playing=TRUE;
	ms_filter_unlock(f);
	
	return 0;
}

static int dtmfgen_start(MSFilter *f, void *arg){
	if (dtmfgen_put(f,arg)==0){
		DtmfGenState *s=(DtmfGenState*)f->data;
		s->dur=5*s->rate;
		return 0;
	}
	return -1;
}

static int dtmfgen_stop(MSFilter *f, void *arg){
	DtmfGenState *s=(DtmfGenState*)f->data;
	int min_duration=(100*s->rate)/1000; /*wait at least 100 ms*/
	ms_filter_lock(f);
	if (s->pos<min_duration)
		s->dur=min_duration;
	else s->dur=0;
	memset(&s->current_tone,0,sizeof(s->current_tone));
	ms_filter_unlock(f);
	return 0;
}

static int dtmfgen_set_rate(MSFilter *f, void *arg){
	DtmfGenState *s=(DtmfGenState*)f->data;
	s->rate=*((int*)arg);
	return 0;
}

static int dtmfgen_get_rate(MSFilter *f, void *arg) {
	DtmfGenState *s = (DtmfGenState *)f->data;
	*((int *)arg) = s->rate;
	return 0;
}

static int dtmfgen_set_nchannels(MSFilter *f, void *arg) {
	DtmfGenState *s = (DtmfGenState *)f->data;
	s->nchannels = *(int *)arg;
	return 0;
}

static int dtmfgen_get_nchannels(MSFilter *f, void *arg) {
	DtmfGenState *s = (DtmfGenState *)f->data;
	*((int *)arg) = s->nchannels;
	return 0;
}

static int dtmfgen_set_amp(MSFilter *f, void *arg){
	DtmfGenState *s=(DtmfGenState*)f->data;
	s->default_amplitude=*(float*)arg;
	return 0;
}


static void write_dtmf(DtmfGenState *s , int16_t *sample, int nsamples){
	int i, j;
	int16_t dtmf_sample;
	for (i=0;i<nsamples && s->pos<s->dur;i++,s->pos++){
		dtmf_sample = (int16_t)(((float)s->amplitude)*sin(2*M_PI*(float)s->pos*s->lowfreq));
		if (s->highfreq!=0) dtmf_sample += (int16_t)(((float)s->amplitude)*sin(2*M_PI*(float)s->pos*s->highfreq));
		for (j = 0; j < s->nchannels; j++) {
			sample[(i * s->nchannels) + j] = dtmf_sample;
		}
	}
	for (;i<nsamples;++i){
		for (j = 0; j < s->nchannels; j++) {
			sample[(i * s->nchannels) + j] = 0;
		}
	}
	if (s->pos>=s->dur){
		s->pos=0;
		if (s->current_tone.interval > 0) {
			s->silence=s->current_tone.interval;
			s->repeat_count++;
			if (s->current_tone.repeat_count>0 && s->repeat_count>=s->current_tone.repeat_count){
				s->playing=FALSE;
			}
		} else {
			s->playing=FALSE;
			s->silence=TRAILLING_SILENCE;
		}
	}
}

static void dtmfgen_process(MSFilter *f){
	mblk_t *m;
	DtmfGenState *s=(DtmfGenState*)f->data;
	int nsamples;

	ms_filter_lock(f);
	if (ms_queue_empty(f->inputs[0])){
		s->nosamples_time+=f->ticker->interval;
		if ((s->playing || s->silence!=0) && s->nosamples_time>NO_SAMPLES_THRESHOLD){
			/*after 100 ms without stream we decide to generate our own sample
			 instead of writing into incoming stream samples*/
			nsamples=(f->ticker->interval*s->rate)/1000;
			m=allocb(nsamples*s->nchannels*2,0);
			if (s->silence==0){
				if (s->pos==0){
					MSDtmfGenEvent ev;
					ev.tone_start_time=f->ticker->time;
					strncpy(ev.tone_name,s->current_tone.tone_name,sizeof(ev.tone_name));
					ms_filter_notify(f,MS_DTMF_GEN_EVENT,&ev);
				}
				write_dtmf(s,(int16_t*)m->b_wptr,nsamples);
			}else{
				memset(m->b_wptr,0,nsamples*s->nchannels*2);
				s->silence-=f->ticker->interval;
				if (s->silence<0) s->silence=0;
			}
			m->b_wptr+=nsamples*s->nchannels*2;
			ms_queue_put(f->outputs[0],m);
		}
	}else{
		s->nosamples_time=0;
		if (s->current_tone.interval > 0) {
			s->silence-=f->ticker->interval;
			if (s->silence<0) s->silence=0;
		} else s->silence=0;
		while((m=ms_queue_get(f->inputs[0]))!=NULL){
			if (s->playing && s->silence==0){
				if (s->pos==0){
					MSDtmfGenEvent ev;
					ev.tone_start_time=f->ticker->time;
					strncpy(ev.tone_name,s->current_tone.tone_name,sizeof(ev.tone_name));
					ms_filter_notify(f,MS_DTMF_GEN_EVENT,&ev);
				}
				nsamples=(m->b_wptr-m->b_rptr)/(2*s->nchannels);
				write_dtmf(s, (int16_t*)m->b_rptr,nsamples);
			}
			ms_queue_put(f->outputs[0],m);
		}
	}
	ms_filter_unlock(f);
}

MSFilterMethod dtmfgen_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	,	dtmfgen_set_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	,	dtmfgen_get_rate	},
	{	MS_FILTER_SET_NCHANNELS		,	dtmfgen_set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS		,	dtmfgen_get_nchannels	},
	{	MS_DTMF_GEN_PLAY		,	dtmfgen_put		},
	{	MS_DTMF_GEN_START		,   dtmfgen_start },
	{	MS_DTMF_GEN_STOP		, 	dtmfgen_stop },
	{	MS_DTMF_GEN_PLAY_CUSTOM, dtmfgen_play_tone },
	{	MS_DTMF_GEN_SET_DEFAULT_AMPLITUDE, dtmfgen_set_amp },
	{	0				,	NULL			}
};

#ifdef _MSC_VER

MSFilterDesc ms_dtmf_gen_desc={
	MS_DTMF_GEN_ID,
	"MSDtmfGen",
	N_("DTMF generator"),
	MS_FILTER_OTHER,
	NULL,
	1,
	1,
	dtmfgen_init,
	NULL,
	dtmfgen_process,
	NULL,
	dtmfgen_uninit,
	dtmfgen_methods,
	MS_FILTER_IS_PUMP
};

#else

MSFilterDesc ms_dtmf_gen_desc={
	.id=MS_DTMF_GEN_ID,
	.name="MSDtmfGen",
	.text=N_("DTMF generator"),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=1,
	.init=dtmfgen_init,
	.process=dtmfgen_process,
	.uninit=dtmfgen_uninit,
	.methods=dtmfgen_methods,
	.flags=MS_FILTER_IS_PUMP
};

#endif

MS_FILTER_DESC_EXPORT(ms_dtmf_gen_desc)

