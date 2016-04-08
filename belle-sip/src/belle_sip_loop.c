/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"
#include "bctoolbox/map.h"

#ifndef _WIN32
#include <unistd.h>
#include <poll.h>
typedef struct pollfd belle_sip_pollfd_t;

static int belle_sip_poll(belle_sip_pollfd_t *pfd, int count, int duration){
	int err;
	err=poll(pfd,count,duration);
	if (err==-1 && errno!=EINTR)
		belle_sip_error("poll() error: %s",strerror(errno));
	return err;
}

/*
 Poll() based implementation of event loop.
 */

static int belle_sip_event_to_poll(unsigned int events){
	int ret=0;
	if (events & BELLE_SIP_EVENT_READ)
		ret|=POLLIN;
	if (events & BELLE_SIP_EVENT_WRITE)
		ret|=POLLOUT;
	if (events & BELLE_SIP_EVENT_ERROR)
		ret|=POLLERR;
	return ret;
}

static unsigned int belle_sip_poll_to_event(belle_sip_pollfd_t * pfd){
	unsigned int ret=0;
	short events=pfd->revents;
	if (events & POLLIN)
		ret|=BELLE_SIP_EVENT_READ;
	if (events & POLLOUT)
		ret|=BELLE_SIP_EVENT_WRITE;
	if (events & POLLERR)
		ret|=BELLE_SIP_EVENT_ERROR;
	return ret;
}

static void belle_sip_source_to_poll(belle_sip_source_t *s, belle_sip_pollfd_t *pfd, int i){
	pfd[i].fd=s->fd;
	pfd[i].events=belle_sip_event_to_poll(s->events);
	pfd[i].revents=0;
	s->index=i;
}

static unsigned int belle_sip_source_get_revents(belle_sip_source_t *s,belle_sip_pollfd_t *pfd){
	return belle_sip_poll_to_event(&pfd[s->index]);
}

#else


#include <malloc.h>


typedef HANDLE belle_sip_pollfd_t;

static void belle_sip_source_to_poll(belle_sip_source_t *s, belle_sip_pollfd_t *pfd,int i){
	s->index=i;
	pfd[i]=s->fd;
	
	/*special treatments for windows sockets*/
	if (s->sock!=(belle_sip_socket_t)-1){
		int err;
		long events=0;
		
		if (s->events & BELLE_SIP_EVENT_READ)
			events|=FD_READ|FD_ACCEPT;
		if (s->events & BELLE_SIP_EVENT_WRITE)
			events|=FD_WRITE|FD_CONNECT;
		if (events!=s->armed_events){
			s->armed_events=events;
			err=WSAEventSelect(s->sock,s->fd,events);
			if (err!=0) belle_sip_error("WSAEventSelect() failed: %s",belle_sip_get_socket_error_string());
		}
	}
}

static unsigned int belle_sip_source_get_revents(belle_sip_source_t *s,belle_sip_pollfd_t *pfd){
	WSANETWORKEVENTS revents={0};
	int err;
	unsigned int ret=0;
	
	if (WaitForSingleObjectEx(s->fd,0,FALSE)==WAIT_OBJECT_0){
		if (s->sock!=(belle_sip_socket_t)-1){
			/*special treatments for windows sockets*/
			err=WSAEnumNetworkEvents(s->sock,s->fd,&revents);
			if (err!=0){
				belle_sip_error("WSAEnumNetworkEvents() failed: %s socket=%x",belle_sip_get_socket_error_string(),(unsigned int)s->sock);
				return 0;
			}
			if (revents.lNetworkEvents & FD_READ || revents.lNetworkEvents & FD_ACCEPT){
				ret|=BELLE_SIP_EVENT_READ;
			}
			if (revents.lNetworkEvents & FD_WRITE || revents.lNetworkEvents & FD_CONNECT){
				ret|=BELLE_SIP_EVENT_WRITE;
			}
			s->armed_events=0;
		}else{
			ret=BELLE_SIP_EVENT_READ;
			ResetEvent(s->fd);
		}
	}
	return ret;
}

static int belle_sip_poll(belle_sip_pollfd_t *pfd, int count, int duration){
	DWORD ret;
	
	if (count == 0) {
		belle_sip_sleep(duration);
		return 0;
	}

	ret=WaitForMultipleObjectsEx(count,pfd,FALSE,duration,FALSE);
	if (ret==WAIT_FAILED){
		belle_sip_error("WaitForMultipleObjectsEx() failed.");
		return -1;
	}
	if (ret==WAIT_TIMEOUT){
		return 0;
	}
	return ret-WAIT_OBJECT_0;
}

