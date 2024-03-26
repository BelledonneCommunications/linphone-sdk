/*
 * Copyright (c) 2012-2019 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "bctoolbox/map.h"
#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"
#include <limits.h>

#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
typedef struct pollfd belle_sip_pollfd_t;

static int belle_sip_poll(belle_sip_pollfd_t *pfd, int count, int duration) {
	int err;
	err = poll(pfd, count, duration);
	if (err == -1 && errno != EINTR) belle_sip_error("poll() error: %s", strerror(errno));
	return err;
}

/*
 Poll() based implementation of event loop.
 */

static int belle_sip_event_to_poll(unsigned int events) {
	int ret = 0;
	if (events & BELLE_SIP_EVENT_READ) ret |= POLLIN;
	if (events & BELLE_SIP_EVENT_WRITE) ret |= POLLOUT;
	if (events & BELLE_SIP_EVENT_ERROR) ret |= POLLERR;
	return ret;
}

static unsigned int belle_sip_poll_to_event(belle_sip_pollfd_t *pfd) {
	unsigned int ret = 0;
	short events = pfd->revents;
	if (events & POLLIN) ret |= BELLE_SIP_EVENT_READ;
	if (events & POLLOUT) ret |= BELLE_SIP_EVENT_WRITE;
	if (events & POLLERR) ret |= BELLE_SIP_EVENT_ERROR;
	return ret;
}

static void belle_sip_source_to_poll(belle_sip_source_t *s, belle_sip_pollfd_t *pfd, int i) {
	pfd[i].fd = s->fd;
	pfd[i].events = belle_sip_event_to_poll(s->events);
	pfd[i].revents = 0;
	s->index = i;
}

static unsigned int belle_sip_source_get_revents(belle_sip_source_t *s, belle_sip_pollfd_t *pfd) {
	return belle_sip_poll_to_event(&pfd[s->index]);
}

#else

#include <malloc.h>

typedef HANDLE belle_sip_pollfd_t;

static void belle_sip_source_to_poll(belle_sip_source_t *s, belle_sip_pollfd_t *pfd, int i) {
	s->index = i;
	pfd[i] = s->fd;

	/*special treatments for windows sockets*/
	if (s->sock != (belle_sip_socket_t)-1) {
		int err;
		long events = 0;

		if (s->events & BELLE_SIP_EVENT_READ) events |= FD_READ | FD_ACCEPT;
		if (s->events & BELLE_SIP_EVENT_WRITE) events |= FD_WRITE | FD_CONNECT;
		if (events != s->armed_events) {
			s->armed_events = events;
			err = WSAEventSelect(s->sock, s->fd, events);
			if (err != 0) belle_sip_error("WSAEventSelect() failed: %s", belle_sip_get_socket_error_string());
		}
	}
}

static unsigned int belle_sip_source_get_revents(belle_sip_source_t *s, belle_sip_pollfd_t *pfd) {
	WSANETWORKEVENTS revents = {0};
	int err;
	unsigned int ret = 0;

	if (WaitForSingleObjectEx(s->fd, 0, FALSE) == WAIT_OBJECT_0) {
		if (s->sock != (belle_sip_socket_t)-1) {
			/*special treatments for windows sockets*/
			err = WSAEnumNetworkEvents(s->sock, s->fd, &revents);
			if (err != 0) {
				belle_sip_error("WSAEnumNetworkEvents() failed: %s socket=%x", belle_sip_get_socket_error_string(),
				                (unsigned int)s->sock);
				return 0;
			}
			if (revents.lNetworkEvents & FD_READ || revents.lNetworkEvents & FD_ACCEPT) {
				ret |= BELLE_SIP_EVENT_READ;
			}
			if (revents.lNetworkEvents & FD_WRITE || revents.lNetworkEvents & FD_CONNECT) {
				ret |= BELLE_SIP_EVENT_WRITE;
			}
			s->armed_events = 0;
		} else {
			ret = BELLE_SIP_EVENT_READ;
			ResetEvent(s->fd);
		}
	}
	return ret;
}

static int belle_sip_poll(belle_sip_pollfd_t *pfd, int count, int duration) {
	DWORD ret;

	if (count == 0) {
		belle_sip_sleep(duration);
		return 0;
	}

	ret = WaitForMultipleObjectsEx(count, pfd, FALSE, duration, FALSE);
	if (ret == WAIT_FAILED) {
		belle_sip_error("WaitForMultipleObjectsEx() failed.");
		return -1;
	}
	if (ret == WAIT_TIMEOUT) {
		return 0;
	}
	return ret - WAIT_OBJECT_0;
}

