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

#ifdef _MSC_VER
#include <malloc.h>
#endif

#include <speex/speex_resampler.h>
#include <speex/speex.h>

#ifdef ANDROID
#include "cpu-features.h"
#endif

typedef struct _ResampleData{
	MSBufferizer *bz;
	uint32_t ts;
	uint32_t input_rate;
	uint32_t output_rate;
	int in_nchannels;
	int out_nchannels;
	SpeexResamplerState *handle;
	int cpuFeatures; /*store because there is no SPEEX_LIB_GET_CPU_FEATURES*/
} ResampleData;

static ResampleData * resample_data_new(void){
	ResampleData *obj=ms_new0(ResampleData,1);
	obj->bz=ms_bufferizer_new();
	obj->ts=0;
	obj->input_rate=8000;
	obj->output_rate=16000;
	obj->handle=NULL;
	obj->in_nchannels=obj->out_nchannels=1;
	obj->cpuFeatures=0;
	return obj;
}

static void resample_data_destroy(ResampleData *obj){
	if (obj->handle!=NULL)
		speex_resampler_destroy(obj->handle);
	ms_bufferizer_destroy(obj->bz);
	ms_free(obj);
}

static void resample_init(MSFilter *obj){
	ResampleData* data=resample_data_new();
#ifdef SPEEX_LIB_SET_CPU_FEATURES
	#ifdef ANDROID
	if (((android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM) && ((android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0))
		|| (android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM64)) {
		data->cpuFeatures = SPEEX_LIB_CPU_FEATURE_NEON;
	}
	#elif MS_HAS_ARM_NEON
	data->cpuFeatures = SPEEX_LIB_CPU_FEATURE_NEON;
	#endif
	ms_message("speex_lib_ctl init with neon ? %d", (data->cpuFeatures == SPEEX_LIB_CPU_FEATURE_NEON));
	speex_lib_ctl(SPEEX_LIB_SET_CPU_FEATURES, &data->cpuFeatures);
#else
	ms_message("speex_lib_ctl does not support SPEEX_LIB_CPU_FEATURE_NEON");
#endif
	obj->data=data;
}

static void resample_uninit(MSFilter *obj){
	resample_data_destroy((ResampleData*)obj->data);
}

static int resample_channel_adapt(int in_nchannels, int out_nchannels, mblk_t *im, mblk_t **om) {
	if ((in_nchannels == 2) && (out_nchannels == 1)) {
		size_t msgsize = msgdsize(im) / 2;
		*om = allocb(msgsize, 0);
		for (; im->b_rptr < im->b_wptr; im->b_rptr += 4, (*om)->b_wptr += 2) {
			*(int16_t *)(*om)->b_wptr = *(int16_t *)im->b_rptr;
		}
		mblk_meta_copy(im, *om);
		return 1;
	} else if ((in_nchannels == 1) && (out_nchannels == 2)) {
		size_t msgsize = msgdsize(im) * 2;
		*om = allocb(msgsize, 0);
		for (; im->b_rptr < im->b_wptr; im->b_rptr += 2, (*om)->b_wptr += 4) {
			((int16_t *)(*om)->b_wptr)[0] = *(int16_t *)im->b_rptr;
			((int16_t *)(*om)->b_wptr)[1] = *(int16_t *)im->b_rptr;
		}
		mblk_meta_copy(im, *om);
		return 1;
	}
	return 0;
}

static void resample_init_speex(ResampleData *dt){
	int err=0;
	int quality=SPEEX_RESAMPLER_QUALITY_VOIP; /*default value is voip*/
#if MS_HAS_ARM /*on ARM, NEON optimization are mandatory to support this quality, else using basic mode*/
#if SPEEX_LIB_SET_CPU_FEATURES
	if (dt->cpuFeatures != SPEEX_LIB_CPU_FEATURE_NEON)
		quality=SPEEX_RESAMPLER_QUALITY_MIN;
#elif !MS_HAS_ARM_NEON
	quality=SPEEX_RESAMPLER_QUALITY_MIN;
#endif /*SPEEX_LIB_SET_CPU_FEATURES*/
#endif /*MS_HAS_ARM*/
	ms_message("Initializing speex resampler in mode [%s] ",(quality==SPEEX_RESAMPLER_QUALITY_VOIP?"voip":"min"));
	dt->handle=speex_resampler_init(dt->in_nchannels, dt->input_rate, dt->output_rate, quality, &err);
}

static void resample_preprocess(MSFilter *obj){
	ResampleData *dt=(ResampleData*)obj->data;
	if(dt->handle == NULL) resample_init_speex(dt);
}

static void resample_process_ms2(MSFilter *obj){
	ResampleData *dt=(ResampleData*)obj->data;
	mblk_t *im, *om = NULL, *om_chan = NULL;

	if (dt->output_rate==dt->input_rate){
		while((im=ms_queue_get(obj->inputs[0]))!=NULL){
			if (resample_channel_adapt(dt->in_nchannels, dt->out_nchannels, im, &om) == 0) {
				ms_queue_put(obj->outputs[0], im);
			} else {
				ms_queue_put(obj->outputs[0], om);
				freemsg(im);
			}
		}
		return;
	}
	ms_filter_lock(obj);
	if (dt->handle!=NULL){
		spx_uint32_t inrate=0, outrate=0;
		speex_resampler_get_rate(dt->handle,&inrate,&outrate);
		if ((uint32_t)inrate!=dt->input_rate || (uint32_t)outrate!=dt->output_rate){
			speex_resampler_destroy(dt->handle);
			dt->handle=0;
		}
	}
	if (dt->handle==NULL){
		resample_init_speex(dt);
	}


	while((im=ms_queue_get(obj->inputs[0]))!=NULL){
		spx_uint32_t inlen=(spx_uint32_t)((im->b_wptr-im->b_rptr)/(2*dt->in_nchannels));
		spx_uint32_t outlen=(spx_uint32_t)(((inlen*dt->output_rate)/dt->input_rate)+1);
		spx_uint32_t inlen_orig=inlen;
		om=allocb(outlen*2*dt->in_nchannels,0);
		mblk_meta_copy(im, om);
		if (dt->in_nchannels==1){
			speex_resampler_process_int(dt->handle,
					0,
					(spx_int16_t*)im->b_rptr,
					&inlen,
					(spx_int16_t*)om->b_wptr,
					&outlen);
		}else{
			speex_resampler_process_interleaved_int(dt->handle,
					(int16_t*)im->b_rptr,
					&inlen,
					(int16_t*)om->b_wptr,
					&outlen);
		}
		if (inlen_orig!=inlen){
			ms_error("Bug in resampler ! only %u samples consumed instead of %u, out=%u",
				(unsigned int)inlen,(unsigned int)inlen_orig,(unsigned int)outlen);
		}
		om->b_wptr+=outlen*2*dt->in_nchannels;
		mblk_set_timestamp_info(om,dt->ts);
		dt->ts+=outlen;
		if (resample_channel_adapt(dt->in_nchannels, dt->out_nchannels, om, &om_chan) == 0) {
			ms_queue_put(obj->outputs[0], om);
		} else {
			ms_queue_put(obj->outputs[0], om_chan);
			freemsg(om);
		}
		freemsg(im);
	}
	ms_filter_unlock(obj);
}


static int ms_resample_set_sr(MSFilter *obj, void *arg){
	ResampleData *dt=(ResampleData*)obj->data;
	dt->input_rate=((int*)arg)[0];
	return 0;
}

static int ms_resample_set_output_sr(MSFilter *obj, void *arg){
	ResampleData *dt=(ResampleData*)obj->data;
	dt->output_rate=((int*)arg)[0];
	return 0;
}

static int set_input_nchannels(MSFilter *f, void *arg){
	ResampleData *dt=(ResampleData*)f->data;
	int chans=*(int*)arg;
	ms_filter_lock(f);
	if (dt->in_nchannels!=chans && dt->handle!=NULL){
		speex_resampler_destroy(dt->handle);
		dt->handle=NULL;
	}
	dt->in_nchannels=chans;
	ms_filter_unlock(f);
	return 0;
}

static int set_output_nchannels(MSFilter *f, void *arg) {
	ResampleData *dt = (ResampleData *)f->data;
	int chans = *(int *)arg;
	ms_filter_lock(f);
	if (dt->out_nchannels != chans && dt->handle != NULL) {
		speex_resampler_destroy(dt->handle);
		dt->handle = NULL;
	}
	dt->out_nchannels = chans;
	ms_filter_unlock(f);
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	 ,	ms_resample_set_sr		},
	{	MS_FILTER_SET_OUTPUT_SAMPLE_RATE ,	ms_resample_set_output_sr	},
	{	MS_FILTER_SET_NCHANNELS,		set_input_nchannels		},
	{	MS_FILTER_SET_OUTPUT_NCHANNELS,		set_output_nchannels		},
	{	0				 ,	NULL	}
};

#ifdef _MSC_VER

MSFilterDesc ms_resample_desc={
	MS_RESAMPLE_ID,
	"MSResample",
	N_("Audio resampler"),
	MS_FILTER_OTHER,
	NULL,
	1,
	1,
	resample_init,
	resample_preprocess,
	resample_process_ms2,
	NULL,
	resample_uninit,
	methods
};

#else

MSFilterDesc ms_resample_desc={
	.id=MS_RESAMPLE_ID,
	.name="MSResample",
	.text=N_("Audio resampler"),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=1,
	.init=resample_init,
	.preprocess=resample_preprocess,
	.process=resample_process_ms2,
	.uninit=resample_uninit,
	.methods=methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_resample_desc)



