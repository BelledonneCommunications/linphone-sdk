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

static void accumulate(int32_t *sum, int16_t* contrib, int nwords){
	int i;
	for(i=0;i<nwords;++i){
		sum[i]+=contrib[i];
	}
}

static inline int16_t saturate(int32_t s){
	if (s>32767) return 32767;
	if (s<-32767) return -32767;
	return (int16_t)s;
}

static void apply_gain(int16_t *samples, int nsamples, float gain){
	int i;
	for(i=0;i<nsamples;++i){
		samples[i]=saturate((int)(gain*(float)samples[i]));
	}
}

typedef struct Channel{
	MSBufferizer bufferizer;
	int16_t *input;	/*the channel contribution, for removal at output*/
	float gain;
	int active;
} Channel;

static void channel_init(Channel *chan){
	ms_bufferizer_init(&chan->bufferizer);
	chan->input=NULL;
	chan->gain=1.0;
	chan->active=1;
}

static void channel_prepare(Channel *chan, int bytes_per_tick){
	chan->input=ms_malloc0(bytes_per_tick);
}

static int channel_process_in(Channel *chan, MSQueue *q, int32_t *sum, int nsamples){
	ms_bufferizer_put_from_queue(&chan->bufferizer,q);
	if (ms_bufferizer_read(&chan->bufferizer,(uint8_t*)chan->input,nsamples*2)!=0){
		if (chan->active){
			if (chan->gain!=1.0){
				apply_gain(chan->input,nsamples,chan->gain);
			}
			accumulate(sum,chan->input,nsamples);
		}
		return nsamples;
	}else memset(chan->input,0,nsamples*2);
	return 0;
}

static mblk_t *channel_process_out(Channel *chan, int32_t *sum, int nsamples){
	int i;
	mblk_t *om=allocb(nsamples*2,0);
	int16_t *out=(int16_t*)om->b_wptr;

	if (chan->active){
		/*remove own contribution from sum*/
		for(i=0;i<nsamples;++i){
			out[i]=saturate(sum[i]-(int32_t)chan->input[i]);
		}
	}else{
		for(i=0;i<nsamples;++i){
			out[i]=saturate(sum[i]);
		}
	}
	om->b_wptr+=nsamples*2;
	return om;
}

static void channel_unprepare(Channel *chan){
	ms_free(chan->input);
	chan->input=NULL;
}

static void channel_uninit(Channel *chan){
	ms_bufferizer_uninit(&chan->bufferizer);
}

typedef struct MixerState{
	int nchannels;
	int rate;
	int purgeoffset;
	int bytespertick;
	Channel channels[MIXER_MAX_CHANNELS];
	int32_t *sum;
	int conf_mode;
} MixerState;




static void mixer_init(MSFilter *f){
	MixerState *s=ms_new0(MixerState,1);
	int i;
	
	s->nchannels=1;
	s->rate=44100;
	for(i=0;i<MIXER_MAX_CHANNELS;++i){
		channel_init(&s->channels[i]);
	}
	f->data=s;
}

static void mixer_uninit(MSFilter *f){
	int i;
	MixerState *s=(MixerState *)f->data;
	for(i=0;i<MIXER_MAX_CHANNELS;++i){
		channel_uninit(&s->channels[i]);
	}
	ms_free(s);
}

static void mixer_preprocess(MSFilter *f){
	MixerState *s=(MixerState *)f->data;
	int i;
	s->purgeoffset=(int)(MAX_LATENCY*(float)(2*s->nchannels*s->rate));
	s->bytespertick=(2*s->nchannels*s->rate*f->ticker->interval)/1000;
	s->sum=(int32_t*)ms_malloc0((s->bytespertick/2)*sizeof(int32_t));
	for(i=0;i<MIXER_MAX_CHANNELS;++i)
		channel_prepare(&s->channels[i],s->bytespertick);
	/*ms_message("bytespertick=%i, purgeoffset=%i",s->bytespertick,s->purgeoffset);*/
}

static void mixer_postprocess(MSFilter *f){
	MixerState *s=(MixerState *)f->data;
	int i;
	
	ms_free(s->sum);
	s->sum=NULL;
	for(i=0;i<MIXER_MAX_CHANNELS;++i)
		channel_unprepare(&s->channels[i]);
	
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
	bool_t got_something=FALSE;

	memset(s->sum,0,nwords*sizeof(int32_t));

	/* read from all inputs and sum everybody */
	for(i=0;i<MIXER_MAX_CHANNELS;++i){
		MSQueue *q=f->inputs[i];
		if (q){
			if (channel_process_in(&s->channels[i],q,s->sum,nwords))
				got_something=TRUE;
			/*FIXME: incorporate the following into the channel and use a better flow control algorithm*/
			if (ms_bufferizer_get_avail(&s->channels[i].bufferizer)>s->purgeoffset){
				ms_warning("Too much data in channel %i",i);
				ms_bufferizer_flush(&s->channels[i].bufferizer);
			}
		}
	}
#ifdef ALWAYS_STREAMOUT
	got_something=TRUE;
#endif
	/* compute outputs. In conference mode each one has a different output, because its channel own contribution has to be removed*/
	if (got_something){
		if (s->conf_mode==0){
			mblk_t *om=NULL;
			for(i=0;i<MIXER_MAX_CHANNELS;++i){
				MSQueue *q=f->outputs[i];
				if (q){
					if (om==NULL){
						om=make_output(s->sum,nwords);
					}else{
						om=dupb(om);
					}
					ms_queue_put(q,om);
				}
			}
		}else{
			for(i=0;i<MIXER_MAX_CHANNELS;++i){
				MSQueue *q=f->outputs[i];
				if (q){
					ms_queue_put(q,channel_process_out(&s->channels[i],s->sum,nwords));
				}
			}
		}
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
	s->channels[ctl->pin].gain=ctl->param.gain;
	return 0;
}

static int mixer_set_active(MSFilter *f, void *data){
	MixerState *s=(MixerState *)f->data;
	MSAudioMixerCtl *ctl=(MSAudioMixerCtl*)data;
	if (ctl->pin<0 || ctl->pin>=MIXER_MAX_CHANNELS){
		ms_warning("mixer_set_active_gain: invalid pin number %i",ctl->pin);
		return -1;
	}
	s->channels[ctl->pin].active=ctl->param.active;
	return 0;
}

static int mixer_set_conference_mode(MSFilter *f, void *data){
	MixerState *s=(MixerState *)f->data;
	s->conf_mode=*(int*)data;
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_NCHANNELS , mixer_set_nchannels },
	{	MS_FILTER_GET_NCHANNELS , mixer_get_nchannels },
	{	MS_FILTER_SET_SAMPLE_RATE, mixer_set_rate },
	{	MS_FILTER_GET_SAMPLE_RATE, mixer_get_rate },
	{	MS_AUDIO_MIXER_SET_INPUT_GAIN , mixer_set_input_gain },
	{	MS_AUDIO_MIXER_SET_ACTIVE , mixer_set_active },
	{	MS_AUDIO_MIXER_ENABLE_CONFERENCE_MODE, mixer_set_conference_mode	},
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
	MIXER_MAX_CHANNELS,
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
	.noutputs=MIXER_MAX_CHANNELS,
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