#endif

static void belle_sip_source_destroy(belle_sip_source_t *obj){
	if (obj->node.next || obj->node.prev){
		belle_sip_fatal("Destroying source currently used in main loop !");
	}
	belle_sip_source_uninit(obj);
}

static void belle_sip_source_init(belle_sip_source_t *s, belle_sip_source_func_t func, void *data, belle_sip_fd_t fd, unsigned int events, unsigned int timeout_value_ms){
	static unsigned long global_id=1;
	s->node.data=s;
	if (s->id==0) s->id=global_id++;
	s->fd=fd;
	s->events=events;
	s->timeout=timeout_value_ms;
	s->data=data;
	s->notify=func;
	s->sock=(belle_sip_socket_t)-1;
}

void belle_sip_source_uninit(belle_sip_source_t *obj){
#ifdef _WIN32
	if (obj->sock!=(belle_sip_socket_t)-1){
		WSACloseEvent(obj->fd);
		obj->fd=(WSAEVENT)-1;
	}
#endif
	obj->fd=(belle_sip_fd_t)-1;
	obj->sock=(belle_sip_socket_t)-1;
	if (obj->it) {
		bctoolbox_iterator_delete(obj->it);
		obj->it=NULL;
	}
}

void belle_sip_source_set_notify(belle_sip_source_t *s, belle_sip_source_func_t func) {
	s->notify = func;
}

void belle_sip_socket_source_init(belle_sip_source_t *s, belle_sip_source_func_t func, void *data, belle_sip_socket_t sock, unsigned int events, unsigned int timeout_value_ms){
#ifdef _WIN32
	/*on windows, the fd to poll is not the socket */
	belle_sip_fd_t fd=(belle_sip_fd_t)-1;
	if (sock!=(belle_sip_socket_t)-1)
		fd=WSACreateEvent();
	else
		fd=(WSAEVENT)-1;
	belle_sip_source_init(s,func,data,fd,events,timeout_value_ms);
	
#else
	belle_sip_source_init(s,func,data,sock,events,timeout_value_ms);
#endif
	s->sock=sock;
	if (sock!=(belle_sip_socket_t)-1)
		belle_sip_socket_set_nonblocking(sock);
}

void belle_sip_fd_source_init(belle_sip_source_t *s, belle_sip_source_func_t func, void *data, belle_sip_fd_t fd, unsigned int events, unsigned int timeout_value_ms){
	belle_sip_source_init(s,func,data,fd,events,timeout_value_ms);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_source_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_source_t,belle_sip_object_t,belle_sip_source_destroy,NULL,NULL,FALSE);

belle_sip_source_t * belle_sip_socket_source_new(belle_sip_source_func_t func, void *data, belle_sip_socket_t sock, unsigned int events, unsigned int timeout_value_ms){
	belle_sip_source_t *s=belle_sip_object_new(belle_sip_source_t);
	belle_sip_socket_source_init(s,func,data,sock,events,timeout_value_ms);
	return s;
}

belle_sip_source_t * belle_sip_fd_source_new(belle_sip_source_func_t func, void *data, belle_sip_fd_t fd, unsigned int events, unsigned int timeout_value_ms){
	belle_sip_source_t *s=belle_sip_object_new(belle_sip_source_t);
	belle_sip_fd_source_init(s,func,data,fd,events,timeout_value_ms);
	return s;
}

belle_sip_source_t * belle_sip_timeout_source_new(belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms){
	return belle_sip_socket_source_new(func,data,(belle_sip_socket_t)-1,0,timeout_value_ms);
}

unsigned long belle_sip_source_get_id(const belle_sip_source_t *s){
	return s->id;
}
void * belle_sip_source_get_user_data(const belle_sip_source_t *s) {
	return s->data;
}
void belle_sip_source_set_user_data(belle_sip_source_t *s, void *user_data) {
	s->data = user_data;
}
int belle_sip_source_set_events(belle_sip_source_t* source, int event_mask) {
	source->events = event_mask;
	return 0;
}

belle_sip_socket_t belle_sip_source_get_socket(const belle_sip_source_t* source) {
	return source->sock;
}


struct belle_sip_main_loop{
	belle_sip_object_t base;
	belle_sip_list_t *fd_sources;
	bctoolbox_map_t *timer_sources;
	belle_sip_object_pool_t *pool;
	int nsources;
	int run;
	int in_iterate;
};

