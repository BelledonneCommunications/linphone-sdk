/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2014  Belledonne Communications, Grenoble, France

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "common.h"

static PayloadType* create_custom_payload_type(const char *type, const char *subtype, const char *rate, const char *channels, int number){
	PayloadType *pt=payload_type_new();
	if (strcasecmp(type,"audio")==0){
		pt->type=PAYLOAD_AUDIO_PACKETIZED;
	}else if (strcasecmp(type,"video")==0){
		pt->type=PAYLOAD_VIDEO;
	}else{
		fprintf(stderr,"Unsupported payload type should be audio or video, not %s\n",type);
		exit(-1);
	}
	pt->mime_type=ms_strdup(subtype);
	pt->clock_rate=atoi(rate);
	pt->channels=atoi(channels);
	return pt;
}

PayloadType* ms_tools_parse_custom_payload(const char *name){
	char type[64]={0};
	char subtype[64]={0};
	char clockrate[64]={0};
	char nchannels[64];
	char *separator;

	if (strlen(name)>=sizeof(clockrate)-1){
		fprintf(stderr,"Cannot parse %s: too long.\n",name);
		exit(-1);
	}

	separator=strchr(name,'/');
	if (separator){
		char *separator2;

		strncpy(type,name,separator-name);
		separator2=strchr(separator+1,'/');
		if (separator2){
			char *separator3;

			strncpy(subtype,separator+1,separator2-separator-1);
			separator3=strchr(separator2+1,'/');
			if (separator3){
				strncpy(clockrate,separator2+1,separator3-separator2-1);
				strcpy(nchannels,separator3+1);
			} else {
				nchannels[0]='1';
				nchannels[1]='\0';
				strcpy(clockrate,separator2+1);
			}
			fprintf(stdout,"Found custom payload type=%s, mime=%s, clockrate=%s nchannels=%s\n", type, subtype, clockrate, nchannels);
			return create_custom_payload_type(type,subtype,clockrate,nchannels,114);
		}
	}
	fprintf(stderr,"Error parsing payload name %s.\n",name);
	exit(-1);
}
