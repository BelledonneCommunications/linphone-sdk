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

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/mscodecutils.h>
#include <mediastreamer2/msticker.h>

/*filter common method*/
typedef struct {
	MSConcealerContext* concealer;
	int rate;
	int nchannels;
} generic_plc_struct;

const static unsigned int MAX_PLC_COUNT = UINT32_MAX;

static void generic_plc_init(MSFilter *f) {
	generic_plc_struct *mgps = (generic_plc_struct*) ms_new0(generic_plc_struct, 1);
	mgps->concealer = ms_concealer_context_new(MAX_PLC_COUNT);
	mgps->nchannels = 1;
	f->data = mgps;

}

static void generic_plc_process(MSFilter *f) {
	generic_plc_struct *mgps=(generic_plc_struct*)f->data;
	unsigned int buff_size = mgps->rate*sizeof(int16_t)*mgps->nchannels*f->ticker->interval/1000;
	mblk_t *m;
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		unsigned int time = (1000*(m->b_wptr - m->b_rptr))/(mgps->rate*sizeof(int16_t)*mgps->nchannels);
		ms_concealer_inc_sample_time(mgps->concealer, f->ticker->time, time, TRUE);
		ms_queue_put(f->outputs[0], m);
	}
	if (ms_concealer_context_is_concealement_required(mgps->concealer, f->ticker->time)) {
		m = allocb(buff_size, 0);
		memset(m->b_wptr, 0, buff_size);
		m->b_wptr += buff_size;
		ms_queue_put(f->outputs[0], m);
		mblk_set_plc_flag(m, 1);
		ms_concealer_inc_sample_time(mgps->concealer, f->ticker->time, f->ticker->interval, FALSE);
	}
}

static void generic_plc_unit(MSFilter *f) {
	generic_plc_struct *mgps = (generic_plc_struct*) f->data;
	ms_concealer_context_destroy(mgps->concealer);
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

static MSFilterMethod generic_plc_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE	,	generic_plc_set_sr		},
	{	MS_FILTER_GET_SAMPLE_RATE	,	generic_plc_get_sr		},
	{	MS_FILTER_SET_NCHANNELS		,	generic_plc_set_nchannels	},
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
	NULL,
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
	.process = generic_plc_process,
	.uninit = generic_plc_unit,
	.flags = MS_FILTER_IS_PUMP,
	.methods = generic_plc_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_genericplc_desc)