#endif

static void belle_sip_source_destroy(belle_sip_source_t *obj) {
	if (obj->node.next || obj->node.prev) {
		belle_sip_fatal("Destroying source currently used in main loop !");
	}
	belle_sip_source_uninit(obj);
}

static void belle_sip_source_init(belle_sip_source_t *s,
                                  belle_sip_source_func_t func,
                                  void *data,
                                  belle_sip_fd_t fd,
                                  unsigned int events,
                                  unsigned int timeout_value_ms) {
	static unsigned long global_id = 1;
	s->node.data = s;
	if (s->id == 0) s->id = global_id++;
	s->fd = fd;
	s->events = events;
	s->timeout = timeout_value_ms;
	s->data = data;
	s->notify = func;
	s->sock = (belle_sip_socket_t)-1;
}

void belle_sip_source_uninit(belle_sip_source_t *obj) {
#ifdef _WIN32
	if (obj->sock != (belle_sip_socket_t)-1) {
		WSACloseEvent(obj->fd);
		obj->fd = (WSAEVENT)-1;
	}
#endif
	obj->fd = (belle_sip_fd_t)-1;
	obj->sock = (belle_sip_socket_t)-1;
	/*	if (obj->it) {
	        bctbx_iterator_delete(obj->it);
	        obj->it=NULL;
	    }*/
}

void belle_sip_source_set_notify(belle_sip_source_t *s, belle_sip_source_func_t func) {
	s->notify = func;
}

void belle_sip_socket_source_init(belle_sip_source_t *s,
                                  belle_sip_source_func_t func,
                                  void *data,
                                  belle_sip_socket_t sock,
                                  unsigned int events,
                                  unsigned int timeout_value_ms) {
#ifdef _WIN32
	/*on windows, the fd to poll is not the socket */
	belle_sip_fd_t fd = (belle_sip_fd_t)-1;
	if (sock != (belle_sip_socket_t)-1) fd = WSACreateEvent();
	else fd = (WSAEVENT)-1;
	belle_sip_source_init(s, func, data, fd, events, timeout_value_ms);

#else
	belle_sip_source_init(s, func, data, sock, events, timeout_value_ms);
#endif
	s->sock = sock;
	if (sock != (belle_sip_socket_t)-1) belle_sip_socket_set_nonblocking(sock);
}

void belle_sip_fd_source_init(belle_sip_source_t *s,
                              belle_sip_source_func_t func,
                              void *data,
                              belle_sip_fd_t fd,
                              unsigned int events,
                              unsigned int timeout_value_ms) {
	belle_sip_source_init(s, func, data, fd, events, timeout_value_ms);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_source_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_source_t, belle_sip_object_t, belle_sip_source_destroy, NULL, NULL, FALSE);

belle_sip_source_t *belle_sip_socket_source_new(belle_sip_source_func_t func,
                                                void *data,
                                                belle_sip_socket_t sock,
                                                unsigned int events,
                                                unsigned int timeout_value_ms) {
	belle_sip_source_t *s = belle_sip_object_new(belle_sip_source_t);
	belle_sip_socket_source_init(s, func, data, sock, events, timeout_value_ms);
	return s;
}

belle_sip_source_t *belle_sip_fd_source_new(
    belle_sip_source_func_t func, void *data, belle_sip_fd_t fd, unsigned int events, unsigned int timeout_value_ms) {
	belle_sip_source_t *s = belle_sip_object_new(belle_sip_source_t);
	belle_sip_fd_source_init(s, func, data, fd, events, timeout_value_ms);
	return s;
}

belle_sip_source_t *
belle_sip_timeout_source_new(belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms) {
	return belle_sip_socket_source_new(func, data, (belle_sip_socket_t)-1, 0, timeout_value_ms);
}

unsigned long belle_sip_source_get_id(const belle_sip_source_t *s) {
	return s->id;
}
void *belle_sip_source_get_user_data(const belle_sip_source_t *s) {
	return s->data;
}
void belle_sip_source_set_user_data(belle_sip_source_t *s, void *user_data) {
	s->data = user_data;
}
int belle_sip_source_set_events(belle_sip_source_t *source, int event_mask) {
	source->events = event_mask;
	return 0;
}

