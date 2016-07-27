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
#include "mediastreamer2/msticker.h"

#define MS_FILTER_METHOD_GET_FID(id)	(((id)>>16) & 0xFFFF)
#define MS_FILTER_METHOD_GET_INDEX(id) ( ((id)>>8) & 0XFF)

/* we need this pragma because this file implements much of compatibility functions*/
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

void ms_filter_register(MSFilterDesc *desc){
	MSFactory *factory = ms_factory_get_fallback();
	if(factory) {
		ms_factory_register_filter(factory,desc);
	} else {
		ms_error("ms_filter_register(): registration of '%s' filter has failed: no fallback factory has been defined", desc->name);
	}
}

void ms_filter_unregister_all(){
}

bool_t ms_filter_codec_supported(const char *mime){
	return ms_factory_codec_supported(ms_factory_get_fallback(),mime);
}

MSFilterDesc * ms_filter_get_encoding_capturer(const char *mime) {
	return ms_factory_get_encoding_capturer(ms_factory_get_fallback(),mime);
}

MSFilterDesc * ms_filter_get_decoding_renderer(const char *mime) {
	return ms_factory_get_decoding_renderer(ms_factory_get_fallback(),mime);
}

MSFilterDesc * ms_filter_get_encoder(const char *mime){
	return ms_factory_get_encoder(ms_factory_get_fallback(),mime);
}

MSFilterDesc * ms_filter_get_decoder(const char *mime){
	return ms_factory_get_decoder(ms_factory_get_fallback(),mime);
}

MSFilter * ms_filter_create_encoder(const char *mime){
	return ms_factory_create_encoder(ms_factory_get_fallback(),mime);
}

MSFilter * ms_filter_create_decoder(const char *mime){
	return ms_factory_create_decoder(ms_factory_get_fallback(),mime);
}

MSFilter *ms_filter_new_from_desc(MSFilterDesc *desc){
	return ms_factory_create_filter_from_desc(ms_factory_get_fallback(),desc);
}

MSFilter *ms_filter_new(MSFilterId id){
	return ms_factory_create_filter(ms_factory_get_fallback(),id);
}

MSFilterDesc *ms_filter_lookup_by_name(const char *filter_name){
	return ms_factory_lookup_filter_by_name(ms_factory_get_fallback(),filter_name);
}

bool_t ms_filter_desc_implements_interface(MSFilterDesc *desc, MSFilterInterfaceId id){
	MSFilterMethod *methods=desc->methods;
	if (!methods) return FALSE;
	for(;methods->id!=0;methods++){
		unsigned int fid=MS_FILTER_METHOD_GET_FID(methods->id);
		if (fid==id) return TRUE;
	}
	return FALSE;
}

bool_t ms_filter_implements_interface(MSFilter *f, MSFilterInterfaceId id){
	return ms_filter_desc_implements_interface(f->desc,id);
}

bctbx_list_t *ms_filter_lookup_by_interface(MSFilterInterfaceId id){
	return ms_factory_lookup_filter_by_interface(ms_factory_get_fallback(),id);
}

MSFilter *ms_filter_new_from_name(const char *filter_name){
	return ms_factory_create_filter_from_name(ms_factory_get_fallback(),filter_name);
}


MSFilterId ms_filter_get_id(MSFilter *f){
	return f->desc->id;
}

const char * ms_filter_get_name(MSFilter *f) {
	return f->desc->name;
}

int ms_filter_link(MSFilter *f1, int pin1, MSFilter *f2, int pin2){
	MSQueue *q;
	ms_message("ms_filter_link: %s:%p,%i-->%s:%p,%i",f1->desc->name,f1,pin1,f2->desc->name,f2,pin2);
	ms_return_val_if_fail(pin1<f1->desc->noutputs, -1);
	ms_return_val_if_fail(pin2<f2->desc->ninputs, -1);
	ms_return_val_if_fail(f1->outputs[pin1]==NULL,-1);
	ms_return_val_if_fail(f2->inputs[pin2]==NULL,-1);
	q=ms_queue_new(f1,pin1,f2,pin2);
	f1->outputs[pin1]=q;
	f2->inputs[pin2]=q;
	return 0;
}

