/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2013  Belledonne Communications SARL

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


#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "belle-sip/belle-sip.h"

static belle_sip_stack_t *stack=NULL;

static void process_response(void *data, const belle_http_response_event_t *event){
	belle_http_response_t *resp=event->response;
	const char *body=belle_sip_message_get_body(BELLE_SIP_MESSAGE(resp));
	if (body){
		fprintf(stdout,"%s",body);
	}
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
}

static void process_io_error(void *data, const belle_sip_io_error_event_t *event){
	fprintf(stderr,"IO error\n");
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
}

static void process_timeout(void *data, const belle_sip_timeout_event_t *event){
	fprintf(stderr,"Timeout\n");
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
}

int main(int argc, char *argv[]){
	belle_http_provider_t *prov;
	belle_http_request_t *req;
	belle_generic_uri_t *uri;
	belle_http_request_listener_callbacks_t cbs={0};
	
	if (argc<2){
		fprintf(stderr,"Usage:\n%s <http uri> [--debug]\n",argv[0]);
		return -1;
	}
	if (argc==3){
		if (strcmp(argv[2],"--debug")==0){
			belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
		}
	}
	stack=belle_sip_stack_new(NULL);
	prov=belle_sip_stack_create_http_provider(stack,"0.0.0.0");
	uri=belle_generic_uri_parse(argv[1]);
	if (!uri){
		fprintf(stderr,"Bad uri %s\n",argv[1]);
		return -1;
	}
	cbs.process_response=process_response;
	cbs.process_io_error=process_io_error;
	cbs.process_timeout=process_timeout;
	req=belle_http_request_create("GET",uri,NULL);
	belle_http_provider_send_request(prov,req,belle_http_request_listener_create_from_callbacks(&cbs,NULL));
	belle_sip_stack_main(stack);
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);
	return 0;
}