void belle_sip_main_loop_remove_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source){
	bool_t elem_removed = FALSE;
	if (source->node.next || source->node.prev || &source->node==ml->fd_sources)  {
		ml->fd_sources=belle_sip_list_remove_link(ml->fd_sources,&source->node);
		belle_sip_object_unref(source);
		elem_removed = TRUE;
	}
	if (source->it) {
		bctoolbox_map_erase(ml->timer_sources, source->it);
		bctoolbox_iterator_delete(source->it);
		source->it=NULL;
		belle_sip_object_unref(source);
		elem_removed = TRUE;
	}
	if (elem_removed) {
		source->cancelled=TRUE;
		ml->nsources--;
		if (source->on_remove)
			source->on_remove(source);
		
	}
}


static void belle_sip_main_loop_destroy(belle_sip_main_loop_t *ml){
	while (ml->fd_sources){
		belle_sip_main_loop_remove_source(ml,(belle_sip_source_t*)ml->fd_sources->data);
	}
	if (belle_sip_object_pool_cleanable(ml->pool)){
		belle_sip_object_unref(ml->pool);
	}
	bctoolbox_mmap_long_delete(ml->timer_sources);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_main_loop_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_main_loop_t,belle_sip_object_t,belle_sip_main_loop_destroy,NULL,NULL,FALSE);

belle_sip_main_loop_t *belle_sip_main_loop_new(void){
	belle_sip_main_loop_t*m=belle_sip_object_new(belle_sip_main_loop_t);
	m->pool=belle_sip_object_pool_push();
	m->timer_sources = bctoolbox_mmap_long_new();
	return m;
}

void belle_sip_main_loop_add_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source){
	if (source->node.next || source->node.prev){
		belle_sip_fatal("Source is already linked somewhere else.");
		return;
	}
	if (source->node.data!=source){
		belle_sip_fatal("Insane source passed to belle_sip_main_loop_add_source() !");
		return;
	}
	
	source->ml=ml;
	
	if (source->timeout>=0){
		belle_sip_object_ref(source);
		source->expire_ms=belle_sip_time_ms()+source->timeout;
		source->it = bctoolbox_map_insert_and_delete_with_returned_it(ml->timer_sources
																	  , (bctoolbox_pair_t*)bctoolbox_pair_long_new(source->expire_ms, source));
	}
	source->cancelled=FALSE;
	if (source->fd != -1 ) {
		belle_sip_object_ref(source);
		ml->fd_sources=belle_sip_list_prepend_link(ml->fd_sources,&source->node);
	}

	ml->nsources++;
}

belle_sip_source_t* belle_sip_main_loop_create_timeout(belle_sip_main_loop_t *ml
														, belle_sip_source_func_t func
														, void *data
														, unsigned int timeout_value_ms
														,const char* timer_name) {
	belle_sip_source_t * s=belle_sip_timeout_source_new(func,data,timeout_value_ms);
	belle_sip_object_set_name((belle_sip_object_t*)s,timer_name);
	belle_sip_main_loop_add_source(ml,s);
	return s;
}

unsigned long belle_sip_main_loop_add_timeout(belle_sip_main_loop_t *ml, belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms){
	belle_sip_source_t * s=belle_sip_main_loop_create_timeout(ml,func,data,timeout_value_ms,"Timer");
	belle_sip_object_unref(s);
	return s->id;
}

void belle_sip_main_loop_do_later(belle_sip_main_loop_t *ml, belle_sip_callback_t func, void *data){
	belle_sip_source_t * s=belle_sip_main_loop_create_timeout(ml,(belle_sip_source_func_t)func,data,0,"defered task");
	s->oneshot=TRUE;
	belle_sip_object_unref(s);
}


void belle_sip_source_set_timeout(belle_sip_source_t *s, unsigned int value_ms){
	if (!s->expired){
		s->expire_ms=belle_sip_time_ms()+value_ms;
	}
	s->timeout=value_ms;
}
void belle_sip_source_set_remove_cb(belle_sip_source_t *s, belle_sip_source_remove_callback_t func) {
	s->on_remove=func;
}
unsigned int belle_sip_source_get_timeout(const belle_sip_source_t *s){
	return s->timeout;
}
void belle_sip_source_cancel(belle_sip_source_t *s){
	if (s) s->cancelled=TRUE;
	if (s->it) {
		bctoolbox_map_erase(s->ml->timer_sources, s->it);
		bctoolbox_iterator_delete(s->it);
		s->it=NULL;
		if (s->fd == -1) {
			/*hack to make sure source is "removed at next iterate*/
			s->ml->fd_sources = belle_sip_list_prepend_link(s->ml->fd_sources,&s->node);
		}
	}
}

