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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msvolume.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msutils.h"
#include "ortp/utils.h"
#include <math.h>

#ifdef HAVE_SPEEXDSP
#include <speex/speex_preprocess.h>
#endif

static const float max_e = (32768* 0.7f);   /* 0.7 - is RMS factor */
static const float coef = 0.2f; /* floating averaging coeff. for energy */
//static const float gain_k = 0.02; /* floating averaging coeff. for gain */
static const float vol_upramp = 0.4f;
static const float vol_downramp = 0.4f;   /* not yet runtime parameterizable */
static const float en_weight=4.0;
static const float noise_thres=0.1f;
static const float transmit_thres=4;
static const float min_ng_floorgain=0.005f;
static const float agc_threshold=0.5;


typedef struct Volume{
	float energy;
	float level_pk;
	float instant_energy;
	float lt_speaker_en;
	float gain; 		/**< the one really applied, smoothed target_gain version*/
	float static_gain;	/**< the one fixed by the user */
	int dc_offset;
	//float gain_k;
	float vol_upramp;
	float vol_fast_upramp;
	float vol_downramp;
	float ea_thres;
	float ea_transmit_thres;
	float force;
	float target_gain; /*the target gain choosed by echo limiter and noise gate*/
	int sustain_time; /* time in ms for which echo limiter remains active after resuming from speech to silence.*/
	int sustain_dur;
	MSFilter *peer;
#ifdef HAVE_SPEEXDSP
	SpeexPreprocessState *speex_pp;
#endif
	int sample_rate;
	int nsamples;
	int ng_cut_time; /*noise gate cut time, after last speech detected*/
	int ng_noise_dur;
	float ng_threshold;
	float ng_floorgain;
	float ng_gain;
	MSBufferizer *buffer;
	OrtpExtremum min;
	OrtpExtremum max;
	bool_t agc_enabled;
	bool_t noise_gate_enabled;
	bool_t remove_dc;
	bool_t fast_upramp;
}Volume;

static void volume_init(MSFilter *f){
	Volume *v=(Volume*)ms_new0(Volume,1);
	v->energy=0;
	v->level_pk = 0;
	v->static_gain = v->gain = v->target_gain = 1;
	v->dc_offset = 0;
	v->vol_upramp = vol_upramp;
	v->vol_fast_upramp=vol_upramp*3;
	v->vol_downramp = vol_downramp;
	v->ea_thres = noise_thres;
	v->ea_transmit_thres=transmit_thres;
	v->force=en_weight;
	v->peer=NULL;
	v->sustain_time=200;
	v->sustain_dur = 0;
	v->agc_enabled=FALSE;
	v->buffer=ms_bufferizer_new();
	v->sample_rate=8000;
	v->nsamples=80;
	v->noise_gate_enabled=FALSE;
	v->ng_cut_time = 400;/*TODO: ng_sustain (milliseconds)*/
	v->ng_noise_dur=0;
	v->ng_threshold=noise_thres;
	v->ng_floorgain=min_ng_floorgain;
	v->ng_gain = 1;
	v->remove_dc=FALSE;
#ifdef HAVE_SPEEXDSP
	v->speex_pp=NULL;
#endif
	ortp_extremum_init(&v->max,1000);
	ortp_extremum_init(&v->min,30000);
	f->data=v;
}

static void volume_uninit(MSFilter *f){
	Volume *v=(Volume*)f->data;
#ifdef HAVE_SPEEXDSP
	if (v->speex_pp)
		speex_preprocess_state_destroy(v->speex_pp);
#endif
	ms_bufferizer_destroy(v->buffer);
	ms_free(f->data);
}

static MS2_INLINE float linear_to_db(float linear){
	if (linear==0) return MS_VOLUME_DB_LOWEST;
	return 10*ortp_log10f(linear);
}

static int volume_get(MSFilter *f, void *arg){
	float *farg=(float*)arg;
	Volume *v=(Volume*)f->data;
	*farg=linear_to_db(v->energy);
	return 0;
}

static int volume_get_min(MSFilter *f, void *arg){
	float *farg=(float*)arg;
	Volume *v=(Volume*)f->data;
	*farg=linear_to_db(ortp_extremum_get_current(&v->min));
	return 0;
}

