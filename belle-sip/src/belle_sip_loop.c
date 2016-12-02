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
#include <limits.h>

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
/*	if (obj->it) {
		bctbx_iterator_delete(obj->it);
		obj->it=NULL;
	}*/
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
	bctbx_map_t *timer_sources;
	belle_sip_object_pool_t *pool;
	int nsources;
	int run;
	int in_loop;
	bctbx_mutex_t timer_sources_mutex;
};

void belle_sip_main_loop_remove_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source){
	bool_t elem_removed = FALSE;
	if (source->node.next || source->node.prev || &source->node==ml->fd_sources)  {
		ml->fd_sources=belle_sip_list_remove_link(ml->fd_sources,&source->node);
		belle_sip_object_unref(source);
		elem_removed = TRUE;
	}
	if (source->it) {
		bctbx_mutex_lock(&ml->timer_sources_mutex);
		bctbx_map_erase(ml->timer_sources, source->it);
		bctbx_iterator_delete(source->it);
		bctbx_mutex_unlock(&ml->timer_sources_mutex);

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
	bctbx_mmap_ullong_delete(ml->timer_sources);
	bctbx_mutex_destroy(&ml->timer_sources_mutex);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_main_loop_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_main_loop_t,belle_sip_object_t,belle_sip_main_loop_destroy,NULL,NULL,FALSE);

belle_sip_main_loop_t *belle_sip_main_loop_new(void){
	belle_sip_main_loop_t*m=belle_sip_object_new(belle_sip_main_loop_t);
	m->pool=belle_sip_object_pool_push();
	m->timer_sources = bctbx_mmap_ullong_new();
	bctbx_mutex_init(&m->timer_sources_mutex,NULL);
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
		bctbx_mutex_lock(&ml->timer_sources_mutex);
		source->it = bctbx_map_insert_and_delete_with_returned_it(ml->timer_sources
																	  , (bctbx_pair_t*)bctbx_pair_ullong_new(source->expire_ms, source));
		bctbx_mutex_unlock(&ml->timer_sources_mutex);

	}
	source->cancelled=FALSE;
	if (source->fd != (belle_sip_fd_t)-1 ) {
		belle_sip_object_ref(source);
		ml->fd_sources=belle_sip_list_prepend_link(ml->fd_sources,&source->node);
	}

	ml->nsources++;
}

belle_sip_source_t* belle_sip_main_loop_create_timeout_with_remove_cb(  belle_sip_main_loop_t *ml
																	  , belle_sip_source_func_t func
																	  , void *data
																	  , unsigned int timeout_value_ms
																	  , const char* timer_name
																	  , belle_sip_source_remove_callback_t remove_func) {
	belle_sip_source_t * s=belle_sip_timeout_source_new(func,data,timeout_value_ms);
	belle_sip_object_set_name((belle_sip_object_t*)s,timer_name);
	if (remove_func) {
		belle_sip_source_set_remove_cb(s, remove_func);
	}
	belle_sip_main_loop_add_source(ml,s);
	return s;
}
belle_sip_source_t* belle_sip_main_loop_create_timeout(belle_sip_main_loop_t *ml
																	  , belle_sip_source_func_t func
																	  , void *data
																	  , unsigned int timeout_value_ms
																	  ,const char* timer_name) {
	return belle_sip_main_loop_create_timeout_with_remove_cb(ml, func, data, timeout_value_ms,timer_name,NULL);
	
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
		belle_sip_main_loop_t *ml = s->ml;
		s->expire_ms=belle_sip_time_ms()+value_ms;
		if (s->it){
			/*this timeout is already sorted in the timer_sources map, we need to move it to its new place*/
			bctbx_mutex_lock(&ml->timer_sources_mutex);
			bctbx_map_erase(ml->timer_sources, s->it);
			bctbx_iterator_delete(s->it);
			s->it = bctbx_map_insert_and_delete_with_returned_it(ml->timer_sources,
				(bctbx_pair_t*)bctbx_pair_ullong_new(s->expire_ms, s));
			bctbx_mutex_unlock(&ml->timer_sources_mutex);
		}
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
	s->cancelled=TRUE;
	if (s->it) {
		bctbx_mutex_lock(&s->ml->timer_sources_mutex);
		bctbx_map_erase(s->ml->timer_sources, s->it);
		bctbx_iterator_delete(s->it);
		/*put on front*/
		s->it = bctbx_map_insert_and_delete_with_returned_it(s->ml->timer_sources, (bctbx_pair_t*)bctbx_pair_ullong_new(0, s));
		
		bctbx_mutex_unlock(&s->ml->timer_sources_mutex);
	}
}

static int match_source_id(const void *s, const void *pid){
	if ( ((belle_sip_source_t*)s)->id==(unsigned long)(intptr_t)pid){
		return 0;
	}
	return -1;
}

belle_sip_source_t *belle_sip_main_loop_find_source(belle_sip_main_loop_t *ml, unsigned long id){
	bctbx_iterator_t *it;
	belle_sip_source_t *ret=NULL;
	belle_sip_list_t *elem=belle_sip_list_find_custom(ml->fd_sources,match_source_id,(const void*)(intptr_t)id);
	if (elem!=NULL) {
		ret = (belle_sip_source_t*)elem->data;
	} else if ((it = bctbx_map_find_custom(ml->timer_sources, match_source_id, (const void*)(intptr_t)id))) {
		ret = (belle_sip_source_t*)bctbx_pair_get_second(bctbx_iterator_get_pair(it));
		bctbx_iterator_delete(it);
	} /*else
		ret = NULL;*/
	
	return ret;
	
}