int ms_filter_unlink(MSFilter *f1, int pin1, MSFilter *f2, int pin2){
	MSQueue *q;
	ms_message("ms_filter_unlink: %s:%p,%i-->%s:%p,%i",f1 ? f1->desc->name : "!NULL!",f1,pin1,f2 ? f2->desc->name : "!NULL!",f2,pin2);
	ms_return_val_if_fail(pin1<f1->desc->noutputs, -1);
	ms_return_val_if_fail(pin2<f2->desc->ninputs, -1);
	ms_return_val_if_fail(f1->outputs[pin1]!=NULL,-1);
	ms_return_val_if_fail(f2->inputs[pin2]!=NULL,-1);
	ms_return_val_if_fail(f1->outputs[pin1]==f2->inputs[pin2],-1);
	q=f1->outputs[pin1];
	f1->outputs[pin1]=f2->inputs[pin2]=0;
	ms_queue_destroy(q);
	return 0;
}

static MS2_INLINE bool_t is_interface_method(unsigned int magic){
	return magic==MS_FILTER_BASE_ID || magic>MSFilterInterfaceBegin;
}

static int _ms_filter_call_method(MSFilter *f, unsigned int id, void *arg){
	MSFilterMethod *methods=f->desc->methods;
	int i;
	unsigned int magic=MS_FILTER_METHOD_GET_FID(id);
	if (!is_interface_method(magic) && magic!=f->desc->id) {
		ms_fatal("Method type checking failed when calling %u on filter %s",id,f->desc->name);
		return -1;
	}
	for(i=0;methods!=NULL && methods[i].method!=NULL; i++){
		unsigned int mm=MS_FILTER_METHOD_GET_FID(methods[i].id);
		if (mm!=f->desc->id && !is_interface_method(mm)) {
			ms_fatal("Bad method definition on filter %s. fid=%u , mm=%u",f->desc->name,f->desc->id,mm);
			return -1;
		}
		if (methods[i].id==id){
			return methods[i].method(f,arg);
		}
	}
	if (magic!=MS_FILTER_BASE_ID) ms_error("no such method on filter %s, fid=%i method index=%i",f->desc->name,magic,
	                           MS_FILTER_METHOD_GET_INDEX(id) );
	return -1;
}

int ms_filter_call_method(MSFilter *f, unsigned int id, void *arg){
	/*compatibility stuff*/
	if (id==MS_AUDIO_DECODER_SET_RTP_PAYLOAD_PICKER && !ms_filter_has_method(f,MS_AUDIO_DECODER_SET_RTP_PAYLOAD_PICKER))
		id=MS_FILTER_SET_RTP_PAYLOAD_PICKER;
	return _ms_filter_call_method(f,id,arg);
}

bool_t ms_filter_has_method(MSFilter *f, unsigned int id){
	MSFilterMethod *methods=f->desc->methods;
	int i;
	for(i=0;methods!=NULL && methods[i].method!=NULL; i++)
		if (methods[i].id==id) return TRUE;
	return FALSE;
}

int ms_filter_call_method_noarg(MSFilter *f, unsigned int id){
	return ms_filter_call_method(f,id,NULL);
}

void ms_filter_destroy(MSFilter *f){
	if (f->desc->uninit!=NULL)
		f->desc->uninit(f);
	if (f->inputs!=NULL)	ms_free(f->inputs);
	if (f->outputs!=NULL)	ms_free(f->outputs);
	ms_mutex_destroy(&f->lock);
	ms_filter_clear_notify_callback(f);
	ms_filter_clean_pending_events(f);
	ms_free(f);
}

void ms_filter_process(MSFilter *f){
	MSTimeSpec start,stop;
	ms_debug("Executing process of filter %s:%p",f->desc->name,f);

	if (f->stats)
		ms_get_cur_time(&start);

	f->desc->process(f);
	if (f->stats){
		ms_get_cur_time(&stop);
		f->stats->count++;
		f->stats->elapsed+=(stop.tv_sec-start.tv_sec)*1000000000LL + (stop.tv_nsec-start.tv_nsec);
	}

}

void ms_filter_task_process(MSFilterTask *task){
	MSTimeSpec start,stop;
	MSFilter *f=task->f;
	/*ms_message("Executing task of filter %s:%p",f->desc->name,f);*/

	if (f->stats)
		ms_get_cur_time(&start);

	task->taskfunc(f);
	if (f->stats){
		ms_get_cur_time(&stop);
		f->stats->count++;
		f->stats->elapsed+=(stop.tv_sec-start.tv_sec)*1000000000LL + (stop.tv_nsec-start.tv_nsec);
	}
	f->postponed_task--;
}

