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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "belle-sip/belle-sip.h"

int main(int argc, char *argv[]){
	char *str;
	struct stat st;
	int fd;
	int i;
	
	if (argc!=2){
		fprintf(stderr,"Usage:\n%s <text file containing messages>\n",argv[0]);
		return -1;
	}
	if (stat(argv[1],&st)==-1){
		fprintf(stderr,"Could not stat %s: %s\n",argv[1],strerror(errno));
		return -1;
	}
	
	fd=open(argv[1],O_RDONLY);
	if (fd==-1){
		fprintf(stderr,"Could not open %s: %s\n",argv[1],strerror(errno));
		return -1;
	}
	str=belle_sip_malloc0(st.st_size+1);
	if (read(fd,str,st.st_size)==-1){
		fprintf(stderr,"Could not open %s: %s\n",argv[1],strerror(errno));
		belle_sip_free(str);
		close(fd);
		return -1;
	}
	close(fd);
	
	for (i=0;i<st.st_size;){
		size_t read;
		belle_sip_message_t *msg=belle_sip_message_parse_raw(str+i,st.st_size-i,&read);
		if (msg){
			printf("Succesfully parsed message of %i bytes.",(int)read);
		}else{
			fprintf(stderr,"Failed to parse message.");
			break;
		}
		i+=read;
	}
	belle_sip_free(str);
	return 0;
}
