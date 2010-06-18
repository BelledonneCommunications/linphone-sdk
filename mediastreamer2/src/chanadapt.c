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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mschanadapter.h"

/*
 This filter transforms stereo buffers to mono and vice versa.
*/

typedef struct AdapterState{
	int inputchans;
	int outputchans;
}AdapterState;

static void adapter_init(MSFilter *f){
	AdapterState *s=ms_new(AdapterState,1);
	s->inputchans=1;
	s->outputchans=1;
	f->data=s;
}

static void adapter_uninit(MSFilter *f){
	ms_free(f->data);
}

static void adapter_process(MSFilter *f){
	AdapterState *s=(AdapterState*)f->data;
	mblk_t *im,*om;
	int msgsize;
	
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		if (s->inputchans==s->outputchans){
			ms_queue_put(f->outputs[0],im);
		}else if (s->inputchans==2){
			msgsize=msgdsize(im)/2;
			om=allocb(msgsize,0);
			for (;im->b_rptr<im->b_wptr;im->b_rptr+=4,om->b_wptr+=2){
				*(int16_t*)om->b_wptr=*(int16_t*)im->b_rptr;
			}
			ms_queue_put(f->outputs[0],om);
			freemsg(im);
		}else if (s->outputchans==2){
			msgsize=msgdsize(im)*2;
			om=allocb(msgsize,0);
			for (;im->b_rptr<im->b_wptr;im->b_rptr+=2,om->b_wptr+=4){
				((int16_t*)om->b_wptr)[0]=*(int16_t*)im->b_rptr;
				((int16_t*)om->b_wptr)[1]=*(int16_t*)im->b_rptr;
			}
			ms_queue_put(f->outputs[0],om);
			freemsg(im);
		}
	}
}

static int adapter_set_nchannels(MSFilter *f, void *data){
	AdapterState *s=(AdapterState*)f->data;
	s->inputchans=*(int*)data;
	return 0;
}

static int adapter_get_nchannels(MSFilter *f, void *data){
	AdapterState *s=(AdapterState*)f->data;
	*(int*)data=s->inputchans;
	return 0;
}

static int adapter_set_out_nchannels(MSFilter *f, void *data){
	AdapterState *s=(AdapterState*)f->data;
	s->outputchans=*(int*)data;
	return 0;
}

static int adapter_get_out_nchannels(MSFilter *f, void *data){
	AdapterState *s=(AdapterState*)f->data;
	*(int*)data=s->outputchans;
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_NCHANNELS , adapter_set_nchannels },
	{	MS_FILTER_GET_NCHANNELS, adapter_get_nchannels },
	{	MS_CHANNEL_ADAPTER_SET_OUTPUT_NCHANNELS, adapter_set_out_nchannels },
	{  MS_CHANNEL_ADAPTER_GET_OUTPUT_NCHANNELS, adapter_get_out_nchannels },
	{ 0,	NULL }
};


#ifdef _MSC_VER

MSFilterDesc ms_channel_adapter_desc={
	MS_CHANNEL_ADAPTER_ID,
	"MSChannelAdapter",
	N_("A filter that converts from mono to stereo and vice versa."),
	MS_FILTER_OTHER,
	NULL,
	1,
	1,
	adapter_init,
	NULL,
	adapter_process,
	NULL,
	adapter_uninit,
	methods
};

#else

MSFilterDesc ms_channel_adapter_desc={
	.id=MS_CHANNEL_ADAPTER_ID,
	.name="MSChannelAdapter",
	.text=N_("A filter that converts from mono to stereo and vice versa."),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=1,
	.init=adapter_init,
	.process=adapter_process,
	.uninit=adapter_uninit,
	.methods=methods
};
#endif

MS_FILTER_DESC_EXPORT(ms_channel_adapter_desc)