static int match_source_id(const void *s, const void *pid){
	if ( ((belle_sip_source_t*)s)->id==(unsigned long)(intptr_t)pid){
		return 0;
	}
	return -1;
}

belle_sip_source_t *belle_sip_main_loop_find_source(belle_sip_main_loop_t *ml, unsigned long id){
	bctoolbox_iterator_t *it;
	belle_sip_source_t *ret=NULL;
	belle_sip_list_t *elem=belle_sip_list_find_custom(ml->fd_sources,match_source_id,(const void*)(intptr_t)id);
	if (elem!=NULL) {
		ret = (belle_sip_source_t*)elem->data;
	} else if ((it = bctoolbox_map_find_custom(ml->timer_sources, match_source_id, (const void*)(intptr_t)id))) {
		ret = (belle_sip_source_t*)bctoolbox_pair_get_second(bctoolbox_iterator_get_pair(it));
		bctoolbox_iterator_delete(it);
	} /*else
		ret = NULL;*/
	
	return ret;
	
}

void belle_sip_main_loop_cancel_source(belle_sip_main_loop_t *ml, unsigned long id){
	belle_sip_source_t *s=belle_sip_main_loop_find_source(ml,id);
	belle_sip_source_cancel(s);
}

void belle_sip_main_loop_iterate(belle_sip_main_loop_t *ml){
	size_t pfd_size = ml->nsources * sizeof(belle_sip_pollfd_t);
	belle_sip_pollfd_t *pfd=(belle_sip_pollfd_t*)belle_sip_malloc0(pfd_size);
	int i=0;
	belle_sip_source_t *s;
	belle_sip_list_t *elem,*next;
	int duration=-1;
	int ret;
	uint64_t cur,next_wakeup_time;
	belle_sip_list_t *to_be_notified=NULL;
	int can_clean=belle_sip_object_pool_cleanable(ml->pool); /*iterate might not be called by the thread that created the main loop*/ 
	belle_sip_object_pool_t *tmp_pool=NULL;
	bctoolbox_iterator_t *it,*end;
	
	if (ml->in_iterate){
		belle_sip_warning("belle_sip_main_loop_iterate([%p]): reentrancy detected, doing nothing.",ml);
		return;
	}
	ml->in_iterate=TRUE;
	
	if (!can_clean){
		/*Push a temporary pool for the time of the iterate loop*/
		tmp_pool=belle_sip_object_pool_push();
	}
	
	/*Step 1: prepare the pollfd table and get the next timeout value */
	for(elem=ml->fd_sources;elem!=NULL;elem=next) {
		next=elem->next;
		s=(belle_sip_source_t*)elem->data;
		if (!s->cancelled){
			if (s->fd!=(belle_sip_fd_t)-1){
				belle_sip_source_to_poll(s,pfd,i);
				++i;
			}
		}
	}
	/*all source with timeout are in ml->timer_sources*/
	if (bctoolbox_map_size(ml->timer_sources) >0) {
		belle_sip_source_t *first_source;
		int64_t diff;
		it = bctoolbox_map_begin(ml->timer_sources);
		first_source = (belle_sip_source_t*)bctoolbox_pair_get_second(bctoolbox_iterator_get_pair(it));
		next_wakeup_time = first_source->expire_ms;
		
		/* compute the amount of time to wait for shortest timeout*/
		cur=belle_sip_time_ms();
		diff=next_wakeup_time-cur;
		if (diff>0)
			duration=(int)diff;
		else 
			duration=0;
		bctoolbox_iterator_delete(it);
		it = NULL;
	}
	
	/* do the poll */
	ret=belle_sip_poll(pfd,i,duration);
	if (ret==-1){
		goto end;
	}
	
	/* Step 2: examine poll results and determine the list of source to be notified */
	cur=belle_sip_time_ms();
	for(elem=ml->fd_sources;elem!=NULL;elem=elem->next){
		unsigned revents=0;
		s=(belle_sip_source_t*)elem->data;
		if (!s->cancelled){
			if (s->fd!=(belle_sip_fd_t)-1){
				if (s->notify_required) { /*for testing purpose to force channel to read*/
					revents=BELLE_SIP_EVENT_READ;
					s->notify_required=0; /*reset*/
				} else {
					revents=belle_sip_source_get_revents(s,pfd);
				}
				s->revents=revents;
			}
			if (revents!=0 /*|| (s->timeout>=0 && cur>=s->expire_ms)*/){
				to_be_notified=belle_sip_list_append(to_be_notified,belle_sip_object_ref(s));
				s->expired=TRUE;
				/*if (s->revents==0)
					s->revents=BELLE_SIP_EVENT_TIMEOUT;*/
			}
		}else to_be_notified=belle_sip_list_append(to_be_notified,belle_sip_object_ref(s));
	}

	/* Step 3: find timeouted sources */
	it = bctoolbox_map_begin(ml->timer_sources);
	end = bctoolbox_map_end(ml->timer_sources);
	while (!bctoolbox_iterator_equals(it,end)) {
		s = (belle_sip_source_t*)bctoolbox_pair_get_second(bctoolbox_iterator_get_pair(it));
		if (s->expire_ms > cur) {
			/* no need to continue looping because map is ordered*/
			break;
		} else {
			if (s->revents==0) {
				s->revents=BELLE_SIP_EVENT_TIMEOUT;
				to_be_notified=belle_sip_list_append(to_be_notified,belle_sip_object_ref(s));
				it=bctoolbox_iterator_get_next(it);
			} else {
				/*just remove from timeout list*/
				it=bctoolbox_map_erase(ml->timer_sources, it);
				bctoolbox_iterator_delete(s->it);
				s->it=NULL;
			}
		}
	}
	bctoolbox_iterator_delete(it);
	bctoolbox_iterator_delete(end);
	
	/* Step 4: notify those to be notified */
	for(elem=to_be_notified;elem!=NULL;){
		s=(belle_sip_source_t*)elem->data;
		next=elem->next;
		if (!s->cancelled){
			
			if (s->timeout > 0 && (__belle_sip_log_mask & BELLE_SIP_LOG_DEBUG)) {
				/*to avoid too many traces*/ 
				char *objdesc=belle_sip_object_to_string((belle_sip_object_t*)s);
				belle_sip_debug("source %s notified revents=%u, timeout=%i",objdesc,revents,s->timeout);
				belle_sip_free(objdesc);
			}
			
			ret=s->notify(s->data,s->revents);
			if (ret==BELLE_SIP_STOP || s->oneshot){
				/*this source needs to be removed*/
				belle_sip_main_loop_remove_source(ml,s);
			}else if (s->revents==BELLE_SIP_EVENT_TIMEOUT && s->timeout >= 0){
				/*timeout needs to be started again */
				if (ret==BELLE_SIP_CONTINUE_WITHOUT_CATCHUP){
					s->expire_ms=cur+s->timeout;
				}else{
					s->expire_ms+=s->timeout;
				}
				s->expired=FALSE;
				bctoolbox_map_erase(ml->timer_sources, s->it);
				bctoolbox_iterator_delete(s->it);
				s->it = bctoolbox_map_insert_and_delete_with_returned_it(ml->timer_sources
																			  , (bctoolbox_pair_t*)bctoolbox_pair_long_new(s->expire_ms, s));
			}
		} else {
			belle_sip_main_loop_remove_source(ml,s);
		}
		belle_sip_object_unref(s);
		belle_sip_free(elem); /*free just the element*/
		elem=next;
	}
	
	if (can_clean) belle_sip_object_pool_clean(ml->pool);
	else if (tmp_pool) {
		belle_sip_object_unref(tmp_pool);
		tmp_pool=NULL;
	}
end:
	
	belle_sip_free(pfd);
	ml->in_iterate=FALSE;
}

void belle_sip_main_loop_run(belle_sip_main_loop_t *ml){
	ml->run=1;
	while(ml->run){
		belle_sip_main_loop_iterate(ml);
		if (ml->in_iterate) break;
	}
}

int belle_sip_main_loop_quit(belle_sip_main_loop_t *ml){
	ml->run=0;
	return BELLE_SIP_STOP;
}

void belle_sip_main_loop_sleep(belle_sip_main_loop_t *ml, int milliseconds){
	belle_sip_source_t * s=belle_sip_main_loop_create_timeout(ml,(belle_sip_source_func_t)belle_sip_main_loop_quit,ml,milliseconds,"Main loop sleep timer");
	belle_sip_main_loop_run(ml);
	belle_sip_main_loop_remove_source(ml,s);
	belle_sip_object_unref(s);
}

