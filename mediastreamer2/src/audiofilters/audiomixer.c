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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "mediastreamer2/msaudiomixer.h"
#include "mediastreamer2/msticker.h"

#ifdef _MSC_VER
#include <malloc.h>
#define alloca _alloca
#endif

#define MIXER_MAX_CHANNELS 20
#define ALWAYS_STREAMOUT 1
#define BYPASS_MODE_TIMEOUT 1000

static void accumulate(int32_t *sum, int16_t* contrib, int nwords){
	int i;
	for(i=0;i<nwords;++i){
		sum[i]+=contrib[i];
	}
}

static MS2_INLINE int16_t saturate(int32_t s){
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
	int min_fullness;
	uint64_t last_flow_control;
	uint64_t last_activity;
	bool_t active;
	bool_t output_enabled;
} Channel;

static void channel_init(Channel *chan){
	ms_bufferizer_init(&chan->bufferizer);
	chan->input=NULL;
	chan->gain=1.0;
	chan->active=TRUE;
	chan->output_enabled=TRUE;
}

static void channel_prepare(Channel *chan, int bytes_per_tick){
	chan->input=ms_malloc0(bytes_per_tick);
	chan->last_flow_control=(uint64_t)-1;
	chan->last_activity=(uint64_t)-1;
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

static int channel_flow_control(Channel *chan, int threshold, uint64_t time){
	int size;
	int skip=0;
	if (chan->last_flow_control==(uint64_t)-1){
		chan->last_flow_control=time;
		chan->min_fullness=-1;
		return skip;
	}
	size=(int)ms_bufferizer_get_avail(&chan->bufferizer);
	if (chan->min_fullness==-1 || chan->min_fullness<size) chan->min_fullness=size;
	if (time-chan->last_flow_control>=5000){
		if (chan->min_fullness>=threshold){
			skip=chan->min_fullness-(threshold/2);
			ms_bufferizer_skip_bytes(&chan->bufferizer,skip);
		}
		chan->last_flow_control=time;
		chan->min_fullness=-1;
	}
	return skip;
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
	int bytespertick;
	Channel channels[MIXER_MAX_CHANNELS];
	int32_t *sum;
	int conf_mode;
	int skip_threshold;
	int master_channel;
	bool_t bypass_mode;
	bool_t single_output;
} MixerState;


static void mixer_init(MSFilter *f){
	MixerState *s=ms_new0(MixerState,1);
	int i;
	s->conf_mode=FALSE; /*this is the default, don't change it*/
	s->nchannels=1;
	s->rate=44100;
	s->master_channel=-1;
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

static bool_t has_single_output(MSFilter *f, MixerState *s){
	int i;
	int count=0;
	for (i=0;i<f->desc->noutputs;++i){
		Channel *chan=&s->channels[i];
		if (f->outputs[i] && chan->output_enabled) count++;
	}
	return count==1;
}

static void mixer_preprocess(MSFilter *f){
	MixerState *s=(MixerState *)f->data;
	int i;

	s->bytespertick=(2*s->nchannels*s->rate*f->ticker->interval)/1000;
	s->sum=(int32_t*)ms_malloc0((s->bytespertick/2)*sizeof(int32_t));
	for(i=0;i<MIXER_MAX_CHANNELS;++i)
		channel_prepare(&s->channels[i],s->bytespertick);
	/*ms_message("bytespertick=%i, purgeoffset=%i",s->bytespertick,s->purgeoffset);*/
	s->skip_threshold=s->bytespertick*2;
	s->bypass_mode=FALSE;
	s->single_output=has_single_output(f,s);
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

static void mixer_dispatch_output(MSFilter *f, MixerState*s, MSQueue *inq, int active_input){
	int i;
	for (i=0;i<f->desc->noutputs;i++){
		MSQueue *outq=f->outputs[i];
		Channel *chan=&s->channels[i];
		if (outq && chan->output_enabled && (active_input!=i || s->conf_mode==0)){
			mblk_t *m;
			if (s->single_output){
				while((m=ms_queue_get(inq))!=NULL){
					ms_queue_put(outq,m);
				}
				break;
			}else{
				for(m=ms_queue_peek_first(inq);!ms_queue_end(inq,m);m=ms_queue_next(inq,m)){
					ms_queue_put(outq,dupmsg(m));
				}
			}
		}
	}
	ms_queue_flush(inq);
}

/* the bypass mode is an optimization for the case of a single contributing channel. In such case there is no need to synchronize with other channels
 * and to make a sum. The processing is greatly simplified by just distributing the packets from the single contributing channels to the output channels.*/
static bool_t mixer_check_bypass(MSFilter *f, MixerState *s){
    int i;
    int active_cnt=0;
    int active_input=-1;
	MSQueue *activeq=NULL;
	uint64_t curtime=f->ticker->time;
	for (i=0;i<f->desc->ninputs;i++){
		MSQueue *q=f->inputs[i];
		if (q){
			Channel *chan=&s->channels[i];
			if (!ms_queue_empty(q)){
				chan->last_activity=curtime;
				activeq=q;
				active_cnt++;
				active_input=i;
			}else{
				if (chan->last_activity==(uint64_t)-1){
					chan->last_activity=curtime;
				}else if (curtime-chan->last_activity<BYPASS_MODE_TIMEOUT){
					activeq=q;
					active_cnt++;
					active_input=i;
				}
			}
		}
	}
	if (active_cnt==1){
		if (!s->bypass_mode){
			s->bypass_mode=TRUE;
			ms_message("MSAudioMixer [%p] is entering bypass mode.",f);
		}
		mixer_dispatch_output(f,s,activeq,active_input);
		return TRUE;
	}else if (active_cnt>1){
		if (s->bypass_mode){
			s->bypass_mode=FALSE;
			ms_message("MSAudioMixer [%p] is leaving bypass mode.",f);
		}
		return FALSE;
	}
	/*last case: no contributing channels at all. There is then nothing to do.*/
	return TRUE;
}

static void mixer_process(MSFilter *f){
	MixerState *s=(MixerState *)f->data;
	int i;
	int nwords=s->bytespertick/2;
	int skip=0;
	bool_t got_something=FALSE;

	ms_filter_lock(f);
	if (mixer_check_bypass(f,s)){
		ms_filter_unlock(f);
		return;
	}
	
	memset(s->sum,0,nwords*sizeof(int32_t));

	/* read from all inputs and sum everybody */
	for(i=0;i<f->desc->ninputs;++i){
		MSQueue *q=f->inputs[i];

		if (q){
			if (channel_process_in(&s->channels[i],q,s->sum,nwords))
				got_something=TRUE;
			if ((skip=channel_flow_control(&s->channels[i],s->skip_threshold,f->ticker->time))>0){
				ms_warning("Too much data in channel %i, %i ms in excess dropped",i,(skip*1000)/(2*s->nchannels*s->rate));
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
				Channel *chan=&s->channels[i];
				if (q && chan->output_enabled){
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
				Channel *chan=&s->channels[i];
				if (q && chan->output_enabled){
					ms_queue_put(q,channel_process_out(&s->channels[i],s->sum,nwords));
				}
			}
		}
	}
	ms_filter_unlock(f);
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

static int mixer_enable_output(MSFilter *f, void *data){
	MixerState *s=(MixerState *)f->data;
	MSAudioMixerCtl *ctl=(MSAudioMixerCtl*)data;
	if (ctl->pin<0 || ctl->pin>=MIXER_MAX_CHANNELS){
		ms_warning("mixer_enable_output: invalid pin number %i",ctl->pin);
		return -1;
	}
	ms_filter_lock(f);
	s->channels[ctl->pin].output_enabled=ctl->param.enabled;
	s->single_output=has_single_output(f,s);
	ms_filter_unlock(f);
	return 0;
}


static int mixer_set_conference_mode(MSFilter *f, void *data){
	MixerState *s=(MixerState *)f->data;
	s->conf_mode=*(int*)data;
	return 0;
}


/*not implemented yet. A master channel is a channel that is used as a reference to mix other inputs. Samples from the master channel should never be dropped*/
static int mixer_set_master_channel(MSFilter *f, void *data){
	MixerState *s=(MixerState *)f->data;
	s->master_channel=*(int*)data;
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
	{	MS_AUDIO_MIXER_SET_MASTER_CHANNEL , mixer_set_master_channel },
	{	MS_AUDIO_MIXER_ENABLE_OUTPUT,	mixer_enable_output },
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
