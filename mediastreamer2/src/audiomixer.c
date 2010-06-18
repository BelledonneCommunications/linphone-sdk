/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Simon MORLAT (simon.morlat@linphone.org)

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

#include "mediastreamer2/msaudiomixer.h"
#include "mediastreamer2/msticker.h"

#ifdef _MSC_VER
#include <malloc.h>
#define alloca _alloca
#endif

#define MIXER_MAX_CHANNELS 20
#define MAX_LATENCY 0.08
#define ALWAYS_STREAMOUT 1

typedef struct MixerState{
	int nchannels;
	int rate;
	int purgeoffset;
	int bytespertick;
	MSBufferizer channels[MIXER_MAX_CHANNELS];
	float gains[MIXER_MAX_CHANNELS];
	int32_t *sum;
} MixerState;

static void mixer_init(MSFilter *f){
	MixerState *s=ms_new0(MixerState,1);
	int i;
	
	s->nchannels=1;
	s->rate=44100;
	for(i=0;i<MIXER_MAX_CHANNELS;++i){
		ms_bufferizer_init(&s->channels[i]);
		s->gains[i]=1;
	}
	f->data=s;
}

static void mixer_uninit(MSFilter *f){
	int i;
	MixerState *s=(MixerState *)f->data;
	for(i=0;i<MIXER_MAX_CHANNELS;++i){
		ms_bufferizer_uninit(&s->channels[i]);
	}
	ms_free(s);
}

static void mixer_preprocess(MSFilter *f){
	MixerState *s=(MixerState *)f->data;
	s->purgeoffset=(int)(MAX_LATENCY*(float)(2*s->nchannels*s->rate));
	s->bytespertick=(2*s->nchannels*s->rate*f->ticker->interval)/1000;
	s->sum=(int32_t*)ms_malloc0((s->bytespertick/2)*sizeof(int32_t));
	/*ms_message("bytespertick=%i, purgeoffset=%i",s->bytespertick,s->purgeoffset);*/
}

static void mixer_postprocess(MSFilter *f){
	MixerState *s=(MixerState *)f->data;
	ms_free(s->sum);
	s->sum=NULL;
}

static void accumulate(int32_t *sum, int16_t* contrib, int nwords){
	int i;
	for(i=0;i<nwords;++i){
		sum[i]+=contrib[i];
	}
}

static void accumulate_mpy(int32_t *sum, int16_t* contrib, int nwords, float gain){
	int i;
	for(i=0;i<nwords;++i){
		sum[i]+=(int32_t)(gain*(float)contrib[i]);
	}
}

static inline int16_t saturate(int32_t s){
	if (s>32767) return 32767;
	if (s<-32767) return -32767;
	return (int16_t)s;
}

static mblk_t *make_output(int32_t *sum, int nwords){
	mblk_t *om=allocb(nwords*2,0);
	int i;
	for(i=0;i<nwords;++i,om->b_wptr+=2){
		*(int16_t*)om->b_wptr=saturate(sum[i]);
	}
	return om;
}

static void mixer_process(MSFilter *f){
	MixerState *s=(MixerState *)f->data;
	int i;
	int nwords=s->bytespertick/2;
	uint8_t *tmpbuf=(uint8_t *)alloca(s->bytespertick);
	bool_t got_something=FALSE;

	memset(s->sum,0,nwords*sizeof(int32_t));

	for(i=0;i<MIXER_MAX_CHANNELS;++i){
		MSQueue *q=f->inputs[i];
		if (q){
			ms_bufferizer_put_from_queue(&s->channels[i],q);
			if (ms_bufferizer_get_avail(&s->channels[i])>=s->bytespertick){
				ms_bufferizer_read(&s->channels[i],tmpbuf,s->bytespertick);
				if (s->gains[i]==1)
					accumulate(s->sum,(int16_t*)tmpbuf,nwords);
				else
					accumulate_mpy(s->sum,(int16_t*)tmpbuf,nwords,s->gains[i]);
				got_something=TRUE;
			}
			if (ms_bufferizer_get_avail(&s->channels[i])>s->purgeoffset){
				ms_warning("Too much data in channel %i",i);
				ms_bufferizer_flush (&s->channels[i]);
			}
		}
	}
#ifdef ALWAYS_STREAMOUT
	got_something=TRUE;
#endif
	
	if (got_something){
		ms_queue_put(f->outputs[0],make_output(s->sum,nwords));
	}
}

static int mixer_set_rate(MSFilter *f, void *data){
	MixerState *s=(MixerState *)f->data;
	s->rate=*(int*)data;
	return 0;
}

static int mixer_get_rate(MSFilter *f, void *data){
	MixerState *s=(MixerState *)f->data;
	*(int*)data=s->rate;
	return 0;
}

static int mixer_set_nchannels(MSFilter *f, void *data){
	MixerState *s=(MixerState *)f->data;
	s->nchannels=*(int*)data;
	return 0;
}

static int mixer_get_nchannels(MSFilter *f, void *data){
	MixerState *s=(MixerState *)f->data;
	*(int*)data=s->nchannels;
	return 0;
}

static int mixer_set_input_gain(MSFilter *f, void *data){
	MixerState *s=(MixerState *)f->data;
	MSAudioMixerCtl *ctl=(MSAudioMixerCtl*)data;
	if (ctl->pin<0 || ctl->pin>=MIXER_MAX_CHANNELS){
		ms_warning("mixer_set_input_gain: invalid pin number %i",ctl->pin);
		return -1;
	}
	s->gains[ctl->pin]=ctl->gain;
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_NCHANNELS , mixer_set_nchannels },
	{	MS_FILTER_GET_NCHANNELS , mixer_get_nchannels },
	{	MS_FILTER_SET_SAMPLE_RATE, mixer_set_rate },
	{	MS_FILTER_GET_SAMPLE_RATE, mixer_get_rate },
	{	MS_AUDIO_MIXER_SET_INPUT_GAIN , mixer_set_input_gain },
	{0,NULL}
};

#ifdef _MSC_VER

MSFilterDesc ms_audio_mixer_desc={
	MS_AUDIO_MIXER_ID,
	"MSAudioMixer",
	N_("A filter that mixes down 16 bit sample audio streams"),
	MS_FILTER_OTHER,
	NULL,
	MIXER_MAX_CHANNELS,
	1,
	mixer_init,
	mixer_preprocess,
	mixer_process,
	mixer_postprocess,
	mixer_uninit,
	methods,
	MS_FILTER_IS_PUMP
};

#else

MSFilterDesc ms_audio_mixer_desc={
	.id=MS_AUDIO_MIXER_ID,
	.name="MSAudioMixer",
	.text=N_("A filter that mixes down 16 bit sample audio streams"),
	.category=MS_FILTER_OTHER,
	.ninputs=MIXER_MAX_CHANNELS,
	.noutputs=1,
	.init=mixer_init,
	.preprocess=mixer_preprocess,
	.process=mixer_process,
	.postprocess=mixer_postprocess,
	.uninit=mixer_uninit,
	.methods=methods,
	.flags=MS_FILTER_IS_PUMP
};

#endif

MS_FILTER_DESC_EXPORT(ms_audio_mixer_desc)