static int volume_get_max(MSFilter *f, void *arg){
	float *farg=(float*)arg;
	Volume *v=(Volume*)f->data;
	*farg=linear_to_db(ortp_extremum_get_current(&v->max));
	return 0;
}

static int volume_set_sample_rate(MSFilter *f, void *arg){
	Volume *v=(Volume*)f->data;
	v->sample_rate=*(int*)arg;
	return 0;
}

static int volume_get_linear(MSFilter *f, void *arg){
	float *farg=(float*)arg;
	Volume *v=(Volume*)f->data;
	*farg = v->energy;
	return 0;
}
// use our builtin agc
#if 0
static float volume_agc_process(Volume *v, mblk_t *om){
	speex_preprocess_run(v->speex_pp,(int16_t*)om->b_rptr);
	return 1;
}
#else

static float volume_agc_process(Volume *v, mblk_t *om) {
	static int counter;
	// target is: 1
	float gain_reduct = (agc_threshold + v->level_pk) / 1;
	/* actual gain ramp timing the same as with echo limiter process */
	if (!(++counter % 20)) {
		ms_debug("_level=%f, gain reduction=%f, gain=%f, ng_gain=%f %f %f",
				v->level_pk, gain_reduct, v->gain, v->ng_gain, v->ng_threshold, v->static_gain);
	}
	return gain_reduct;
}

#endif

static MS2_INLINE float compute_gain(Volume *v, float energy, float weight) {
	float ret = v->static_gain / (1 + (energy * weight));
	return ret;
}

/*
 The principle of this algorithm is that we apply a gain to the input signal which is opposite to
 the energy measured by the peer MSVolume.
 For example if some noise is played by the speaker, then the signal captured by the microphone will
 be lowered.  The gain changes smoothly when the peer energy is decreasing, but is immediately
 changed when the peer energy is increasing.
*/

static float volume_echo_avoider_process(Volume *v, mblk_t *om) {
	static int counter;
	float peer_e,peer_pk;
	int nsamples = (int)((om->b_wptr - om->b_rptr) / 2);
	float mic_spk_ratio;
	peer_e = ((Volume *)(v->peer->data))->energy;
	peer_pk=((Volume *)(v->peer->data))->energy;

	if (peer_pk>v->lt_speaker_en)
		v->lt_speaker_en=peer_pk;
	else v->lt_speaker_en=(0.005f*peer_pk)+(0.995f*v->lt_speaker_en);
	mic_spk_ratio=(v->energy/(v->lt_speaker_en+v->ea_thres));

	/* where v->target_gain is not set, it is kept steady - not to modify elsewhere! */
	if (peer_e > v->ea_thres) {
		if (mic_spk_ratio>v->ea_transmit_thres){
			ms_debug("Local mic is capturing louder than speaker output mic_spk_ratio=%f",mic_spk_ratio);
			v->target_gain=v->static_gain;
			v->fast_upramp=TRUE;
		}else{
			/*lower our gain when peer above threshold*/
			v->target_gain = compute_gain(v, peer_e, v->force);
			v->sustain_dur = v->sustain_time;
		}
	}else {
		if (v->sustain_dur > 0) {
			/*restore normal gain when INITIAL (soft start) call OR timeout */
			v->sustain_dur -= (nsamples * 1000) / v->sample_rate;
		}
		else{
			v->target_gain = v->static_gain;
			v->fast_upramp=TRUE;
		}
	}
	if (!(++counter % 20)) {
		ms_debug("volume_echo_avoider_process(): mic_en=%f, peer_e=%f, target_g=%f, gain=%f, spk_peak=%f",
		             v->energy, peer_e, v->target_gain, v->gain, v->lt_speaker_en);
	}
	return v->target_gain;
}

static void volume_noise_gate_process(Volume *v , float energy, mblk_t *om){
	static int counter;
	float tgain = v->ng_floorgain;  /* start with floorgain */
	int nsamples=(int)((om->b_wptr-om->b_rptr)/2);
	if (energy > v->ng_threshold) {
		v->ng_noise_dur = v->ng_cut_time;
		tgain = 1.0;
	}
	else {
		if (v->ng_noise_dur > 0) {
			v->ng_noise_dur -= (nsamples * 1000) / v->sample_rate;
			tgain = 1.0;
		}
	}
	/* simple averaging computation is adequate here: fast rise, relatively slower decrease */
	/* of gain - ears impression */
	v->ng_gain = v->ng_gain*0.75f + tgain*0.25f;
	if (!(++counter % 10)) {
		ms_debug("%d: nglevel=%f, energy=%f, tgain=%f, ng_gain=%f",
				          (v->peer!=NULL)?1:0, energy, v->energy, tgain, v->ng_gain);
	}
}

