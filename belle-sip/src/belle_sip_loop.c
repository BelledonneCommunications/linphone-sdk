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

#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"

struct belle_sip_source{
	belle_sip_list_t node;
	int fd;
	int revents;
	int timeout;
	void *data;
	belle_sip_source_func_t notify;
	void (*on_remove)(belle_sip_source_t *);
};

void belle_sip_source_destroy(belle_sip_source_t *obj){
	belle_sip_free(obj);
}

belle_sip_source_t * belle_sip_timeout_source_new(belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms){
	belle_sip_source_t *s=belle_sip_new0(belle_sip_source_t);
	s->fd=-1;
	s->timeout=timeout_value_ms;
	s->data=data;
	s->notify=func;
	return s;
}


struct belle_sip_main_loop{
	belle_sip_source_t *sources;
};


belle_sip_main_loop_t *belle_sip_main_loop_new(void){
	belle_sip_main_loop_t*m=belle_sip_new0(belle_sip_main_loop_t);
	return m;
}

void belle_sip_main_loop_add_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source){
	if (source->node.next || source->node.prev){
		belle_sip_fatal("Source is already linked somewhere else.");
		return;
	}
	ml->sources=(belle_sip_source_t*)belle_sip_list_append_link((belle_sip_list_t*)ml->sources,(belle_sip_list_t*)source);
}

void belle_sip_main_loop_remove_source(belle_sip_main_loop_t *ml, belle_sip_source_t *source){
	ml->sources=(belle_sip_source_t*)belle_sip_list_remove_link((belle_sip_list_t*)ml->sources,(belle_sip_list_t*)source);
}

void belle_sip_main_loop_add_timeout(belle_sip_main_loop_t *ml, belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms){
	belle_sip_source_t * s=belle_sip_timeout_source_new(func,data,timeout_value_ms);
	s->on_remove=belle_sip_source_destroy;
	belle_sip_main_loop_add_source(ml,s);
}
