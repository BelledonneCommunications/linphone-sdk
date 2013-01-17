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


#include "mediastreamer2/mseventqueue.h"
#include "mediastreamer2/msfilter.h"

#ifndef MS_EVENT_BUF_SIZE
#define MS_EVENT_BUF_SIZE 8192
#endif

struct _MSEventQueue{
	ms_mutex_t mutex; /*could be replaced by an atomic counter for freeroom*/
	uint8_t *rptr;
	uint8_t *wptr;
	uint8_t *endptr;
	uint8_t *lim;
	int freeroom;
	int size;
	uint8_t buffer[MS_EVENT_BUF_SIZE];
};

static void write_event(MSEventQueue *q, MSFilter *f, unsigned int ev_id, void *arg){
	int argsize=ev_id & 0xff;
	int size=argsize+16;
	uint8_t *nextpos=q->wptr+size;

	if (q->freeroom<size){
		ms_error("Dropped event, no more free space in event buffer !");
		return;
	}
	
	if (nextpos>q->lim){
		/* need to wrap around */
		q->endptr=q->wptr;
		q->wptr=q->buffer;
		nextpos=q->wptr+size;
	}
	*(long*)q->wptr=(long)f;
	*(long*)(q->wptr+8)=(long)ev_id;
	if (argsize>0) memcpy(q->wptr+16,arg,argsize);
	q->wptr=nextpos;
	ms_mutex_lock(&q->mutex);
	q->freeroom-=size;
	ms_mutex_unlock(&q->mutex);
}

static bool_t read_event(MSEventQueue *q){
	int available=q->size-q->freeroom;
	if (available>0){
		MSFilter *f;
		unsigned int id;
		void *data;
		int argsize;
		int evsize;
		
		f=(MSFilter *)*(long*)(q->rptr);
		id=(unsigned int)*(long*)(q->rptr+8);
		argsize=id & 0xff;
		evsize=argsize+16;
		data=q->rptr+16;
		if (f->notify!=NULL)
			f->notify(f->notify_ud,f,id,argsize>0 ? data : NULL);
		q->rptr+=evsize;
		if (q->rptr>=q->endptr){
			q->rptr=q->buffer;
		}
		ms_mutex_lock(&q->mutex);
		q->freeroom+=evsize;
		ms_mutex_unlock(&q->mutex);
		return TRUE;
	}
	return FALSE;
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

void ms_event_queue_destroy(MSEventQueue *q){
	ms_mutex_destroy(&q->mutex);
	ms_free(q);
}

static MSEventQueue *ms_global_event_queue=NULL;

void ms_set_global_event_queue(MSEventQueue *q){
	ms_global_event_queue=q;
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


void ms_filter_notify(MSFilter *f, unsigned int id, void *arg){
	if (f->notify!=NULL){
		if (ms_global_event_queue==NULL){
			/* synchronous notification */
			f->notify(f->notify_ud,f,id,arg);
		}else{
			write_event(ms_global_event_queue,f,id,arg);
		}
	}
}

void ms_filter_notify_synchronous(MSFilter *f, unsigned int id, void *arg){
	if (f->notify){
		f->notify(f->notify_ud,f,id,arg);
	}
}

void ms_filter_notify_no_arg(MSFilter *f, unsigned int id){
	ms_filter_notify(f,id,NULL);
}
