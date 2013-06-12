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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"


static MSList *desc_list=NULL;
static bool_t statistics_enabled=FALSE;
static MSList *stats_list=NULL;

static int compare_stats_with_name(const MSFilterStats *stat, const char *name){
	return strcmp(stat->name,name);
}

static MSFilterStats *find_or_create_stats(MSFilterDesc *desc){
	MSList *elem=ms_list_find_custom(stats_list,(MSCompareFunc)compare_stats_with_name,desc->name);
	MSFilterStats *ret=NULL;
	if (elem==NULL){
		ret=ms_new0(MSFilterStats,1);
		ret->name=desc->name;
		stats_list=ms_list_append(stats_list,ret);
	}else ret=(MSFilterStats*)elem->data;
	return ret;
}

void ms_filter_register(MSFilterDesc *desc){
	if (desc->id==MS_FILTER_NOT_SET_ID){
		ms_fatal("MSFilterId for %s not set !",desc->name);
	}
	/*lastly registered encoder/decoders may replace older ones*/
	desc_list=ms_list_prepend(desc_list,desc);
}

void ms_filter_unregister_all(){
	if (desc_list!=NULL) {
		ms_list_free(desc_list);
		desc_list=NULL;
	}
	if (stats_list!=NULL){
		ms_list_for_each(stats_list,ms_free);
		ms_list_free(stats_list);
		stats_list=NULL;
	}
}

bool_t ms_filter_codec_supported(const char *mime){
	MSFilterDesc *enc = ms_filter_get_encoding_capturer(mime);
	MSFilterDesc *dec = ms_filter_get_decoding_renderer(mime);

	if (enc == NULL) enc = ms_filter_get_encoder(mime);
	if (dec == NULL) dec = ms_filter_get_decoder(mime);

	if(enc!=NULL && dec!=NULL) return TRUE;

	if(enc==NULL) ms_message("Could not find encoder for %s", mime);
	if(dec==NULL) ms_message("Could not find decoder for %s", mime);
	return FALSE;
}

MSFilterDesc * ms_filter_get_encoding_capturer(const char *mime) {
	MSList *elem;

	for (elem = desc_list; elem != NULL; elem = ms_list_next(elem)) {
		MSFilterDesc *desc = (MSFilterDesc *)elem->data;
		if (desc->category == MS_FILTER_ENCODING_CAPTURER) {
			char *saveptr=NULL;
			char *enc_fmt = ms_strdup(desc->enc_fmt);
			char *token = strtok_r(enc_fmt, " ", &saveptr);
			while (token != NULL) {
				if (strcasecmp(token, mime) == 0) {
					break;
				}
				token = strtok_r(NULL, " ", &saveptr);
			}
			ms_free(enc_fmt);
			if (token != NULL) return desc;
		}
	}

	return NULL;
}

MSFilterDesc * ms_filter_get_decoding_renderer(const char *mime) {
	MSList *elem;

	for (elem = desc_list; elem != NULL; elem = ms_list_next(elem)) {
		MSFilterDesc *desc = (MSFilterDesc *)elem->data;
		if (desc->category == MS_FILTER_DECODING_RENDERER) {
			char *saveptr=NULL;
			char *enc_fmt = ms_strdup(desc->enc_fmt);
			char *token = strtok_r(enc_fmt, " ", &saveptr);
			while (token != NULL) {
				if (strcasecmp(token, mime) == 0) {
					break;
				}
				token = strtok_r(NULL, " ", &saveptr);
			}
			ms_free(enc_fmt);
			if (token != NULL) return desc;
		}
	}

	return NULL;
}

MSFilterDesc * ms_filter_get_encoder(const char *mime){
	MSList *elem;
	for (elem=desc_list;elem!=NULL;elem=ms_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if (desc->category==MS_FILTER_ENCODER && 
			strcasecmp(desc->enc_fmt,mime)==0){
			return desc;
		}
	}
	return NULL;
}

MSFilterDesc * ms_filter_get_decoder(const char *mime){
	MSList *elem;
	for (elem=desc_list;elem!=NULL;elem=ms_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if (desc->category==MS_FILTER_DECODER && 
			strcasecmp(desc->enc_fmt,mime)==0){
			return desc;
		}
	}
	return NULL;
}

MSFilter * ms_filter_create_encoder(const char *mime){
	MSFilterDesc *desc=ms_filter_get_encoder(mime);
	if (desc!=NULL) return ms_filter_new_from_desc(desc);
	return NULL;
}

MSFilter * ms_filter_create_decoder(const char *mime){
	MSFilterDesc *desc=ms_filter_get_decoder(mime);
	if (desc!=NULL) return ms_filter_new_from_desc(desc);
	return NULL;
}