belle_sip_socket_t belle_sip_source_get_socket(const belle_sip_source_t *source) {
	return source->sock;
}

struct belle_sip_main_loop {
	belle_sip_object_t base;
	belle_sip_list_t *fd_sources;
	bctbx_map_t *timer_sources;
	bctbx_mutex_t
	    sources_mutex; // mutex to avoid concurency between source addition/removing/cancelling and main loop iteration.
	belle_sip_object_pool_t *pool;
	int nsources;
	int run;
	int in_loop;
#ifndef _WIN32
	int control_fds[2];
	unsigned long thread_id;
#endif
};

static void belle_sip_main_loop_remove_source_internal(belle_sip_main_loop_t *ml,
                                                       belle_sip_source_t *source,
                                                       bool_t destroy_timer_sources) {
	int unrefs = 0;

	bctbx_mutex_lock(&ml->sources_mutex);
	if (source->node.next || source->node.prev || &source->node == ml->fd_sources) {
		ml->fd_sources = belle_sip_list_remove_link(ml->fd_sources, &source->node);
		unrefs++;
	}
	if (source->it) {
		if (destroy_timer_sources) bctbx_map_erase(ml->timer_sources, source->it);
		bctbx_iterator_delete(source->it);
		source->it = NULL;
		unrefs++;
	}
	if (unrefs) {
		source->cancelled = TRUE;
		ml->nsources--;
		bctbx_mutex_unlock(&ml->sources_mutex);
		if (source->on_remove) source->on_remove(source);
		// the mutex must be taken for unrefing because belle_sip_object_unref() isn't thread-safe
		bctbx_mutex_lock(&ml->sources_mutex);
		for (; unrefs > 0; --unrefs)
			belle_sip_object_unref(source);
	}
	bctbx_mutex_unlock(&ml->sources_mutex);
}

void belle_sip_main_loop_remove_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source) {
	belle_sip_main_loop_remove_source_internal(ml, source, TRUE);
}

static void belle_sip_main_loop_destroy(belle_sip_main_loop_t *ml) {
	bctbx_iterator_t *it = bctbx_map_ullong_begin(ml->timer_sources);
	bctbx_iterator_t *end = bctbx_map_ullong_end(ml->timer_sources);

	while (!bctbx_iterator_ullong_equals(it, end)) {
		belle_sip_main_loop_remove_source_internal(
		    ml, (belle_sip_source_t *)bctbx_pair_ullong_get_second(bctbx_iterator_ullong_get_pair(it)), FALSE);
		it = bctbx_iterator_ullong_get_next(it);
	}

	bctbx_iterator_ullong_delete(it);
	bctbx_iterator_ullong_delete(end);

	while (ml->fd_sources) {
		belle_sip_main_loop_remove_source(ml, (belle_sip_source_t *)ml->fd_sources->data);
	}
	if (belle_sip_object_pool_cleanable(ml->pool)) {
		belle_sip_object_unref(ml->pool);
	}

	bctbx_mmap_ullong_delete(ml->timer_sources);
	bctbx_mutex_destroy(&ml->sources_mutex);

#ifndef _WIN32
	close(ml->control_fds[0]);
	close(ml->control_fds[1]);
#endif
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_main_loop_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_main_loop_t, belle_sip_object_t, belle_sip_main_loop_destroy, NULL, NULL, FALSE);

belle_sip_main_loop_t *belle_sip_main_loop_new(void) {
	belle_sip_main_loop_t *m = belle_sip_object_new(belle_sip_main_loop_t);
	m->pool = belle_sip_object_pool_push();
	m->timer_sources = bctbx_mmap_ullong_new();
	bctbx_mutex_init(&m->sources_mutex, NULL);

#ifndef _WIN32
	if (pipe(m->control_fds) == -1) {
		belle_sip_fatal("Cannot create control pipe of main loop thread: %s", strerror(errno));
	}
	if (fcntl(m->control_fds[0], F_SETFL, O_NONBLOCK) < 0) {
		belle_sip_fatal("Fail to set O_NONBLOCK flag on the reading fd of the control pipe: %s", strerror(errno));
	}
	m->thread_id = 0;
#endif

	return m;
}

