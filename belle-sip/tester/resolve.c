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

static belle_sip_stack_t *stack;

static void resolver_callback(void *data, const char *queried_name, struct addrinfo *ai_list){
	int err;
	struct addrinfo *ai_it;
	char name[NI_MAXHOST];
	char port[NI_MAXSERV];
	
	for(ai_it=ai_list;ai_it!=NULL;ai_it=ai_it->ai_next){
		err=bctbx_getnameinfo(ai_it->ai_addr,ai_list->ai_addrlen,name,sizeof(name),port,sizeof(port),NI_NUMERICSERV|NI_NUMERICHOST);
		if (err!=0){
			fprintf(stderr,"getnameinfo error: %s",gai_strerror(err));
		}else{
			printf("\t%s %s\n",name,port);
		}
	}
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
}

int main(int argc, char *argv[]){
	int i;
	const char *domain=NULL;
	const char *transport="udp";
	
	if (argc<2){
		fprintf(stderr,"Usage:\n%s <domain name> [transport] [--debug]\n",argv[0]);
		return -1;
	}
	domain=argv[1];
	for (i=2;i<argc;i++){
		if (strcmp(argv[i],"--debug")==0){
			belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
		}else if (strstr(argv[i],"--")!=argv[i]) transport=argv[i];
	}
	stack=belle_sip_stack_new(NULL);
	printf("Trying to resolve domain '%s', with transport hint '%s'\n",domain,transport);
	belle_sip_stack_resolve(stack,"sip", transport,domain,5060,AF_INET6,resolver_callback,NULL);
	belle_sip_stack_main(stack);
	belle_sip_object_unref(stack);
	return 0;
}