MSFilter *ms_filter_new_from_desc(MSFilterDesc *desc){
	MSFilter *obj;
	obj=(MSFilter *)ms_new0(MSFilter,1);
	ms_mutex_init(&obj->lock,NULL);
	obj->desc=desc;
	if (desc->ninputs>0)	obj->inputs=(MSQueue**)ms_new0(MSQueue*,desc->ninputs);
	if (desc->noutputs>0)	obj->outputs=(MSQueue**)ms_new0(MSQueue*,desc->noutputs);

	if (statistics_enabled){
		obj->stats=find_or_create_stats(desc);
	}
	if (obj->desc->init!=NULL)
		obj->desc->init(obj);
	return obj;
}

MSFilter *ms_filter_new(MSFilterId id){
	MSList *elem;
	if (id==MS_FILTER_PLUGIN_ID){
		ms_warning("cannot create plugin filters with ms_filter_new_from_id()");
		return NULL;
	}
	for (elem=desc_list;elem!=NULL;elem=ms_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if (desc->id==id){
			return ms_filter_new_from_desc(desc);
		}
	}
	ms_error("No such filter with id %i",id);
	return NULL;
}

MSFilterDesc *ms_filter_lookup_by_name(const char *filter_name){
	MSList *elem;
	for (elem=desc_list;elem!=NULL;elem=ms_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if (strcmp(desc->name,filter_name)==0){
			return desc;
		}
	}
	return NULL;
}

MSFilter *ms_filter_new_from_name(const char *filter_name){
	MSFilterDesc *desc=ms_filter_lookup_by_name(filter_name);
	if (desc==NULL) return NULL;
	return ms_filter_new_from_desc(desc);
}


MSFilterId ms_filter_get_id(MSFilter *f){
	return f->desc->id;
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

#define MS_FILTER_METHOD_GET_FID(id)	(((id)>>16) & 0xFFFF)
#define MS_FILTER_METHOD_GET_INDEX(id) ( ((id)>>8) & 0XFF) 

static inline bool_t is_interface_method(unsigned int magic){
	return magic==MS_FILTER_BASE_ID || magic>MSFilterInterfaceBegin;
}

int ms_filter_call_method(MSFilter *f, unsigned int id, void *arg){
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

void ms_filter_set_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *ud){
	f->notify=fn;
	f->notify_ud=ud;
}

void ms_filter_destroy(MSFilter *f){
	if (f->desc->uninit!=NULL)
		f->desc->uninit(f);
	if (f->inputs!=NULL)	ms_free(f->inputs);
	if (f->outputs!=NULL)	ms_free(f->outputs);
	ms_mutex_destroy(&f->lock);
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
	task=ms_new(MSFilterTask,1);
	task->f=f;
	task->taskfunc=taskfunc;
	ticker->task_list=ms_list_prepend(ticker->task_list,task);
	f->postponed_task++;
}

static void find_filters(MSList **filters, MSFilter *f ){
	int i,found;
	MSQueue *link;
	if (f==NULL) ms_fatal("Bad graph.");
	/*ms_message("seeing %s, seen=%i",f->desc->name,f->seen);*/
	if (f->seen){
		return;
	}
	f->seen=TRUE;
	*filters=ms_list_append(*filters,f);
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

MSList * ms_filter_find_neighbours(MSFilter *me){
	MSList *l=NULL;
	MSList *it;
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
	statistics_enabled=enabled;
}

const MSList * ms_filter_get_statistics(void){
	return stats_list;
}

void ms_filter_reset_statistics(void){
	MSList *elem;
	
	for(elem=stats_list;elem!=NULL;elem=elem->next){
		MSFilterStats *stats=(MSFilterStats *)elem->data;
		stats->elapsed=0;
		stats->count=0;
	}
}

static int usage_compare(const MSFilterStats *s1, const MSFilterStats *s2){
	if (s1->elapsed==s2->elapsed) return 0;
	if (s1->elapsed<s2->elapsed) return 1;
	return -1;
}


void ms_filter_log_statistics(void){
	MSList *sorted=NULL;
	MSList *elem;
	uint64_t total=1;
	for(elem=stats_list;elem!=NULL;elem=elem->next){
		MSFilterStats *stats=(MSFilterStats *)elem->data;
		sorted=ms_list_insert_sorted(sorted,stats,(MSCompareFunc)usage_compare);
		total+=stats->elapsed;
	}
	ms_message("===========================================================");
	ms_message("                  FILTER USAGE STATISTICS                  ");
	ms_message("Name                Count     Time/tick (ms)      CPU Usage");
	ms_message("-----------------------------------------------------------");
	for(elem=sorted;elem!=NULL;elem=elem->next){
		MSFilterStats *stats=(MSFilterStats *)elem->data;
		double percentage=100.0*((double)stats->elapsed)/(double)total;
		double tpt=((double)stats->elapsed*1e-6)/((double)stats->count+1.0);
		ms_message("%-19s %-9i %-19g %-10g",stats->name,stats->count,tpt,percentage);
	}
	ms_message("===========================================================");
	ms_list_free(sorted);
}

