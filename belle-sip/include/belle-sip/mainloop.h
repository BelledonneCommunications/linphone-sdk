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

#ifndef BELLE_SIP_MAINLOOP_H
#define BELLE_SIP_MAINLOOP_H


#define BELLE_SIP_EVENT_READ 1
#define BELLE_SIP_EVENT_WRITE (1<<1)
#define BELLE_SIP_EVENT_ERROR (1<<2)
#define BELLE_SIP_EVENT_TIMEOUT (1<<3)

typedef struct belle_sip_source belle_sip_source_t;

BELLESIP_EXPORT int belle_sip_source_set_events(belle_sip_source_t* source, int event_mask);
BELLESIP_EXPORT belle_sip_socket_t belle_sip_source_get_socket(const belle_sip_source_t* source);

/**
 * Callback function prototype for main loop notifications.
 * Return value is important:
 * BELLE_SIP_STOP => source is removed from main loop.
 * BELLE_SIP_CONTINUE => source is kept, timeout is restarted if any according to last expiry time
 * BELLE_SIP_CONTINUE_WITHOUT_CATCHUP => source is kept, timeout is restarted if any according to current time
**/
typedef int (*belle_sip_source_func_t)(void *user_data, unsigned int events);

typedef void (*belle_sip_callback_t)(void *user_data);

typedef struct belle_sip_main_loop belle_sip_main_loop_t;

#define BELLE_SIP_CONTINUE_WITHOUT_CATCHUP 2
#define BELLE_SIP_CONTINUE	1
#define BELLE_SIP_STOP		0

BELLE_SIP_BEGIN_DECLS

BELLESIP_EXPORT void belle_sip_main_loop_add_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source);

BELLESIP_EXPORT void belle_sip_main_loop_remove_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source);

/**
 * Creates a mainloop.
**/
BELLESIP_EXPORT belle_sip_main_loop_t *belle_sip_main_loop_new(void);

/**
 * Adds a timeout into the main loop
 * @param ml
 * @param func a callback function to be called to notify timeout expiration
 * @param data a pointer to be passed to the callback
 * @param timeout_value_ms duration of the timeout.
 * @returns timeout id
**/
BELLESIP_EXPORT unsigned long belle_sip_main_loop_add_timeout(belle_sip_main_loop_t *ml, belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms);

/**
 * Adds a timeout into the main loop
 * The caller of this function is responsible for freeing (with belle_sip_object_unref()) the returned belle_sip_source_t object when it is no longer
 * needed.
 * @param ml
 * @param func a callback function to be called to notify timeout expiration
 * @param data a pointer to be passed to the callback
 * @param timeout_value_ms duration of the timeout.
 * @param timer_name name of the timer, can be null
 * @returns timeout belle_sip_source_t  with ref count = 1
**/
BELLESIP_EXPORT belle_sip_source_t* belle_sip_main_loop_create_timeout(belle_sip_main_loop_t *ml
							, belle_sip_source_func_t func
							, void *data
							, unsigned int timeout_value_ms
							,const char* timer_name);

/**
 * Schedule an arbitrary task at next main loop iteration.
**/
BELLESIP_EXPORT void belle_sip_main_loop_do_later(belle_sip_main_loop_t *ml, belle_sip_callback_t func, void *data);

/**
 * Creates a timeout source, similarly to belle_sip_main_loop_add_timeout().
 * However in this case the timeout must be entered manually using belle_sip_main_loop_add_source().
 * Its pointer can be used to remove it from the source (that is cancelling it).
**/
BELLESIP_EXPORT belle_sip_source_t * belle_sip_timeout_source_new(belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms);

BELLESIP_EXPORT void belle_sip_source_set_timeout(belle_sip_source_t *s, unsigned int value_ms);

BELLESIP_EXPORT unsigned int belle_sip_source_get_timeout(const belle_sip_source_t *s);

BELLESIP_EXPORT belle_sip_source_t * belle_sip_socket_source_new(belle_sip_source_func_t func, void *data, belle_sip_socket_t fd, unsigned int events, unsigned int timeout_value_ms);

BELLESIP_EXPORT unsigned long belle_sip_source_get_id(belle_sip_source_t *s);

BELLESIP_EXPORT belle_sip_source_t *belle_sip_main_loop_find_source(belle_sip_main_loop_t *ml, unsigned long id);

/**
 * Executes the main loop forever (or until belle_sip_main_loop_quit() is called)
**/
BELLESIP_EXPORT void belle_sip_main_loop_run(belle_sip_main_loop_t *ml);

/**
 * Executes the main loop for the time specified in milliseconds.
**/
BELLESIP_EXPORT void belle_sip_main_loop_sleep(belle_sip_main_loop_t *ml, int milliseconds);

/**
 * Break out the main loop.
**/
BELLESIP_EXPORT int belle_sip_main_loop_quit(belle_sip_main_loop_t *ml);

/**
 * Cancel (removes) a source. It is not freed.
**/
BELLESIP_EXPORT void belle_sip_main_loop_cancel_source(belle_sip_main_loop_t *ml, unsigned long id);

BELLE_SIP_END_DECLS
#if defined(__cplusplus) && defined(BELLE_SIP_USE_STL)
#include <functional>
/*purpose of this function is to simplify c++ timer integration.
* ex:
* std::string helloworld("Hello world):
* belle_sip_source_cpp_func_t *func = new belle_sip_source_cpp_func_t([helloworld](unsigned int events) {
*						std::cout << helloworld << std::endl;
*						return BELLE_SIP_STOP;
*					});
*create timer
*mTimer = belle_sip_main_loop_create_timeout( mainloop
*											,(belle_sip_source_func_t)belle_sip_source_cpp_func
*											, func
*											, 1000
*											,"timer for c++");
*
*/
typedef std::function<int (unsigned int)> belle_sip_source_cpp_func_t;
BELLESIP_EXPORT inline int belle_sip_source_cpp_func(belle_sip_source_cpp_func_t* user_data, unsigned int events)
{
	int result = (*user_data)(events);
	delete user_data;
	return result;
}
#endif

#endif