void belle_sip_main_loop_cancel_source(belle_sip_main_loop_t *ml, unsigned long id){
	belle_sip_source_t *s=belle_sip_main_loop_find_source(ml,id);
	if (s) belle_sip_source_cancel(s);
}

static void belle_sip_main_loop_iterate(belle_sip_main_loop_t *ml){
	size_t pfd_size = ml->nsources * sizeof(belle_sip_pollfd_t);
	belle_sip_pollfd_t *pfd=(belle_sip_pollfd_t*)belle_sip_malloc0(pfd_size);
	int i=0;
	belle_sip_source_t *s;
	belle_sip_list_t *elem,*next;
	int duration=-1;
	int ret;
	uint64_t cur;
	belle_sip_list_t *to_be_notified=NULL;
	int can_clean=belle_sip_object_pool_cleanable(ml->pool); /*iterate might not be called by the thread that created the main loop*/ 
	belle_sip_object_pool_t *tmp_pool=NULL;
	bctbx_iterator_t *it,*end;
	
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
	if (bctbx_map_size(ml->timer_sources) >0) {
		int64_t diff;
		uint64_t next_wakeup_time;
		it = bctbx_map_begin(ml->timer_sources);
		/*use first because in case of canceled timer, key ==0 , key != s->expire_ms */
		next_wakeup_time = bctbx_pair_ullong_get_first((const bctbx_pair_ullong_t *)bctbx_iterator_get_pair(it));
		/* compute the amount of time to wait for shortest timeout*/
		cur=belle_sip_time_ms();
		diff=next_wakeup_time-cur;
		if (diff>0)
			duration=MIN((unsigned int)diff,INT_MAX);
		else 
			duration=0;
		bctbx_iterator_delete(it);
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
			} else {
				belle_sip_error("Source [%p] does not contains any fd !",s);
			}
			if (revents!=0){
				to_be_notified=belle_sip_list_append(to_be_notified,belle_sip_object_ref(s));
			}
		}else to_be_notified=belle_sip_list_append(to_be_notified,belle_sip_object_ref(s));
	}

	/* Step 3: find timeouted sources */
	
	bctbx_mutex_lock(&ml->timer_sources_mutex); /*iterator chain might be alterated by element insertion*/
	it = bctbx_map_begin(ml->timer_sources);
	end = bctbx_map_end(ml->timer_sources);
	while (!bctbx_iterator_equals(it,end)) {
		/*use first because in case of canceled timer, key != s->expire_ms*/
		uint64_t expire = bctbx_pair_ullong_get_first((const bctbx_pair_ullong_t *)bctbx_iterator_get_pair(it));
		s = (belle_sip_source_t*)bctbx_pair_get_second(bctbx_iterator_get_pair(it));
		if (expire > cur) {
			/* no need to continue looping because map is ordered*/
			break;
		} else {
			if (s->revents==0) {
				s->expired=TRUE;
				to_be_notified=belle_sip_list_append(to_be_notified,belle_sip_object_ref(s));
			} /*else already in to_be_notified by Step 2*/
			
			s->revents|=BELLE_SIP_EVENT_TIMEOUT;
			it=bctbx_iterator_get_next(it);
		}
	}
	bctbx_iterator_delete(it);
	bctbx_iterator_delete(end);
	bctbx_mutex_unlock(&ml->timer_sources_mutex);
	
	/* Step 4: notify those to be notified */
	for(elem=to_be_notified;elem!=NULL;){
		s=(belle_sip_source_t*)elem->data;
		next=elem->next;
		if (!s->cancelled){
			
			if (s->timeout > 0 && belle_sip_log_level_enabled(BELLE_SIP_LOG_DEBUG)) {
				/*to avoid too many traces*/ 
				char *objdesc=belle_sip_object_to_string((belle_sip_object_t*)s);
				belle_sip_debug("source %s notified revents=%u, timeout=%i",objdesc,revents,s->timeout);
				belle_sip_free(objdesc);
			}
			
			ret=s->notify(s->data,s->revents);
			if (ret==BELLE_SIP_STOP || s->oneshot){
				/*this source needs to be removed*/
				belle_sip_main_loop_remove_source(ml,s);
			} else  {
				if (s->expired && s->it) {
					bctbx_mutex_lock(&ml->timer_sources_mutex);
					bctbx_map_erase(ml->timer_sources, s->it);
					bctbx_iterator_delete(s->it);
					bctbx_mutex_unlock(&ml->timer_sources_mutex);
					s->it=NULL;
					belle_sip_object_unref(s);
				}
				if (!s->it && s->timeout >= 0){
					/*timeout needs to be started again */
					if (ret==BELLE_SIP_CONTINUE_WITHOUT_CATCHUP){
						s->expire_ms=cur+s->timeout;
					}else{
						s->expire_ms+=s->timeout;
					}
					s->expired=FALSE;
					bctbx_mutex_lock(&ml->timer_sources_mutex);
					s->it = bctbx_map_insert_and_delete_with_returned_it(ml->timer_sources,
							(bctbx_pair_t*)bctbx_pair_ullong_new(s->expire_ms, s));
					bctbx_mutex_unlock(&ml->timer_sources_mutex);
					belle_sip_object_ref(s);
				}
			}
		} else {
			belle_sip_main_loop_remove_source(ml,s);
		}
		s->revents=0;
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
}

void belle_sip_main_loop_run(belle_sip_main_loop_t *ml){
	if (ml->in_loop){
		belle_sip_warning("belle_sip_main_loop_run(): reentrancy detected, doing nothing");
		return;
	}
	ml->run = TRUE;
	ml->in_loop = TRUE;
	while(ml->run){
		belle_sip_main_loop_iterate(ml);
	}
	ml->in_loop = FALSE;
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

