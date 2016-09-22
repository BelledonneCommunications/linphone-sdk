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


#include "mediastreamer2/mseventqueue.h"
#include "mediastreamer2/msfilter.h"

#ifndef MS_EVENT_BUF_SIZE
#define MS_EVENT_BUF_SIZE 8192
#endif

typedef enum {
	OnlySynchronous,
	OnlyAsynchronous,
	Both
}InvocationMode;

static void ms_filter_invoke_callbacks(MSFilter **f, unsigned int id, void *arg, InvocationMode synchronous_mode);

struct _MSNotifyContext{
	MSFilterNotifyFunc fn;
	void *ud;
	int synchronous;
};

typedef struct _MSNotifyContext MSNotifyContext;

struct _MSEventQueue{
	ms_mutex_t mutex; /*could be replaced by an atomic counter for freeroom*/
	uint8_t *rptr;
	uint8_t *wptr;
	uint8_t *endptr;
	uint8_t *lim;
	int freeroom;
	int size;
	MSFilter *current_notifier;
	uint8_t buffer[MS_EVENT_BUF_SIZE];
};

typedef struct {
	MSFilter* filter;
	unsigned int ev_id;
	int pad;
} MSEventHeader;

static int round_size(int sz) {
	return (sz + (sizeof(void*) - 1)) & ~(sizeof(void*) - 1);
}

static void write_event(MSEventQueue *q, MSFilter *f, unsigned int ev_id, void *arg){
	int argsize=ev_id & 0xff;
	int size=round_size(argsize);
	uint8_t *nextpos;
	int header_size = sizeof(MSEventHeader);
	size += header_size;
	ms_mutex_lock(&q->mutex);
	nextpos=q->wptr+size;

	if (q->freeroom<size){
		ms_mutex_unlock(&q->mutex);
		ms_error("Dropped event, no more free space in event buffer !");
		return;
	}

	if (nextpos>q->lim){
		/* need to wrap around */
		q->endptr=q->wptr;
		q->wptr=q->buffer;
		nextpos=q->wptr+size;
	}

	if (((intptr_t)q->wptr % 4) != 0) ms_fatal("Unaligned access");
	((MSEventHeader *)q->wptr)->filter = f;
	((MSEventHeader *)q->wptr)->ev_id = ev_id;

	if (argsize > 0) memcpy(q->wptr + header_size, arg, argsize);
	q->wptr=nextpos;

	/* buffer actual size(q->endptr) may have grown within the limit q->lim, prevent unwanted reading reset to the begining by setting the actual endptr */
	if (nextpos>q->endptr) {
		q->endptr=nextpos;
	}

	q->freeroom-=size;
	ms_mutex_unlock(&q->mutex);
}

static int parse_event(uint8_t *rptr, MSFilter **f, unsigned int *id, void **data, int *argsize){
	int evsize;
	int header_size = sizeof(MSEventHeader);

	if (((intptr_t)rptr % 4) != 0) ms_fatal("Unaligned access");
	*f = ((MSEventHeader *)rptr)->filter;
	*id = ((MSEventHeader *)rptr)->ev_id;

	*argsize = (*id) & 0xff;
	evsize = round_size((*argsize)) + header_size;
	*data = rptr + header_size;
	return evsize;
}

static bool_t read_event(MSEventQueue *q){
	int available=q->size-q->freeroom;
	if (available>0){
		MSFilter *f;
		unsigned int id;
		void *data;
		int argsize;
		int evsize;

		ms_mutex_lock(&q->mutex);/*q->endptr can be changed by write_event() so mutex is needed*/
		if (q->rptr>=q->endptr){
			q->rptr=q->buffer;
		}
		ms_mutex_unlock(&q->mutex);

		evsize=parse_event(q->rptr,&f,&id,&data,&argsize);
		if (f) {
			q->current_notifier=f;
			ms_filter_invoke_callbacks(&q->current_notifier,id,argsize>0 ? data : NULL, OnlyAsynchronous);
			q->current_notifier=NULL;
		}
		q->rptr+=evsize;

		ms_mutex_lock(&q->mutex);
		q->freeroom+=evsize;
		ms_mutex_unlock(&q->mutex);
		return TRUE;
	}
	return FALSE;
}

/*clean all events belonging to a MSFilter that is about to be destroyed*/
void ms_event_queue_clean(MSEventQueue *q, MSFilter *destroyed){
	int freeroom=q->freeroom;
	uint8_t *rptr=q->rptr;

	while(q->size>freeroom){
		MSFilter *f;
		unsigned int id;
		void *data;
		int argsize;
		int evsize;

		evsize=parse_event(rptr,&f,&id,&data,&argsize);
		if (f==destroyed){
			ms_message("Cleaning pending event of MSFilter [%s:%p]",destroyed->desc->name,destroyed);
			((MSEventHeader*)rptr)->filter = NULL;
		}
		rptr+=evsize;

		if (rptr>=q->endptr){
			rptr=q->buffer;
		}
		freeroom+=evsize;
	}
	if (q->current_notifier==destroyed){
		q->current_notifier=NULL;
	}
}