void belle_sip_main_loop_add_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source) {
	bctbx_mutex_lock(&ml->sources_mutex);
	if (source->node.next || source->node.prev) {
		belle_sip_fatal("Source is already linked somewhere else.");
	}
	if (source->node.data != source) {
		belle_sip_fatal("Insane source passed to belle_sip_main_loop_add_source() !");
	}

	source->ml = ml;

	if (source->timeout >= 0) {
		belle_sip_object_ref(source);
		source->expire_ms = belle_sip_time_ms() + source->timeout;
		source->it = bctbx_map_insert_and_delete_with_returned_it(
		    ml->timer_sources, (bctbx_pair_t *)bctbx_pair_ullong_new(source->expire_ms, source));
	}
	source->cancelled = FALSE;
	if (source->fd != (belle_sip_fd_t)-1) {
		belle_sip_object_ref(source);
		ml->fd_sources = belle_sip_list_concat(&source->node, ml->fd_sources);
	}

	ml->nsources++;
#ifndef _WIN32
	if (ml->thread_id != bctbx_thread_self()) belle_sip_main_loop_wake_up(ml);
#endif

	bctbx_mutex_unlock(&ml->sources_mutex);
}

belle_sip_source_t *belle_sip_main_loop_create_timeout_with_remove_cb(belle_sip_main_loop_t *ml,
                                                                      belle_sip_source_func_t func,
                                                                      void *data,
                                                                      unsigned int timeout_value_ms,
                                                                      const char *timer_name,
                                                                      belle_sip_source_remove_callback_t remove_func) {
	belle_sip_source_t *s = belle_sip_timeout_source_new(func, data, timeout_value_ms);
	belle_sip_object_set_name((belle_sip_object_t *)s, timer_name);
	if (remove_func) {
		belle_sip_source_set_remove_cb(s, remove_func);
	}
	belle_sip_main_loop_add_source(ml, s);
	return s;
}
belle_sip_source_t *belle_sip_main_loop_create_timeout(belle_sip_main_loop_t *ml,
                                                       belle_sip_source_func_t func,
                                                       void *data,
                                                       unsigned int timeout_value_ms,
                                                       const char *timer_name) {
	return belle_sip_main_loop_create_timeout_with_remove_cb(ml, func, data, timeout_value_ms, timer_name, NULL);
}

unsigned long belle_sip_main_loop_add_timeout(belle_sip_main_loop_t *ml,
                                              belle_sip_source_func_t func,
                                              void *data,
                                              unsigned int timeout_value_ms) {
	belle_sip_source_t *s = belle_sip_main_loop_create_timeout(ml, func, data, timeout_value_ms, "Timer");
	belle_sip_object_unref(s);
	return s->id;
}

typedef struct {
	belle_sip_callback_t func;
	void *user_data;
	belle_sip_source_t *source;
} DoLaterData;

static int _do_later_cb(void *user_data, unsigned int event) {
	DoLaterData *data = (DoLaterData *)user_data;
	data->func(data->user_data);
	belle_sip_object_unref(data->source);
	belle_sip_free(data);
	return BELLE_SIP_STOP;
}

void belle_sip_main_loop_do_later_with_name(belle_sip_main_loop_t *ml,
                                            belle_sip_callback_t func,
                                            void *data,
                                            const char *timer_name) {
	DoLaterData *dolater_data = belle_sip_new0(DoLaterData);
	dolater_data->func = func;
	dolater_data->user_data = data;

	/* The belle_sip_source_t is stored in dolater_data in order to decrement the refcounter in the
	   same thread than the main loop's. Otherwise, the ref counter may be corrupted because of race conditions
	   if the belle_sip_source_t was after the call to belle_sip_main_loop_add_source() */
	dolater_data->source = belle_sip_timeout_source_new(_do_later_cb, dolater_data, 0);

	belle_sip_object_set_name((belle_sip_object_t *)dolater_data->source, timer_name ? timer_name : "deferred task");
	dolater_data->source->oneshot = TRUE;

	/* This function MUST be the last to guarantee thread-safety. */
	belle_sip_main_loop_add_source(ml, dolater_data->source);
}

void belle_sip_main_loop_do_later(belle_sip_main_loop_t *ml, belle_sip_callback_t func, void *data) {
	belle_sip_main_loop_do_later_with_name(ml, func, data, NULL);
}

void belle_sip_source_set_timeout(belle_sip_source_t *s, unsigned int value_ms) {
	/*
	   WARNING: that's important to cast 'value_ms' into 'int' before giving it to belle_sip_source_set_timeout_int64()
	   in order to mimic the behavior of belle_sip_source_set_timeout() when the timeout was declared as 'int'
	   in the belle_sip_source_t structure.
	   That allows to write belle_sip_source_set_timeout(s, -1) to disable the timeout.
	*/
	belle_sip_source_set_timeout_int64(s, (int)value_ms);
}

