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


#ifndef belle_sip_stack_h
#define belle_sip_stack_h

struct belle_sip_hop{
	char *host;
	char *transport;
	int port;
};

typedef struct belle_sip_hop belle_sip_hop_t;

struct belle_sip_timer_config{
	int T1;
	int T2;
	int T3;
	int T4;
};

typedef struct belle_sip_timer_config belle_sip_timer_config_t;

BELLE_SIP_BEGIN_DECLS

belle_sip_stack_t * belle_sip_stack_new(const char *properties);

belle_sip_listening_point_t *belle_sip_stack_create_listening_point(belle_sip_stack_t *s, const char *ipaddress, int port, const char *transport);

void belle_sip_stack_delete_listening_point(belle_sip_stack_t *s, belle_sip_listening_point_t *lp);

belle_sip_provider_t *belle_sip_stack_create_provider(belle_sip_stack_t *s, belle_sip_listening_point_t *lp);

belle_sip_main_loop_t* belle_sip_stack_get_main_loop(belle_sip_stack_t *stack);

void belle_sip_stack_main(belle_sip_stack_t *stack);

void belle_sip_stack_sleep(belle_sip_stack_t *stack, unsigned int milliseconds);

void belle_sip_hop_free(belle_sip_hop_t *hop);

BELLE_SIP_END_DECLS

#endif

