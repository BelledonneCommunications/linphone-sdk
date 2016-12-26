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

static void usage(const char *argv0){
	fprintf(stderr,"Usage:\n%s <http uri> [--ca-path <path> ] [--debug] [--no-tls-check]\n",argv0);
	exit(-1);
}

int main(int argc, char *argv[]){
	belle_http_provider_t *prov;
	belle_http_request_t *req;
	belle_generic_uri_t *uri;
	belle_http_request_listener_callbacks_t cbs={0};
	const char *ca_path = NULL;
	belle_tls_crypto_config_t *cfg;
	int i;
	int check_tls = 1;
	
	if (argc<2){
		usage(argv[0]);
	}
	for (i = 2; i < argc; ++i){
		if (strcmp(argv[i], "--ca-path") == 0){
			i++;
			ca_path = argv[i];
		}else if (strcmp(argv[i], "--debug")==0){
			belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
		}else if (strcmp(argv[i], "--no-tls-check")==0){
			check_tls = 0;
		}else{
			usage(argv[0]);
		}
	}
	stack=belle_sip_stack_new(NULL);
	prov=belle_sip_stack_create_http_provider(stack,"::0");
	cfg = belle_tls_crypto_config_new();
	if (ca_path){
		
		belle_tls_crypto_config_set_root_ca(cfg, ca_path);
	}
	if (!check_tls) belle_tls_crypto_config_set_verify_exceptions(cfg, BELLE_TLS_VERIFY_ANY_REASON);
	belle_http_provider_set_tls_crypto_config(prov, cfg);
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