void belle_sip_source_set_timeout_int64(belle_sip_source_t *s, int64_t value_ms) {
	belle_sip_main_loop_t *ml = s->ml;
	int removed_from_map = FALSE;
	// take the mutex only when the source has been added to the mail loop
	if (ml) bctbx_mutex_lock(&ml->sources_mutex);
	if (!s->expired) {
		s->expire_ms = belle_sip_time_ms() + value_ms;
		if (s->it) {
			/*this timeout is already sorted in the timer_sources map, we need to move it to its new place*/
			bctbx_map_erase(ml->timer_sources, s->it);
			bctbx_iterator_delete(s->it);
			if (value_ms != -1) {
				s->it = bctbx_map_insert_and_delete_with_returned_it(
				    ml->timer_sources, (bctbx_pair_t *)bctbx_pair_ullong_new(s->expire_ms, s));
			} else {
				s->it = NULL;
				removed_from_map = TRUE;
			}
		}
	}
	s->timeout = value_ms;
	if (removed_from_map) belle_sip_object_unref(s);
	if (ml) bctbx_mutex_unlock(&ml->sources_mutex);
}

void belle_sip_source_set_remove_cb(belle_sip_source_t *s, belle_sip_source_remove_callback_t func) {
	s->on_remove = func;
}

unsigned int belle_sip_source_get_timeout(const belle_sip_source_t *s) {
	return (unsigned int)s->timeout;
}

int64_t belle_sip_source_get_timeout_int64(const belle_sip_source_t *s) {
	return s->timeout;
}

void belle_sip_source_cancel(belle_sip_source_t *s) {
	if (s->ml) {
		bctbx_mutex_lock(&s->ml->sources_mutex);
		s->cancelled = TRUE;
		if (s->it) {
			bctbx_map_erase(s->ml->timer_sources, s->it);
			bctbx_iterator_delete(s->it);
			/*put on front*/
			s->it = bctbx_map_insert_and_delete_with_returned_it(s->ml->timer_sources,
			                                                     (bctbx_pair_t *)bctbx_pair_ullong_new(0, s));
		}
		bctbx_mutex_unlock(&s->ml->sources_mutex);
	} else {
		s->cancelled = TRUE;
	}
}

static int match_source_id(const void *s, const void *pid) {
	if (((belle_sip_source_t *)s)->id == (unsigned long)(intptr_t)pid) {
		return 0;
	}
	return -1;
}

belle_sip_source_t *belle_sip_main_loop_find_source(belle_sip_main_loop_t *ml, unsigned long id) {
	bctbx_iterator_t *it;
	belle_sip_source_t *ret = NULL;
	belle_sip_list_t *elem = belle_sip_list_find_custom(ml->fd_sources, match_source_id, (const void *)(intptr_t)id);
	if (elem != NULL) {
		ret = (belle_sip_source_t *)elem->data;
	} else if ((it = bctbx_map_find_custom(ml->timer_sources, match_source_id, (const void *)(intptr_t)id))) {
		ret = (belle_sip_source_t *)bctbx_pair_get_second(bctbx_iterator_get_pair(it));
		bctbx_iterator_delete(it);
	} /*else
	    ret = NULL;*/

	return ret;
}

void belle_sip_main_loop_cancel_source(belle_sip_main_loop_t *ml, unsigned long id) {
	belle_sip_source_t *s = belle_sip_main_loop_find_source(ml, id);
	if (s) belle_sip_source_cancel(s);
}

/**
 * Clear all data of a pipe.
 * @param[in] read_fd Opened file descriptor used to read the pipe. This fd MUST have O_NONBLOCK flag set.
 * @return On success, return the number of bytes that have been cleard or zero if the pipe was already empty.
 * On failure, return -1 and set errno.
 */
static ssize_t clear_pipe(int read_fd) {
	char buffer[1024];
	ssize_t nread, cum_nread = 0;
	do {
		nread = read(read_fd, buffer, sizeof(buffer));
		if (nread > 0) cum_nread += nread;
	} while (nread > 0);
	if (nread < 0 && errno != EAGAIN) return -1;
	else return cum_nread;
}