static int volume_set_db_gain(MSFilter *f, void *gain){
	float *fgain=(float*)gain;
	Volume *v=(Volume*)f->data;
	v->gain = v->static_gain = (float)pow(10,(*fgain)/10);
	ms_message("MSVolume set gain to [%f db], [%f] linear",*fgain,v->gain);
	return 0;
}

static int volume_set_gain(MSFilter *f, void *arg){
	float *farg=(float*)arg;
	Volume *v=(Volume*)f->data;
	v->gain = v->target_gain = v->static_gain = *farg;
	return 0;
}

static int volume_get_gain(MSFilter *f, void *arg){
	float *farg=(float*)arg;
	Volume *v=(Volume*)f->data;
	*farg = v->static_gain;
	return 0;
}

static int volume_get_gain_db(MSFilter *f, void *arg){
	float *farg=(float*)arg;
	Volume *v=(Volume*)f->data;
	*farg = linear_to_db (v->static_gain);
	return 0;
}


static int volume_set_peer(MSFilter *f, void *arg){
	MSFilter *p=(MSFilter*)arg;
	Volume *v=(Volume*)f->data;
	v->peer=p;
	return 0;
}

static int volume_set_agc(MSFilter *f, void *arg){
	Volume *v=(Volume*)f->data;
	v->agc_enabled=*(int*)arg;
	return 0;
}

static int volume_set_ea_threshold(MSFilter *f, void*arg){
	Volume *v=(Volume*)f->data;
	float val=*(float*)arg;
	if (val<0 || val>1) {
		ms_error("Error: threshold must be in range [0..1]");
		return -1;
	}
	v->ea_thres = val;
	return 0;
}

static int volume_set_ea_transmit_threshold(MSFilter *f, void*arg){
	Volume *v=(Volume*)f->data;
	float val=*(float*)arg;
	v->ea_transmit_thres=val;
	return 0;
}

// currently defined for vol_upramp (downramp always fast!)
static int volume_set_ea_speed(MSFilter *f, void*arg){
	Volume *v=(Volume*)f->data;
	float val=*(float*)arg;
	if (val < 0 || val > .5) {
		ms_error("Error: speed must be in range [0..0.5]");
		return -1;
	}
	v->vol_upramp = val;
	return 0;
}

static int volume_set_ea_force(MSFilter *f, void*arg){
	Volume *v=(Volume*)f->data;
	float val=*(float*)arg;
	v->force=val;
	return 0;
}

static int volume_set_ea_sustain(MSFilter *f, void *arg){
	Volume *v=(Volume*)f->data;
	v->sustain_time=*(int*)arg;
	return 0;
}

static int volume_enable_noise_gate(MSFilter *f, void *arg){
	Volume *v=(Volume*)f->data;
	v->noise_gate_enabled=*(int*)arg;
	if (v->noise_gate_enabled){
		v->gain = v->target_gain = v->ng_floorgain; // start with floorgain (soft start)
	}
	return 0;
}

static int volume_set_noise_gate_threshold(MSFilter *f, void *arg){
	Volume *v=(Volume*)f->data;
	v->ng_threshold= *(float*)arg;
	return 0;
}

static int volume_set_noise_gate_floorgain(MSFilter *f, void *arg){
	Volume *v=(Volume*)f->data;
	v->ng_floorgain=*(float*)arg;
	/* don't allow setting too low values, otherwise, the ramp cannot produce */
	if (v->ng_floorgain<min_ng_floorgain){
		v->ng_floorgain=min_ng_floorgain;
	}
	if (v->noise_gate_enabled){
		v->gain = v->target_gain = v->ng_floorgain; // start with floorgain (soft start)
	}
	return 0;
}

static int volume_remove_dc(MSFilter *f, void *arg){
	Volume *v=(Volume*)f->data;
	v->remove_dc=*(int*)arg;
	return 0;
}

static MS2_INLINE int16_t saturate(int val) {
	return (val>32767) ? 32767 : ( (val<-32767) ? -32767 : val);
}

