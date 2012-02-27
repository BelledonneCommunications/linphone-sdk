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

#include <stdio.h>
#include "belle-sip/belle-sip.h"

int main(int argc, char *argv[]){
	belle_sip_stack_t * stack=belle_sip_stack_new(NULL);
	belle_sip_listening_point_t *lp;
	belle_sip_provider_t *prov;
	belle_sip_request_t *req;

	belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
	
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"UDP");
	prov=belle_sip_stack_create_provider(stack,lp);
	req=belle_sip_request_create(
	                    belle_sip_uri_parse("sip:test.linphone.org"),
	                    "REGISTER",
	                    belle_sip_provider_create_call_id(prov),
	                    belle_sip_header_cseq_create(20,"REGISTER"),
	                    belle_sip_header_from_create("Tester <sip:tester@test.linphone.org>","a0dke45"),
	                    belle_sip_header_to_create("Tester <sip:tester@test.linphone.org>",NULL),
	                    belle_sip_header_via_new(),
	                    70);
	char *tmp=belle_sip_object_to_string(BELLE_SIP_OBJECT(req));

	
	printf("Message to send:\n%s\n",tmp);
	belle_sip_free(tmp);
	belle_sip_provider_send_request(prov,req);
	belle_sip_stack_sleep(stack,5000);
	printf("Exiting\n");
	belle_sip_object_unref(BELLE_SIP_OBJECT(prov));
	belle_sip_object_unref(BELLE_SIP_OBJECT(stack));
	return 0;
}