static void belle_sip_main_loop_iterate(belle_sip_main_loop_t *ml) {
	size_t pfd_size = (ml->nsources + 1) * sizeof(belle_sip_pollfd_t);
	belle_sip_pollfd_t *pfd = (belle_sip_pollfd_t *)belle_sip_malloc0(pfd_size);
	int i = 0;
	belle_sip_list_t *elem, *next;
	int duration = -1;
	int ret;
	uint64_t cur;
	belle_sip_list_t *to_be_notified = NULL;
	int can_clean = belle_sip_object_pool_cleanable(
	    ml->pool); /*iterate might not be called by the thread that created the main loop*/
	belle_sip_object_pool_t *tmp_pool = NULL;
	bctbx_iterator_t *it, *end;

	if (!can_clean) {
		/*Push a temporary pool for the time of the iterate loop*/
		tmp_pool = belle_sip_object_pool_push();
	}

	/*Step 1: prepare the pollfd table and get the next timeout value */
	bctbx_mutex_lock(&ml->sources_mutex); // Lock for the whole step 1
	for (elem = ml->fd_sources; elem != NULL; elem = next) {
		next = elem->next;
		belle_sip_source_t *s = (belle_sip_source_t *)elem->data;
		if (!s->cancelled) {
			if (s->fd != (belle_sip_fd_t)-1) {
				belle_sip_source_to_poll(s, pfd, i);
				++i;
			}
		}
	}
#ifndef _WIN32
	pfd[i].fd = ml->control_fds[0];
	pfd[i].events = POLLIN;
	++i;
#endif
	/*all source with timeout are in ml->timer_sources*/
	if (bctbx_map_size(ml->timer_sources) > 0) {
		int64_t diff;
		uint64_t next_wakeup_time;
		it = bctbx_map_begin(ml->timer_sources);
		/*use first because in case of canceled timer, key ==0 , key != s->expire_ms */
		next_wakeup_time = bctbx_pair_ullong_get_first((const bctbx_pair_ullong_t *)bctbx_iterator_get_pair(it));
		/* compute the amount of time to wait for shortest timeout*/
		cur = belle_sip_time_ms();
		diff = next_wakeup_time - cur;
		if (diff > 0) duration = MIN((unsigned int)diff, INT_MAX);
		else duration = 0;
		bctbx_iterator_delete(it);
		it = NULL;
	}
	bctbx_mutex_unlock(&ml->sources_mutex);

	/* do the poll */
	ret = belle_sip_poll(pfd, i, duration);
	if (ret == -1) {
		goto end;
	}

#ifndef _WIN32
	if (pfd[i - 1].revents == POLLIN) {
		if (clear_pipe(ml->control_fds[0]) == -1)
			belle_sip_fatal("Cannot read control pipe of main loop thread: %s", strerror(errno));
	}
#endif

	/* Step 2: examine poll results and determine the list of source to be notified */
	bctbx_mutex_lock(&ml->sources_mutex); // Lock for step 2 and step 3.
	cur = belle_sip_time_ms();
	for (elem = ml->fd_sources; elem != NULL; elem = elem->next) {
		unsigned revents = 0;
		belle_sip_source_t *s = (belle_sip_source_t *)elem->data;
		if (!s->cancelled) {
			if (s->fd != (belle_sip_fd_t)-1) {
				if (s->notify_required) { /*for testing purpose to force channel to read*/
					revents = BELLE_SIP_EVENT_READ;
					s->notify_required = 0; /*reset*/
				} else {
					revents = belle_sip_source_get_revents(s, pfd);
				}
				s->revents = revents;
			} else {
				belle_sip_error("Source [%p] does not contains any fd !", s);
			}
			if (revents != 0) {
				to_be_notified = belle_sip_list_append(to_be_notified, belle_sip_object_ref(s));
			}
		} else to_be_notified = belle_sip_list_append(to_be_notified, belle_sip_object_ref(s));
	}

	/* Step 3: find timeouted sources */
	it = bctbx_map_begin(ml->timer_sources);
	end = bctbx_map_end(ml->timer_sources);
	while (!bctbx_iterator_equals(it, end)) {
		/*use first because in case of canceled timer, key != s->expire_ms*/
		uint64_t expire = bctbx_pair_ullong_get_first((const bctbx_pair_ullong_t *)bctbx_iterator_get_pair(it));
		belle_sip_source_t *s = (belle_sip_source_t *)bctbx_pair_get_second(bctbx_iterator_get_pair(it));
		if (expire > cur) {
			/* no need to continue looping because map is ordered*/
			break;
		} else {
			if (s->revents == 0) {
				s->expired = TRUE;
				to_be_notified = belle_sip_list_append(to_be_notified, belle_sip_object_ref(s));
			} /*else already in to_be_notified by Step 2*/

			s->revents |= BELLE_SIP_EVENT_TIMEOUT;
			it = bctbx_iterator_get_next(it);
		}
	}
	bctbx_iterator_delete(it);
	bctbx_iterator_delete(end);
	bctbx_mutex_unlock(&ml->sources_mutex);

	/* Step 4: notify those to be notified */
	for (elem = to_be_notified; elem != NULL;) {
		belle_sip_source_t *s = (belle_sip_source_t *)elem->data;
		next = elem->next;
		if (!s->cancelled) {

			if (belle_sip_log_level_enabled(BELLE_SIP_LOG_DEBUG)) {
				/*to avoid too many traces*/
				char *objdesc = belle_sip_object_to_string((belle_sip_object_t *)s);
				belle_sip_debug("source %s notified revents=%u, timeout=%i", objdesc, s->revents, (int)s->timeout);
				belle_sip_free(objdesc);
			}

			ret = s->notify(s->data, s->revents);
			if (ret == BELLE_SIP_STOP || s->oneshot) {
				/*this source needs to be removed*/
				belle_sip_main_loop_remove_source(ml, s);
			} else {
				bctbx_mutex_lock(&ml->sources_mutex);
				if (s->expired && s->it) {
					bctbx_map_erase(ml->timer_sources, s->it);
					bctbx_iterator_delete(s->it);
					s->it = NULL;
					belle_sip_object_unref(s);
				}
				if (!s->it && s->timeout >= 0) {
					/*timeout needs to be started again */
					if (ret == BELLE_SIP_CONTINUE_WITHOUT_CATCHUP) {
						s->expire_ms = cur + s->timeout;
					} else {
						s->expire_ms += s->timeout;
					}
					s->expired = FALSE;
					s->it = bctbx_map_insert_and_delete_with_returned_it(
					    ml->timer_sources, (bctbx_pair_t *)bctbx_pair_ullong_new(s->expire_ms, s));
					belle_sip_object_ref(s);
				}
				bctbx_mutex_unlock(&ml->sources_mutex);
			}
		} else {
			belle_sip_main_loop_remove_source(ml, s);
		}
		s->revents = 0;
		belle_sip_object_unref(s);
		belle_sip_free(elem); /*free just the element*/
		elem = next;
	}

	if (can_clean) belle_sip_object_pool_clean(ml->pool);
	else if (tmp_pool) {
		belle_sip_object_unref(tmp_pool);
		tmp_pool = NULL;
	}

end:
	belle_sip_free(pfd);
}

