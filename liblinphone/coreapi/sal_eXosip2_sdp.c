/*
linphone
Copyright (C) 2010  Simon MORLAT (simon.morlat@free.fr)

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "ortp/b64.h"
#include "sal.h"
#include <eXosip2/eXosip.h>

#define keywordcmp(key,b) strncmp(key,b,sizeof(key))

#ifdef FOR_LATER

static char *make_relay_session_id(const char *username, const char *relay){
	/*ideally this should be a hash of the parameters with a random part*/
	char tmp[128];
	int s1=(int)random();
	int s2=(int)random();
	long long int res=((long long int)s1)<<32 | (long long int) s2;
	void *src=&res;
	b64_encode(src, sizeof(long long int), tmp, sizeof(tmp));
	return osip_strdup(tmp);
}


static void add_relay_info(sdp_message_t *sdp, int mline, const char *relay, const char *relay_session_id){

	if (relay) sdp_message_a_attribute_add(sdp, mline,
				     osip_strdup ("relay-addr"),osip_strdup(relay));
	if (relay_session_id) sdp_message_a_attribute_add(sdp, mline,
				     osip_strdup ("relay-session-id"), osip_strdup(relay_session_id));
}

#endif

static char * int_2char(int a){
	char *p=osip_malloc(16);
	snprintf(p,16,"%i",a);
	return p;
}

/* return the value of attr "field" for payload pt at line pos (field=rtpmap,fmtp...)*/
static const char *sdp_message_a_attr_value_get_with_pt(sdp_message_t *sdp,int pos,int pt,const char *field)
{
	int i,tmppt=0,scanned=0;
	char *tmp;
	sdp_attribute_t *attr;
	for (i=0;(attr=sdp_message_attribute_get(sdp,pos,i))!=NULL;i++){
		if (keywordcmp(field,attr->a_att_field)==0 && attr->a_att_value!=NULL){
			int nb = sscanf(attr->a_att_value,"%i %n",&tmppt,&scanned);
			/* the return value may depend on how %n is interpreted by the libc: see manpage*/
			if (nb == 1 || nb==2 ){
				if (pt==tmppt){
					tmp=attr->a_att_value+scanned;
					if (strlen(tmp)>0)
						return tmp;
				}
			}else ms_warning("sdp has a strange a= line (%s) nb=%i",attr->a_att_value,nb);
		}
	}
	return NULL;
}

#ifdef FOR_LATER
/* return the value of attr "field" */
static const char *sdp_message_a_attr_value_get(sdp_message_t *sdp,int pos,const char *field)
{
	int i;
	sdp_attribute_t *attr;
	for (i=0;(attr=sdp_message_attribute_get(sdp,pos,i))!=NULL;i++){
		if (keywordcmp(field,attr->a_att_field)==0 && attr->a_att_value!=NULL){
			return attr->a_att_value;
		}
	}
	return NULL;
}
#endif

static int _sdp_message_get_a_ptime(sdp_message_t *sdp, int mline){
	int i,ret;
	sdp_attribute_t *attr;
	for (i=0;(attr=sdp_message_attribute_get(sdp,mline,i))!=NULL;i++){
		if (keywordcmp("ptime",attr->a_att_field)==0){
			int nb = sscanf(attr->a_att_value,"%i",&ret);
			/* the return value may depend on how %n is interpreted by the libc: see manpage*/
			if (nb == 1){
				return ret;
			}else ms_warning("sdp has a strange a=ptime line (%s) ",attr->a_att_value);
		}
	}
	return 0;
}

static sdp_message_t *create_generic_sdp(const SalMediaDescription *desc)
{
	sdp_message_t *local;
	int inet6;

	sdp_message_init (&local);
	if (strchr(desc->addr,':')!=NULL){
		inet6=1;
	}else inet6=0;
	sdp_message_v_version_set (local, osip_strdup ("0"));
	sdp_message_o_origin_set (local, osip_strdup (desc->username),
			  osip_strdup ("123456"), osip_strdup ("654321"),
			  osip_strdup ("IN"), inet6 ? osip_strdup("IP6") : osip_strdup ("IP4"),
			  osip_strdup (desc->addr));
	sdp_message_s_name_set (local, osip_strdup ("A conversation"));
	sdp_message_c_connection_add (local, -1,
			      osip_strdup ("IN"), inet6 ? osip_strdup ("IP6") : osip_strdup ("IP4"),
			      osip_strdup (desc->addr), NULL, NULL);
	sdp_message_t_time_descr_add (local, osip_strdup ("0"), osip_strdup ("0"));
	return local;
}



static void add_payload(sdp_message_t *msg, int line, const PayloadType *pt)
{
	char attr[256];
	sdp_message_m_payload_add (msg,line, int_2char (payload_type_get_number(pt)));
	if (pt->type==PAYLOAD_AUDIO_CONTINUOUS || pt->type==PAYLOAD_AUDIO_PACKETIZED)
		snprintf (attr,sizeof(attr),"%i %s/%i/%i", payload_type_get_number(pt), 
		    	pt->mime_type, pt->clock_rate,pt->channels);
	else
		snprintf (attr,sizeof(attr),"%i %s/%i", payload_type_get_number(pt), 
		    	pt->mime_type, pt->clock_rate);
	sdp_message_a_attribute_add (msg, line,
				     osip_strdup ("rtpmap"), osip_strdup(attr));

	if (pt->recv_fmtp != NULL)
	{
		snprintf (attr,sizeof(attr),"%i %s", payload_type_get_number(pt),pt->recv_fmtp);
		sdp_message_a_attribute_add (msg, line, osip_strdup ("fmtp"),
				     osip_strdup(attr));
	}
}


