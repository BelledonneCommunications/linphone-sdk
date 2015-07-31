/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2011 Yann Diorcet(yann.diorcet@linphone.org)

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

#include <stdint.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/mscodecutils.h>
#include <mediastreamer2/msticker.h>
#include "mediastreamer2/msgenericplc.h"
#ifdef HAVE_G729B
#include "bcg729/decoder.h"
#endif

/* define transition duration in ms when starting/ending a comfort noise period - it introduces an equivalent delay */
#define TRANSITION_DELAY 1

/*filter common method*/
typedef struct {
	MSConcealerContext* concealer;
	int rate;
	int nchannels;
	MSCngData cng_data;
	bool_t cng_set;
	bool_t cng_running;
	unsigned char *continuity_buffer;
#ifdef HAVE_G729B
	bcg729DecoderChannelContextStruct *decoderChannelContext;
#endif
} generic_plc_struct;

const static unsigned int MAX_PLC_COUNT = UINT32_MAX;

static void generic_plc_transition_mix(int16_t *inout_buffer, int16_t *continuity_buffer, size_t fading_sample_nbr) {
	int i;
	for (i=0; i<fading_sample_nbr; i++) {
		float progress = ((float) i)/fading_sample_nbr;
		inout_buffer[i] = (int16_t)((float)continuity_buffer[i]*(1-progress) + (float)inout_buffer[i]*progress);
	}
}

static void generic_plc_init(MSFilter *f) {
	generic_plc_struct *mgps = (generic_plc_struct*) ms_new0(generic_plc_struct, 1);
#ifdef HAVE_G729B
	mgps->decoderChannelContext = initBcg729DecoderChannel(); /* initialize bcg729 decoder for CNG */
#endif
	mgps->concealer = ms_concealer_context_new(MAX_PLC_COUNT);
	mgps->nchannels = 1;
	f->data = mgps;
}

static void generic_plc_preprocess(MSFilter *f) {
	generic_plc_struct *mgps=(generic_plc_struct*)f->data;
	mgps->continuity_buffer = ms_malloc0(mgps->rate*sizeof(int16_t)*TRANSITION_DELAY/1000); /* continuity buffer introduce a 2ms delay */
}

static void generic_plc_process(MSFilter *f) {
	generic_plc_struct *mgps=(generic_plc_struct*)f->data;
	unsigned int buff_size = mgps->rate*sizeof(int16_t)*mgps->nchannels*f->ticker->interval/1000;
	mblk_t *m;
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		int transitionBufferSize = mgps->rate*sizeof(int16_t)*TRANSITION_DELAY/1000;
		unsigned char buf[128];
		unsigned int time = (1000*(m->b_wptr - m->b_rptr))/(mgps->rate*sizeof(int16_t)*mgps->nchannels);
		ms_concealer_inc_sample_time(mgps->concealer, f->ticker->time, time, TRUE);

		/* introduce 16 sample delay (2ms) */
		memcpy(buf, m->b_wptr-transitionBufferSize, transitionBufferSize);
		memmove(m->b_rptr+transitionBufferSize, m->b_rptr, m->b_wptr - m->b_rptr - transitionBufferSize);
		memcpy(m->b_rptr, mgps->continuity_buffer, transitionBufferSize);
		memcpy(mgps->continuity_buffer, buf, transitionBufferSize);

		if (mgps->cng_running){
			/*we were doing CNG, now resuming with normal audio*/
			int16_t continuity_buffer[80];
#ifdef HAVE_G729B
			bcg729Decoder(mgps->decoderChannelContext, NULL, 0, 1, 1, 1, continuity_buffer);
#else
			memset (continuity_buffer, 0, 80*sizeof(int16_t));
#endif
			memcpy(m->b_rptr, continuity_buffer, transitionBufferSize);
			generic_plc_transition_mix((int16_t *)(m->b_rptr+transitionBufferSize), &(continuity_buffer[mgps->rate*TRANSITION_DELAY/1000]), mgps->rate*TRANSITION_DELAY/1000);
			mgps->cng_running=FALSE;
			mgps->cng_set=FALSE;
		}

		ms_queue_put(f->outputs[0], m);
	}
	if (ms_concealer_context_is_concealement_required(mgps->concealer, f->ticker->time)) {
#ifdef HAVE_G729B
		m = allocb(buff_size, 0);
		
		/* Transmitted CNG data is in mgps->cng_data : give it to bcg729 decoder -> output in m->b_wptr */
		if (mgps->cng_set) { /* received some CNG data */
			mgps->cng_set=FALSE; /* reset flag */
			mgps->cng_running=TRUE;

			bcg729Decoder(mgps->decoderChannelContext, mgps->cng_data.data, mgps->cng_data.datasize, 0, 1, 1, (int16_t *)(m->b_wptr));
			mblk_set_cng_flag(m, 1);
			generic_plc_transition_mix((int16_t *)m->b_wptr, (int16_t *)mgps->continuity_buffer, mgps->rate*TRANSITION_DELAY/1000);
			/* TODO: if ticker->interval is not 10 ms which is also G729 frame length, we must generate untransmitted frame CNG until we reach the requested data amount */
		} else if (mgps->cng_running) { /* missing frame but CNG is ongoing: shall be an untransmitted frame */
			bcg729Decoder(mgps->decoderChannelContext, NULL, 0, 1, 1, 1, (int16_t *)(m->b_wptr));
			mblk_set_cng_flag(m, 1);
		} else {
			mblk_set_plc_flag(m, 1);
			memset(m->b_wptr, 0, buff_size);
		}
#else
		m = allocb(buff_size, 0);
		if (!mgps->cng_running && mgps->cng_set){
			mgps->cng_running=TRUE;
			mblk_set_cng_flag(m, 1);
			/*TODO do something with the buffer*/
		}else{
			mblk_set_plc_flag(m, 1);
		}
		memset(m->b_wptr, 0, buff_size);

#endif
		m->b_wptr += buff_size;
		ms_queue_put(f->outputs[0], m);
		ms_concealer_inc_sample_time(mgps->concealer, f->ticker->time, f->ticker->interval, FALSE);
	}
}