void belle_sip_main_loop_run(belle_sip_main_loop_t *ml) {
#ifndef _WIN32
	ml->thread_id = bctbx_thread_self();
#endif
	if (ml->in_loop) {
		belle_sip_warning("belle_sip_main_loop_run(): reentrancy detected, doing nothing");
		return;
	}
	ml->run = TRUE;
	ml->in_loop = TRUE;
	while (ml->run) {
		belle_sip_main_loop_iterate(ml);
	}
	ml->in_loop = FALSE;
}

int belle_sip_main_loop_quit(belle_sip_main_loop_t *ml) {
	ml->run = 0;
	return BELLE_SIP_STOP;
}

void belle_sip_main_loop_sleep(belle_sip_main_loop_t *ml, int milliseconds) {
	belle_sip_source_t *s = belle_sip_main_loop_create_timeout(ml, (belle_sip_source_func_t)belle_sip_main_loop_quit,
	                                                           ml, milliseconds, "Main loop sleep timer");

	belle_sip_main_loop_run(ml);
	belle_sip_main_loop_remove_source(ml, s);
	belle_sip_object_unref(s);
}

void belle_sip_main_loop_wake_up(belle_sip_main_loop_t *ml) {

#ifndef _WIN32
	if (write(ml->control_fds[1], "wake up!", 1) == -1) {
		belle_sip_fatal("Cannot write to control pipe of main loop thread: %s", strerror(errno));
	}
#else
	belle_sip_error("belle_sip_main_loop_wake_up() is not implemented for Windows.");
#endif
}
