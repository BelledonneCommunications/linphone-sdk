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

#include "belle_sip_internal.h"


static void belle_sip_stack_destroy(belle_sip_stack_t *stack){
	belle_sip_object_unref(stack->ml);
	belle_sip_list_for_each (stack->lp,belle_sip_object_unref);
	belle_sip_list_free(stack->lp);
}

belle_sip_stack_t * belle_sip_stack_new(const char *properties){
	belle_sip_stack_t *stack=belle_sip_object_new(belle_sip_stack_t,belle_sip_stack_destroy);
	stack->ml=belle_sip_main_loop_new ();
	return stack;
}

belle_sip_listening_point_t *belle_sip_stack_create_listening_point(belle_sip_stack_t *s, const char *ipaddress, int port, const char *transport){
	belle_sip_listening_point_t *lp=NULL;
	if (strcasecmp(transport,"UDP")==0){
		lp=belle_sip_udp_listening_point_new (s,ipaddress,port);
	}else{
		belle_sip_fatal("Unsupported transport %s",transport);
	}
	if (lp!=NULL){
		s->lp=belle_sip_list_append(s->lp,lp);
	}
	return lp;
}

void belle_sip_stack_delete_listening_point(belle_sip_stack_t *s, belle_sip_listening_point_t *lp){
	s->lp=belle_sip_list_remove(s->lp,lp);
	belle_sip_object_unref(lp);
}

belle_sip_provider_t *belle_sip_stack_create_provider(belle_sip_stack_t *s, belle_sip_listening_point_t *lp){
	belle_sip_provider_t *p=belle_sip_provider_new(s,lp);
	return p;
}

void belle_sip_stack_delete_provider(belle_sip_stack_t *s, belle_sip_provider_t *p){
	belle_sip_object_unref(p);
}

void belle_sip_stack_main(belle_sip_stack_t *stack){
	belle_sip_main_loop_run(stack->ml);
}

void belle_sip_stack_sleep(belle_sip_stack_t *stack, unsigned int milliseconds){
	belle_sip_main_loop_sleep (stack->ml,milliseconds);
}

void belle_sip_stack_get_next_hop(belle_sip_stack_t *stack, belle_sip_request_t *req, belle_sip_hop_t *hop){
	hop->transport="UDP";
	/*should find top most route or request uri */
}