MSEventQueue *ms_event_queue_new(){
	MSEventQueue *q=ms_new0(MSEventQueue,1);
	int bufsize=MS_EVENT_BUF_SIZE;
	ms_mutex_init(&q->mutex,NULL);
	q->lim=q->buffer+bufsize;
	q->freeroom=bufsize;
	q->wptr=q->rptr=q->buffer;
	q->endptr=q->lim;
	q->size=bufsize;
	return q;
}

//void ms_event_queue_destroy(MSEventQueue *q){
//	/*compatibility code*/
//	if (q==ms_factory_get_event_queue(ms_factory_get_fallback())){
//		ms_factory_set_event_queue(ms_factory_get_fallback(),NULL);
//	}
//	ms_mutex_destroy(&q->mutex);
//	ms_free(q);
//}

void ms_event_queue_destroy(MSEventQueue *q){
	/*compatibility code*/
	ms_mutex_destroy(&q->mutex);
	ms_free(q);
}

void ms_event_queue_skip(MSEventQueue *q){
	int bufsize=q->size;
	q->lim=q->buffer+bufsize;
	q->freeroom=bufsize;
	q->wptr=q->rptr=q->buffer;
	q->endptr=q->lim;
}


void ms_event_queue_pump(MSEventQueue *q){
	while(read_event(q)){
	}
}

static MSNotifyContext * ms_notify_context_new(MSFilterNotifyFunc fn, void *ud, bool_t synchronous){
	MSNotifyContext *ctx=ms_new0(MSNotifyContext,1);
	ctx->fn=fn;
	ctx->ud=ud;
	ctx->synchronous=synchronous;
	return ctx;
}

static void ms_notify_context_destroy(MSNotifyContext *obj){
	ms_free(obj);
}

void ms_filter_add_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *ud, bool_t synchronous){
	f->notify_callbacks=bctbx_list_append(f->notify_callbacks,ms_notify_context_new(fn,ud,synchronous));
}

void ms_filter_remove_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *ud){
	bctbx_list_t *elem;
	bctbx_list_t *found=NULL;
	for(elem=f->notify_callbacks;elem!=NULL;elem=elem->next){
		MSNotifyContext *ctx=(MSNotifyContext*)elem->data;
		if (ctx->fn==fn && ctx->ud==ud){
			found=elem;
			break;
		}
	}
	if (found){
		ms_notify_context_destroy((MSNotifyContext*)found->data);
		f->notify_callbacks=bctbx_list_erase_link(f->notify_callbacks,found);
	}else ms_warning("ms_filter_remove_notify_callback(filter=%p): no registered callback with fn=%p and ud=%p",f,fn,ud);
}

void ms_filter_clear_notify_callback(MSFilter *f){
	f->notify_callbacks=bctbx_list_free_with_data(f->notify_callbacks,(void (*)(void*))ms_notify_context_destroy);
}

static void ms_filter_invoke_callbacks(MSFilter **f, unsigned int id, void *arg, InvocationMode synchronous_mode){
	bctbx_list_t *elem;
	for (elem=(*f)->notify_callbacks;elem!=NULL;elem=elem->next){
		MSNotifyContext *ctx=(MSNotifyContext*)elem->data;
		if (synchronous_mode==Both || (synchronous_mode==OnlyAsynchronous && !ctx->synchronous)
			|| (synchronous_mode==OnlySynchronous && ctx->synchronous)){
			ctx->fn(ctx->ud,*f,id,arg);
		}
		if (*f==NULL) break; /*the filter was destroyed by a callback invocation*/
	}
}

void ms_filter_set_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *ud){
	ms_filter_add_notify_callback(f,fn,ud,FALSE);
}


void ms_filter_notify(MSFilter *f, unsigned int id, void *arg){
	if (f->notify_callbacks!=NULL){
		if (f->factory->evq==NULL){
			/* synchronous notification */
			ms_filter_invoke_callbacks(&f,id,arg,Both);
		}else{
			ms_filter_invoke_callbacks(&f,id,arg,OnlySynchronous);
			write_event(f->factory->evq,f,id,arg);
		}
	}
}

void ms_filter_notify_no_arg(MSFilter *f, unsigned int id){
	ms_filter_notify(f,id,NULL);
}

void ms_filter_clean_pending_events(MSFilter *f){
	if (f->factory->evq)
		ms_event_queue_clean(f->factory->evq,f);
}

/* we need this pragma because this file implements much of compatibility functions*/
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

void ms_set_global_event_queue(MSEventQueue *q){
	ms_factory_set_event_queue(ms_factory_get_fallback(),q);
}