// note: number of samples should not vary much
// with filtered peak detection, variable buffer size from volume_process call is not optimal
static void update_energy(Volume *v, int16_t *signal, int numsamples, uint64_t curtime) {
	int i;
	float acc = 0;
	float en;
	int lp = 0, pk = 0;

	for (i=0;i<numsamples;++i){
		int s=signal[i];
		acc += s * s;

		lp = abs(s);
		if (lp > pk)
			pk = lp;
	}
	en = (float)((sqrt(acc / numsamples)+1) / max_e);
	v->energy = (en * coef) + v->energy * (1.0f - coef);
	v->level_pk = (float)pk / max_e;
	v->instant_energy = en;// currently non-averaged energy seems better (short artefacts)
	ortp_extremum_record_max(&v->max,curtime,v->energy);
	ortp_extremum_record_min(&v->min,curtime,v->energy);
}

static void apply_gain(Volume *v, mblk_t *m, float tgain) {
	int16_t *sample;
	int dc_offset = 0;
	int32_t intgain;
	float gain;

	/* ramps with factors means linear ramps in logarithmic domain */

	if (v->gain < tgain) {
		if (v->gain<v->ng_floorgain)
			v->gain=v->ng_floorgain;
		v->gain *= 1 + (v->fast_upramp ? v->vol_fast_upramp : v->vol_upramp);
		if (v->gain > tgain)
			v->gain = tgain;
	}else if (v->gain > tgain) {
		v->gain *= 1 - v->vol_downramp;
		if (v->gain < tgain)
			v->gain = tgain;
		v->fast_upramp=FALSE;
	}

	gain=v->gain * v->ng_gain;
	intgain = (int32_t)(gain* 4096);


	//if (v->peer) ms_message("MSVolume:%p Applying gain %5f, v->gain=%5f, tgain=%5f, ng_gain=%5f",v,gain,v->gain,tgain,v->ng_gain);

	if (v->remove_dc){
		for (	sample=(int16_t*)m->b_rptr;
					sample<(int16_t*)m->b_wptr;
					++sample){
			dc_offset+= *sample;
			*sample = saturate(((*sample - v->dc_offset) * intgain) / 4096);
		}
		/* offset smoothing */
		v->dc_offset = (v->dc_offset*7 + dc_offset*2/(int)(m->b_wptr - m->b_rptr)) / 8;
	}else if (gain!=1){
		for (	sample=(int16_t*)m->b_rptr;
					sample<(int16_t*)m->b_wptr;
					++sample){
			*sample = saturate(((*sample) * intgain) / 4096);
		}
	}
}

static void volume_preprocess(MSFilter *f){
	Volume *v=(Volume*)f->data;
	/*process agc by chunks of 10 ms*/
	v->nsamples=(int)(0.01*(float)v->sample_rate);
	if (v->agc_enabled){
		ms_message("AGC is enabled.");
#if defined HAVE_SPEEXDSP && !defined MS_FIXED_POINT
		if (v->speex_pp==NULL){
			int tmp=1;
			v->speex_pp=speex_preprocess_state_init(v->nsamples,v->sample_rate);
			if (speex_preprocess_ctl(v->speex_pp,SPEEX_PREPROCESS_SET_AGC,&tmp)==-1){
				ms_warning("Speex AGC is not available.");
			}
			tmp=0;
			speex_preprocess_ctl(v->speex_pp,SPEEX_PREPROCESS_SET_VAD,&tmp);
			speex_preprocess_ctl(v->speex_pp,SPEEX_PREPROCESS_SET_DENOISE,&tmp);
			speex_preprocess_ctl(v->speex_pp,SPEEX_PREPROCESS_SET_DEREVERB,&tmp);
		}
#endif
	}
	ortp_extremum_reset(&v->min);
	ortp_extremum_reset(&v->max);
}

