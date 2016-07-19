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


#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <unistd.h>
#endif
#include <string.h>

#include "belle-sip/belle-sip.h"
#include "belle-sip/belle-sdp.h"
#include <bctoolbox/vfs.h>

int main(int argc, char *argv[]){
	char *str;
	struct stat st;
	int fd;
	int i;
	const char *filename=NULL;
	const char *protocol="sip";

	if (argc<2){
		fprintf(stderr,"Usage:\n%s [--protocol sip|http|sdp] <text file containing messages>\n",argv[0]);
		return -1;
	}
	for(i=1;i<argc;++i){
		if (strcmp(argv[i],"--protocol")==0){
			i++;
			if (i<argc){
				protocol=argv[i];
			}else{
				fprintf(stderr,"Missing argument for --protocol\n");
				return -1;
			}
		}else filename=argv[i];
	}
	if (!filename){
		fprintf(stderr,"No filename specified\n");
				return -1;
	}
	if (stat(filename,&st)==-1){
		fprintf(stderr,"Could not stat %s: %s\n",filename,strerror(errno));
		return -1;
	}

	fd=open(filename,O_RDONLY);
	if (fd==-1){
		fprintf(stderr,"Could not open %s: %s\n",filename,strerror(errno));
		return -1;
	}
	str=belle_sip_malloc0(st.st_size+1);
	if (read(fd,str,st.st_size)==-1){
		fprintf(stderr,"Could not read %s: %s\n",filename,strerror(errno));
		belle_sip_free(str);
		close(fd);
		return -1;
	}
	close(fd);
	belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);

	for (i=0;i<st.st_size;){
		size_t read;
		if (strcasecmp(protocol,"sip")==0 || strcasecmp(protocol,"http")==0){
			belle_sip_message_t *msg;
			uint64_t begin,end;
			begin=belle_sip_time_ms();
			msg=belle_sip_message_parse_raw(str+i,st.st_size-i,&read);
			end=belle_sip_time_ms();
			if (msg){
				printf("Successfully parsed %s message of %i bytes in %i ms.\n",protocol,(int)read, (int)(end-begin));
			}else{
				fprintf(stderr,"Failed to parse message.\n");
				break;
			}
			i+=read;
		}else if (strcasecmp(protocol,"sdp")==0){
			belle_sdp_session_description_t *sdp=belle_sdp_session_description_parse(str);
			if (sdp){
				printf("Successfully parsed %s message of %i bytes.\n",protocol,(int)strlen(str));
			}else{
				fprintf(stderr,"Failed to parse SDP message.\n");
			}
			break;
		}
	}
	belle_sip_free(str);
	return 0;
}