void ms_filter_preprocess(MSFilter *f, struct _MSTicker *t){
	f->last_tick=0;
	f->ticker=t;
	if (f->desc->preprocess!=NULL)
		f->desc->preprocess(f);
}

void ms_filter_postprocess(MSFilter *f){
	if (f->desc->postprocess!=NULL)
		f->desc->postprocess(f);
	f->ticker=NULL;
}

bool_t ms_filter_inputs_have_data(MSFilter *f){
	int i;
	for(i=0;i<f->desc->ninputs;i++){
		MSQueue *q=f->inputs[i];
		if (q!=NULL && q->q.q_mcount>0) return TRUE;
	}
	return FALSE;
}

void ms_filter_postpone_task(MSFilter *f, MSFilterFunc taskfunc){
	MSFilterTask *task;
	MSTicker *ticker=f->ticker;
	if (ticker==NULL){
		ms_error("ms_filter_postpone_task(): this method cannot be called outside of filter's process method.");
		return;
	}
	task=ms_new0(MSFilterTask,1);
	task->f=f;
	task->taskfunc=taskfunc;
	ticker->task_list=bctbx_list_prepend(ticker->task_list,task);
	f->postponed_task++;
}

static void find_filters(bctbx_list_t **filters, MSFilter *f ){
	int i,found;
	MSQueue *link;
	if (f==NULL) ms_fatal("Bad graph.");
	/*ms_message("seeing %s, seen=%i",f->desc->name,f->seen);*/
	if (f->seen){
		return;
	}
	f->seen=TRUE;
	*filters=bctbx_list_append(*filters,f);
	/* go upstream */
	for(i=0;i<f->desc->ninputs;i++){
		link=f->inputs[i];
		if (link!=NULL) find_filters(filters,link->prev.filter);
	}
	/* go downstream */
	for(i=0,found=0;i<f->desc->noutputs;i++){
		link=f->outputs[i];
		if (link!=NULL) {
			found++;
			find_filters(filters,link->next.filter);
		}
	}
	if (f->desc->noutputs>=1 && found==0){
		ms_fatal("Bad graph: filter %s has %i outputs, none is connected.",f->desc->name,f->desc->noutputs);
	}
}

bctbx_list_t * ms_filter_find_neighbours(MSFilter *me){
	bctbx_list_t *l=NULL;
	bctbx_list_t *it;
	find_filters(&l,me);
	/*reset seen boolean for further lookups to succeed !*/
	for(it=l;it!=NULL;it=it->next){
		MSFilter *f=(MSFilter*)it->data;
		f->seen=FALSE;
	}
	return l;
}

void ms_connection_helper_start(MSConnectionHelper *h){
	h->last.filter=0;
	h->last.pin=-1;
}

int ms_connection_helper_link(MSConnectionHelper *h, MSFilter *f, int inpin, int outpin){
	int err=0;
	if (h->last.filter==NULL){
		h->last.filter=f;
		h->last.pin=outpin;
	}else{
		err=ms_filter_link(h->last.filter,h->last.pin,f,inpin);
		if (err==0){
			h->last.filter=f;
			h->last.pin=outpin;
		}
	}
	return err;
}

int ms_connection_helper_unlink(MSConnectionHelper *h, MSFilter *f, int inpin, int outpin){
	int err=0;
	if (h->last.filter==NULL){
		h->last.filter=f;
		h->last.pin=outpin;
	}else{
		err=ms_filter_unlink(h->last.filter,h->last.pin,f,inpin);
		if (err==0){
			h->last.filter=f;
			h->last.pin=outpin;
		}
	}
	return err;
}

void ms_filter_enable_statistics(bool_t enabled){
	ms_factory_enable_statistics(ms_factory_get_fallback(),enabled);
}

const bctbx_list_t * ms_filter_get_statistics(void){
	return ms_factory_get_statistics(ms_factory_get_fallback());
}

void ms_filter_reset_statistics(void){
	ms_factory_reset_statistics(ms_factory_get_fallback());
}

void ms_filter_log_statistics(void){
	ms_factory_log_statistics(ms_factory_get_fallback());
}

const char * ms_format_type_to_string(MSFormatType type){
	switch(type){
		case MSAudio: return "MSAudio";
		case MSVideo: return "MSVideo";
		case MSText: return "MSText";
		case MSUnknownMedia: return "MSUnknownMedia";
	}
	return "invalid";
}