static void volume_process(MSFilter *f){
	mblk_t *m;
	Volume *v=(Volume*)f->data;
	float target_gain;

	/* Important notice: any processes called herein can modify v->target_gain, at
	 * end of this function apply_gain() is called, thus: later process calls can
	 * override this target gain, and order must be well thought out
	 */
	if (v->agc_enabled || v->peer!=NULL){
		mblk_t *om;
		size_t nbytes=(size_t)(v->nsamples*2);
		ms_bufferizer_put_from_queue(v->buffer,f->inputs[0]);
		while(ms_bufferizer_get_avail(v->buffer)>=nbytes){
			om=allocb(nbytes,0);
			ms_bufferizer_read(v->buffer,om->b_wptr,nbytes);
			om->b_wptr+=nbytes;
			update_energy(v,(int16_t*)om->b_rptr, v->nsamples, f->ticker->time);
			target_gain = v->static_gain;

			if (v->peer)  /* this ptr set = echo limiter enable flag */
				target_gain = volume_echo_avoider_process(v, om);

			/* Multiply with gain from echo limiter, not "choose smallest". Why?
			 * Remote talks, local echo suppress via mic path, but still audible in
			 * remote speaker. AGC operates fully, too (local speaker close to local mic!);
			 * having agc gain reduction also contribute to total reduction makes sense.
			 */
			if (v->agc_enabled) target_gain/= volume_agc_process(v, om);
			if (v->noise_gate_enabled)
				volume_noise_gate_process(v, v->instant_energy, om);
			apply_gain(v, om, target_gain);
			ms_queue_put(f->outputs[0],om);
		}
	}else{
		/*light processing: no agc. Work in place in the input buffer*/
		while((m=ms_queue_get(f->inputs[0]))!=NULL){
			update_energy(v,(int16_t*)m->b_rptr, (int)((m->b_wptr - m->b_rptr) / 2), f->ticker->time);
			target_gain = v->static_gain;

			if (v->noise_gate_enabled)
				volume_noise_gate_process(v, v->instant_energy, m);
			apply_gain(v, m, target_gain);
			ms_queue_put(f->outputs[0],m);
		}
	}
}

static MSFilterMethod methods[]={
	{	MS_VOLUME_GET		,	volume_get		},
	{	MS_VOLUME_GET_LINEAR	, 	volume_get_linear	},
	{ MS_VOLUME_SET_GAIN, volume_set_gain},
	{	MS_VOLUME_SET_PEER	,	volume_set_peer		},
	{	MS_VOLUME_SET_EA_THRESHOLD , 	volume_set_ea_threshold	},
	{	MS_VOLUME_SET_EA_SPEED	,	volume_set_ea_speed	},
	{	MS_VOLUME_SET_EA_FORCE	, 	volume_set_ea_force	},
	{	MS_VOLUME_SET_EA_SUSTAIN,	volume_set_ea_sustain	},
	{	MS_VOLUME_SET_EA_TRANSMIT_THRESHOLD,	volume_set_ea_transmit_threshold	},
	{	MS_FILTER_SET_SAMPLE_RATE,	volume_set_sample_rate	},
	{	MS_VOLUME_ENABLE_AGC	,	volume_set_agc		},
	{	MS_VOLUME_ENABLE_NOISE_GATE,	volume_enable_noise_gate},
	{	MS_VOLUME_SET_NOISE_GATE_THRESHOLD,	volume_set_noise_gate_threshold},
	{	MS_VOLUME_SET_NOISE_GATE_FLOORGAIN,	volume_set_noise_gate_floorgain},
	{	MS_VOLUME_SET_DB_GAIN	,	volume_set_db_gain		},
	{	MS_VOLUME_GET_GAIN	,	volume_get_gain		},
	{	MS_VOLUME_GET_GAIN_DB	,	volume_get_gain_db		},
	{	MS_VOLUME_REMOVE_DC, volume_remove_dc },
	{	MS_VOLUME_GET_MIN	,	volume_get_min	},
	{	MS_VOLUME_GET_MAX	,	volume_get_max	},
	{	0			,	NULL			}
};

#ifndef _MSC_VER
MSFilterDesc ms_volume_desc={
	.name="MSVolume",
	.text=N_("A filter that controls and measure sound volume"),
	.id=MS_VOLUME_ID,
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=1,
	.init=volume_init,
	.uninit=volume_uninit,
	.preprocess=volume_preprocess,
	.process=volume_process,
	.methods=methods
};
#else
MSFilterDesc ms_volume_desc={
	MS_VOLUME_ID,
	"MSVolume",
	N_("A filter that controls and measure sound volume"),
	MS_FILTER_OTHER,
	NULL,
	1,
	1,
	volume_init,
	volume_preprocess,
	volume_process,
	NULL,
	volume_uninit,
	methods
};
#endif

MS_FILTER_DESC_EXPORT(ms_volume_desc)
