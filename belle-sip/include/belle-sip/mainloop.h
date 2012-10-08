/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
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
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>



#define BELLE_SIP_EVENT_READ 1
#define BELLE_SIP_EVENT_WRITE (1<<1)
#define BELLE_SIP_EVENT_ERROR (1<<2)
#define BELLE_SIP_EVENT_TIMEOUT (1<<3)

typedef struct belle_sip_source belle_sip_source_t;

int belle_sip_source_set_events(belle_sip_source_t* source, int event_mask);
belle_sip_fd_t belle_sip_source_get_fd(const belle_sip_source_t* source);

/**
 * Callback function prototype for main loop notifications.
 * Return value is important:
 * 0 => source is removed from main loop.
 * non zero value => source is kept.
**/
typedef int (*belle_sip_source_func_t)(void *user_data, unsigned int events);

typedef struct belle_sip_main_loop belle_sip_main_loop_t;

#define BELLE_SIP_CONTINUE	TRUE
#define BELLE_SIP_STOP		FALSE

BELLE_SIP_BEGIN_DECLS

void belle_sip_main_loop_add_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source);

void belle_sip_main_loop_remove_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source);

/**
 * Creates a mainloop.
**/
belle_sip_main_loop_t *belle_sip_main_loop_new(void);

/**
 * Adds a timeout into the main loop
 * @param ml
 * @param func a callback function to be called to notify timeout expiration
 * @param data a pointer to be passed to the callback
 * @param timeout_value_ms duration of the timeout.
 * @returns timeout id
**/
unsigned long belle_sip_main_loop_add_timeout(belle_sip_main_loop_t *ml, belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms);


/**
 * Creates a timeout source, similarly to belle_sip_main_loop_add_timeout().
 * However in this case the timeout must be entered manually using belle_sip_main_loop_add_source().
 * Its pointer can be used to remove it from the source (that is cancelling it).
**/
belle_sip_source_t * belle_sip_timeout_source_new(belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms);

void belle_sip_source_set_timeout(belle_sip_source_t *s, unsigned int value_ms);

unsigned int belle_sip_source_get_timeout(const belle_sip_source_t *s);

belle_sip_source_t * belle_sip_fd_source_new(belle_sip_source_func_t func, void *data, int fd, unsigned int events, unsigned int timeout_value_ms);

unsigned long belle_sip_source_get_id(belle_sip_source_t *s);

/**
 * Executes the main loop forever (or until belle_sip_main_loop_quit() is called)
**/
void belle_sip_main_loop_run(belle_sip_main_loop_t *ml);

/**
 * Executes the main loop for the time specified in milliseconds.
**/
void belle_sip_main_loop_sleep(belle_sip_main_loop_t *ml, int milliseconds);

/**
 * Break out the main loop.
**/
int belle_sip_main_loop_quit(belle_sip_main_loop_t *ml);

/**
 * Cancel (removes) a source. It is not freed.
**/
void belle_sip_main_loop_cancel_source(belle_sip_main_loop_t *ml, unsigned long id);

BELLE_SIP_END_DECLS

#endif
