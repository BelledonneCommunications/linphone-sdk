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

#define BELLE_SIP_EVENT_READ 1
#define BELLE_SIP_EVENT_WRITE (1<<1)
#define BELLE_SIP_EVENT_ERROR (1<<2)

typedef struct belle_sip_source belle_sip_source_t;

typedef int (*belle_sip_source_func_t)(void *user_data, unsigned int events);

belle_sip_source_t * belle_sip_timeout_source_new(belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms);

typedef struct belle_sip_main_loop belle_sip_main_loop_t;

belle_sip_main_loop_t *belle_sip_main_loop_new(void);

void belle_sip_main_loop_add_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source);

void belle_sip_main_loop_remove_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source);

void belle_sip_main_loop_add_timeout(belle_sip_main_loop_t *ml, belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms);

#endif
