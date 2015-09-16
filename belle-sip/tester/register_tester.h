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


extern belle_sip_stack_t * stack;
extern belle_sip_provider_t *prov;
extern const char *test_domain;
int call_endeed;
extern int register_before_all(void);
extern int register_after_all(void);
extern belle_sip_request_t* register_user(belle_sip_stack_t * stack
		,belle_sip_provider_t *prov
		,const char *transport
		,int use_transaction
		,const char* username,const char* outbound) ;
extern void unregister_user(belle_sip_stack_t * stack
					,belle_sip_provider_t *prov
					,belle_sip_request_t* initial_request
					,int use_transaction);