static void add_line(sdp_message_t *msg, int lineno, const SalStreamDescription *desc){
	const char *mt=desc->type==SalAudio ? "audio" : "video";
	const MSList *elem;
	const char *addr;
	int port;
	if (desc->candidates[0].addr[0]!='\0'){
		addr=desc->candidates[0].addr;
		port=desc->candidates[0].port;
	}else{
		addr=desc->addr;
		port=desc->port;
	}
	/*only add a c= line within the stream description if address are differents*/
	if (strcmp(addr,sdp_message_c_addr_get(msg, -1, 0))!=0){
		bool_t inet6;
		if (strchr(addr,':')!=NULL){
			inet6=TRUE;
		}else inet6=FALSE;
		sdp_message_c_connection_add (msg, lineno,
			      osip_strdup ("IN"), inet6 ? osip_strdup ("IP6") : osip_strdup ("IP4"),
			      osip_strdup (addr), NULL, NULL);
	}
	sdp_message_m_media_add (msg, osip_strdup (mt),
				 int_2char (port), NULL,
				 osip_strdup ("RTP/AVP"));
	if (desc->bandwidth>0) sdp_message_b_bandwidth_add (msg, lineno, osip_strdup ("AS"),
				     int_2char(desc->bandwidth));
	for(elem=desc->payloads;elem!=NULL;elem=elem->next){
		add_payload(msg, lineno, (PayloadType*)elem->data);
	}
}

sdp_message_t *media_description_to_sdp(const SalMediaDescription *desc){
	int i;
	sdp_message_t *msg=create_generic_sdp(desc);
	for(i=0;i<desc->nstreams;++i){
		add_line(msg,i,&desc->streams[i]);
	}
	return msg;
}

static int payload_type_fill_from_rtpmap(PayloadType *pt, const char *rtpmap){
	if (rtpmap==NULL){
		PayloadType *refpt=rtp_profile_get_payload(&av_profile,payload_type_get_number(pt));
		if (refpt){
			pt->mime_type=ms_strdup(refpt->mime_type);
			pt->clock_rate=refpt->clock_rate;
		}else{
			ms_error("payload number %i has no rtpmap and is unknown in AV Profile, ignored.",
			    payload_type_get_number(pt));
			return -1;
		}
	}else{
		char *mime=ms_strdup(rtpmap);
		char *p=strchr(mime,'/');
		if (p){
			char *chans;
			*p='\0';
			p++;
			chans=strchr(p,'/');
			if (chans){
				*chans='\0';
				chans++;
				pt->channels=atoi(chans);
			}else pt->channels=1;
			pt->clock_rate=atoi(p);
		}
		pt->mime_type=mime;
	}
	return 0;
}

int sdp_to_media_description(sdp_message_t *msg, SalMediaDescription *desc){
	int i,j;
	const char *mtype,*proto,*port,*addr,*number;
	sdp_bandwidth_t *sbw=NULL;
	
	addr=sdp_message_c_addr_get (msg, -1, 0);
	if (addr)
		strncpy(desc->addr,addr,sizeof(desc->addr));
	/* for each m= line */
	for (i=0; !sdp_message_endof_media (msg, i) && i<SAL_MEDIA_DESCRIPTION_MAX_STREAMS; i++)
	{
		SalStreamDescription *stream=&desc->streams[i];
		
		memset(stream,0,sizeof(*stream));
		mtype = sdp_message_m_media_get(msg, i);
		proto = sdp_message_m_proto_get (msg, i);
		port = sdp_message_m_port_get(msg, i);
		stream->proto=SalProtoUnknown;
		if (proto){
			if (strcasecmp(proto,"RTP/AVP")==0)
				stream->proto=SalProtoRtpAvp;
			else if (strcasecmp(proto,"RTP/SAVP")==0){
				stream->proto=SalProtoRtpSavp;
			}
		}
		addr = sdp_message_c_addr_get (msg, i, 0);
		if (addr != NULL)
			strncpy(stream->addr,addr,sizeof(stream->addr));
		stream->ptime=_sdp_message_get_a_ptime(msg,i);
		if (strcasecmp("audio", mtype) == 0){
			stream->type=SalAudio;
		}else if (strcasecmp("video", mtype) == 0){
			stream->type=SalVideo;
		}else stream->type=SalOther;
		for(j=0;(sbw=sdp_message_bandwidth_get(msg,i,j))!=NULL;++j){
			if (strcasecmp(sbw->b_bwtype,"AS")==0) stream->bandwidth=atoi(sbw->b_bandwidth);
		}
		/* for each payload type */
		for (j=0;((number=sdp_message_m_payload_get (msg, i,j)) != NULL); j++){
			const char *rtpmap,*fmtp;
			int ptn=atoi(number);
			PayloadType *pt=payload_type_new();
			payload_type_set_number(pt,ptn);
			/* get the rtpmap associated to this codec, if any */
			rtpmap=sdp_message_a_attr_value_get_with_pt(msg, i,ptn,"rtpmap");
			payload_type_fill_from_rtpmap(pt,rtpmap);
			/* get the fmtp, if any */
			fmtp=sdp_message_a_attr_value_get_with_pt(msg, i, ptn,"fmtp");
			payload_type_set_send_fmtp(pt,fmtp);
		}
	}
	return 0;
}