static void generic_plc_unit(MSFilter *f) {
	generic_plc_struct *mgps = (generic_plc_struct*) f->data;
	ms_free(mgps->continuity_buffer);
	ms_concealer_context_destroy(mgps->concealer);
#ifdef HAVE_G729B
	closeBcg729DecoderChannel(mgps->decoderChannelContext);
#endif
	ms_free(mgps);
}

static int generic_plc_get_sr(MSFilter *f, void *arg){
	generic_plc_struct *s=(generic_plc_struct*)f->data;
	((int*)arg)[0]=s->rate;
	return 0;
}

static int generic_plc_set_sr(MSFilter *f, void *arg){
	generic_plc_struct *s=(generic_plc_struct*)f->data;
	s->rate=((int*)arg)[0];
	return 0;
}

static int generic_plc_set_nchannels(MSFilter *f, void *arg) {
	generic_plc_struct *s = (generic_plc_struct *)f->data;
	s->nchannels = *(int *)arg;
	return 0;
}

static int generic_plc_set_cn(MSFilter *f, void *arg) {
	generic_plc_struct *s = (generic_plc_struct *)f->data;
	memcpy(&s->cng_data,arg, sizeof(MSCngData));
	s->cng_set=TRUE;
	return 0;
}

static MSFilterMethod generic_plc_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE	,	generic_plc_set_sr		},
	{	MS_FILTER_GET_SAMPLE_RATE	,	generic_plc_get_sr		},
	{	MS_FILTER_SET_NCHANNELS		,	generic_plc_set_nchannels	},
	{	MS_GENERIC_PLC_SET_CN		,	generic_plc_set_cn		},
	{ 	0				,	NULL 				}
};

#ifdef _MSC_VER

MSFilterDesc ms_genericplc_desc= {
	MS_GENERIC_PLC_ID,
	"MSGenericPLC",
	N_("Generic PLC."),
	MS_FILTER_OTHER,
	NULL,
	1,
	1,
	generic_plc_init,
	generic_plc_preprocess,
	generic_plc_process,
	NULL,
	generic_plc_unit,
	generic_plc_methods,
	MS_FILTER_IS_PUMP
};

#else

MSFilterDesc ms_genericplc_desc = {
	.id = MS_GENERIC_PLC_ID,
	.name = "MSGenericPLC",
	.text = N_("Generic PLC."),
	.category = MS_FILTER_OTHER,
	.ninputs = 1,
	.noutputs = 1,
	.init = generic_plc_init,
	.preprocess = generic_plc_preprocess,
	.process = generic_plc_process,
	.uninit = generic_plc_unit,
	.flags = MS_FILTER_IS_PUMP,
	.methods = generic_plc_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_genericplc_desc)
