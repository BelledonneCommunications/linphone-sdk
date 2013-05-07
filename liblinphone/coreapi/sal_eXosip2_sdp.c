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


#include "ortp/port.h"
#include "ortp/b64.h"
#include "ortp/ortp_srtp.h"
#include "sal.h"
#include <eXosip2/eXosip.h>

#define keywordcmp(key,b) strcmp(key,b)

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

static int _sdp_message_get_mline_dir(sdp_message_t *sdp, int mline){
	int i;
	sdp_attribute_t *attr;
	for (i=0;(attr=sdp_message_attribute_get(sdp,mline,i))!=NULL;i++){
		if (keywordcmp("sendrecv",attr->a_att_field)==0){
			return SalStreamSendRecv;
		}else if (keywordcmp("sendonly",attr->a_att_field)==0){
			return SalStreamSendOnly;
		}else if (keywordcmp("recvonly",attr->a_att_field)==0){
			return SalStreamRecvOnly;
		}else if (keywordcmp("inactive",attr->a_att_field)==0){
			return SalStreamInactive;
		}
	}
	return SalStreamSendRecv;
}

static sdp_message_t *create_generic_sdp(const SalMediaDescription *desc)
{
	sdp_message_t *local;
	int inet6;
	char sessid[16];
	char sessver[16];
	const char *rtp_addr = desc->addr;
	
	snprintf(sessid,16,"%i",desc->session_id);
	snprintf(sessver,16,"%i",desc->session_ver);
	sdp_message_init (&local);
	if (strchr(desc->addr,':')!=NULL){
		inet6=1;
	}else inet6=0;
	sdp_message_v_version_set (local, osip_strdup ("0"));
	sdp_message_o_origin_set (local, osip_strdup (desc->username),
			  osip_strdup (sessid), osip_strdup (sessver),
			  osip_strdup ("IN"), inet6 ? osip_strdup("IP6") : osip_strdup ("IP4"),
			  osip_strdup (desc->addr));
	sdp_message_s_name_set (local, osip_strdup ("Talk"));
	/* Do not set the c= line to 0.0.0.0 if there is an ICE session. */
	if((desc->ice_ufrag[0] != '\0') || !sal_media_description_has_dir (desc,SalStreamSendOnly))
	{
		sdp_message_c_connection_add (local, -1,
				osip_strdup ("IN"), inet6 ? osip_strdup ("IP6") : osip_strdup ("IP4"),
						osip_strdup (rtp_addr), NULL, NULL);
	}
	else
	{
		sdp_message_c_connection_add (local, -1,
				osip_strdup ("IN"), inet6 ? osip_strdup ("IP6") : osip_strdup ("IP4"),
						inet6 ? osip_strdup ("::0") : osip_strdup ("0.0.0.0"), NULL, NULL);
	}		
	sdp_message_t_time_descr_add (local, osip_strdup ("0"), osip_strdup ("0"));
	if (desc->bandwidth>0) sdp_message_b_bandwidth_add (local, -1, osip_strdup ("AS"),
			int_2char(desc->bandwidth));
	if (desc->ice_completed == TRUE) sdp_message_a_attribute_add(local, -1, osip_strdup("nortpproxy"), osip_strdup("yes"));
	if (desc->ice_pwd[0] != '\0') sdp_message_a_attribute_add(local, -1, osip_strdup("ice-pwd"), osip_strdup(desc->ice_pwd));
	if (desc->ice_ufrag[0] != '\0') sdp_message_a_attribute_add(local, -1, osip_strdup("ice-ufrag"), osip_strdup(desc->ice_ufrag));

	return local;
}


static bool_t is_known_rtpmap(const PayloadType *pt){
	switch(payload_type_get_number(pt)){
		case 0:
		case 8:
		case 3:
		case 34:
			return TRUE;
	}
	return FALSE;
}

static void add_payload(sdp_message_t *msg, int line, const PayloadType *pt, bool_t strip_well_known_rtpmaps)
{
	char attr[256];
	sdp_message_m_payload_add (msg,line, int_2char (payload_type_get_number(pt)));

	if (!strip_well_known_rtpmaps || !is_known_rtpmap(pt)){
		if (pt->channels>1)
			snprintf (attr,sizeof(attr),"%i %s/%i/%i", payload_type_get_number(pt), 
					pt->mime_type, pt->clock_rate,pt->channels);
		else
			snprintf (attr,sizeof(attr),"%i %s/%i", payload_type_get_number(pt), 
					pt->mime_type, pt->clock_rate);
		sdp_message_a_attribute_add (msg, line,
						 osip_strdup ("rtpmap"), osip_strdup(attr));
	}

	if (pt->recv_fmtp != NULL)
	{
		snprintf (attr,sizeof(attr),"%i %s", payload_type_get_number(pt),pt->recv_fmtp);
		sdp_message_a_attribute_add (msg, line, osip_strdup ("fmtp"),
				     osip_strdup(attr));
	}
}

static void add_ice_candidates(sdp_message_t *msg, int lineno, const SalStreamDescription *desc)
{
	char buffer[1024];
	const SalIceCandidate *candidate;
	int nb;
	int i;

	for (i = 0; i < SAL_MEDIA_DESCRIPTION_MAX_ICE_CANDIDATES; i++) {
		candidate = &desc->ice_candidates[i];
		if ((candidate->addr[0] == '\0') || (candidate->port == 0)) break;
		nb = snprintf(buffer, sizeof(buffer), "%s %u UDP %u %s %d typ %s",
			candidate->foundation, candidate->componentID, candidate->priority, candidate->addr, candidate->port, candidate->type);
		if (nb < 0) {
			ms_error("Cannot add ICE candidate attribute!");
			return;
		}
		if (candidate->raddr[0] != '\0') {
			nb = snprintf(buffer + nb, sizeof(buffer) - nb, " raddr %s rport %d", candidate->raddr, candidate->rport);
			if (nb < 0) {
				ms_error("Cannot add ICE candidate attribute!");
				return;
			}
		}
		sdp_message_a_attribute_add(msg, lineno, osip_strdup("candidate"), osip_strdup(buffer));
	}
}

static void add_ice_remote_candidates(sdp_message_t *msg, int lineno, const SalStreamDescription *desc)
{
	char buffer[1024];
	char *ptr = buffer;
	const SalIceRemoteCandidate *candidate;
	int offset = 0;
	int i;

	buffer[0] = '\0';
	for (i = 0; i < SAL_MEDIA_DESCRIPTION_MAX_ICE_REMOTE_CANDIDATES; i++) {
		candidate = &desc->ice_remote_candidates[i];
		if ((candidate->addr[0] != '\0') && (candidate->port != 0)) {
			offset = snprintf(ptr, buffer + sizeof(buffer) - ptr, "%s%d %s %d", (i > 0) ? " " : "", i + 1, candidate->addr, candidate->port);
			if (offset < 0) {
				ms_error("Cannot add ICE remote-candidates attribute!");
				return;
			}
			ptr += offset;
		}
	}
	if (buffer[0] != '\0') sdp_message_a_attribute_add(msg, lineno, osip_strdup("remote-candidates"), osip_strdup(buffer));
}

static void add_line(sdp_message_t *msg, int lineno, const SalStreamDescription *desc){
	const char *mt=NULL;
	const MSList *elem;
	const char *rtp_addr;
	const char *rtcp_addr;
	const char *dir="sendrecv";
	int rtp_port;
	int rtcp_port;
	bool_t strip_well_known_rtpmaps;
	bool_t different_rtp_and_rtcp_addr;
	
	switch (desc->type) {
	case SalAudio:
		mt="audio";
		break;
	case SalVideo:
		mt="video";
		break;
	case SalOther:
		mt=desc->typeother;
		break;
	}
	rtp_addr=desc->rtp_addr;
	rtcp_addr=desc->rtcp_addr;
	rtp_port=desc->rtp_port;
	rtcp_port=desc->rtcp_port;

	if (desc->proto == SalProtoRtpSavp) {
		int i;
		
		sdp_message_m_media_add (msg, osip_strdup (mt),
					 int_2char (rtp_port), NULL,
					 osip_strdup ("RTP/SAVP"));
       
		/* add crypto lines */
		for(i=0; i<SAL_CRYPTO_ALGO_MAX; i++) {
			char buffer[1024];
			switch (desc->crypto[i].algo) {
				case AES_128_SHA1_80:
					snprintf(buffer, 1024, "%d %s inline:%s",
						desc->crypto[i].tag, "AES_CM_128_HMAC_SHA1_80", desc->crypto[i].master_key);
					sdp_message_a_attribute_add(msg, lineno, osip_strdup("crypto"),
						osip_strdup(buffer));
					break;
				case AES_128_SHA1_32:
					snprintf(buffer, 1024, "%d %s inline:%s",
						desc->crypto[i].tag, "AES_CM_128_HMAC_SHA1_32", desc->crypto[i].master_key);
					sdp_message_a_attribute_add(msg, lineno, osip_strdup("crypto"),
						osip_strdup(buffer));
					break;
				case AES_128_NO_AUTH:
					ms_warning("Unsupported crypto suite: AES_128_NO_AUTH");
					break;
				case NO_CIPHER_SHA1_80:
					ms_warning("Unsupported crypto suite: NO_CIPHER_SHA1_80");
					break; 
				default:
					i = SAL_CRYPTO_ALGO_MAX;
			}
		}
		
	} else {
		sdp_message_m_media_add (msg, osip_strdup (mt),
					 int_2char (rtp_port), NULL,
					 osip_strdup ("RTP/AVP"));
		
	}

	/*only add a c= line within the stream description if address are differents*/
	if (rtp_addr[0]!='\0' && strcmp(rtp_addr,sdp_message_c_addr_get(msg, -1, 0))!=0){
		bool_t inet6;
		if (strchr(rtp_addr,':')!=NULL){
			inet6=TRUE;
		}else inet6=FALSE;
		sdp_message_c_connection_add (msg, lineno,
			      osip_strdup ("IN"), inet6 ? osip_strdup ("IP6") : osip_strdup ("IP4"),
			      osip_strdup (rtp_addr), NULL, NULL);
	}

	if (desc->bandwidth>0) sdp_message_b_bandwidth_add (msg, lineno, osip_strdup ("AS"),
				     int_2char(desc->bandwidth));
	if (desc->ptime>0) sdp_message_a_attribute_add(msg,lineno,osip_strdup("ptime"),
	    			int_2char(desc->ptime));
	strip_well_known_rtpmaps=ms_list_size(desc->payloads)>5;
	if (desc->payloads){
		for(elem=desc->payloads;elem!=NULL;elem=elem->next){
			add_payload(msg, lineno, (PayloadType*)elem->data,strip_well_known_rtpmaps);
		}
	}else{
		/* to comply with SDP we cannot have an empty payload type number list */
		/* as it happens only when mline is declined with a zero port, it does not matter to put whatever codec*/
		sdp_message_m_payload_add (msg,lineno, int_2char (0));
	}
	switch(desc->dir){
		case SalStreamSendRecv:
			/*dir="sendrecv";*/
			dir=NULL;
		break;
		case SalStreamRecvOnly:
			dir="recvonly";
			break;
		case SalStreamSendOnly:
			dir="sendonly";
			break;
		case SalStreamInactive:
			dir="inactive";
			break;
	}
	if (dir) sdp_message_a_attribute_add (msg, lineno, osip_strdup (dir),NULL);
	if (rtp_port != 0) {
		different_rtp_and_rtcp_addr = (rtcp_addr[0] != '\0') && (strcmp(rtp_addr, rtcp_addr) != 0);
		if ((rtcp_port != (rtp_port + 1)) || (different_rtp_and_rtcp_addr == TRUE)) {
			if (different_rtp_and_rtcp_addr == TRUE) {
				char buffer[1024];
				snprintf(buffer, sizeof(buffer), "%u IN IP4 %s", rtcp_port, rtcp_addr);
				sdp_message_a_attribute_add(msg, lineno, osip_strdup("rtcp"), osip_strdup(buffer));
			} else {
				sdp_message_a_attribute_add(msg, lineno, osip_strdup("rtcp"), int_2char(rtcp_port));
			}
		}
	}
	if (desc->ice_completed == TRUE) {
		sdp_message_a_attribute_add(msg, lineno, osip_strdup("nortpproxy"), osip_strdup("yes"));
	}
	if (desc->ice_mismatch == TRUE) {
		sdp_message_a_attribute_add(msg, lineno, osip_strdup("ice-mismatch"), NULL);
	} else {
		if (desc->rtp_port != 0) {
			if (desc->ice_pwd[0] != '\0') sdp_message_a_attribute_add(msg, lineno, osip_strdup("ice-pwd"), osip_strdup(desc->ice_pwd));
			if (desc->ice_ufrag[0] != '\0') sdp_message_a_attribute_add(msg, lineno, osip_strdup("ice-ufrag"), osip_strdup(desc->ice_ufrag));
			add_ice_candidates(msg, lineno, desc);
			add_ice_remote_candidates(msg, lineno, desc);
		}
	}
}


sdp_message_t *media_description_to_sdp(const SalMediaDescription *desc){
	int i;
	sdp_message_t *msg=create_generic_sdp(desc);
	for(i=0;i<desc->n_total_streams;++i){
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
	const char *mtype,*proto,*rtp_port,*rtp_addr,*number;
	const char *sess;
	sdp_bandwidth_t *sbw=NULL;
	sdp_attribute_t *attr;
	int nb_ice_candidates;

	/* Get session information. */
	sess = sdp_message_o_sess_id_get(msg);
	if (sess) desc->session_id = strtoul(sess, NULL, 10);
	sess = sdp_message_o_sess_version_get(msg);
	if (sess) desc->session_ver = strtoul(sess, NULL, 10);

	rtp_addr=sdp_message_c_addr_get (msg, -1, 0);
	if (rtp_addr)
		strncpy(desc->addr,rtp_addr,sizeof(desc->addr));
	for(j=0;(sbw=sdp_message_bandwidth_get(msg,-1,j))!=NULL;++j){
		if (strcasecmp(sbw->b_bwtype,"AS")==0) desc->bandwidth=atoi(sbw->b_bandwidth);
	}

	/* Get ICE remote ufrag and remote pwd, and ice_lite flag */
	for (i = 0; (i < SAL_MEDIA_DESCRIPTION_MAX_MESSAGE_ATTRIBUTES) && ((attr = sdp_message_attribute_get(msg, -1, i)) != NULL); i++) {
		if ((keywordcmp("ice-ufrag", attr->a_att_field) == 0) && (attr->a_att_value != NULL)) {
			strncpy(desc->ice_ufrag, attr->a_att_value, sizeof(desc->ice_ufrag));
		} else if ((keywordcmp("ice-pwd", attr->a_att_field) == 0) && (attr->a_att_value != NULL)) {
			strncpy(desc->ice_pwd, attr->a_att_value, sizeof(desc->ice_pwd));
		} else if (keywordcmp("ice-lite", attr->a_att_field) == 0) {
			desc->ice_lite = TRUE;
		}
	}

	desc->n_active_streams = 0;

	/* for each m= line */
	for (i=0; !sdp_message_endof_media (msg, i) && i<SAL_MEDIA_DESCRIPTION_MAX_STREAMS; i++)
	{
		SalStreamDescription *stream=&desc->streams[i];
		nb_ice_candidates = 0;
		
		memset(stream,0,sizeof(*stream));
		mtype = sdp_message_m_media_get(msg, i);
		proto = sdp_message_m_proto_get (msg, i);
		rtp_port = sdp_message_m_port_get(msg, i);
		stream->proto=SalProtoUnknown;
		if (proto){
			if (strcasecmp(proto,"RTP/AVP")==0)
				stream->proto=SalProtoRtpAvp;
			else if (strcasecmp(proto,"RTP/SAVP")==0){
				stream->proto=SalProtoRtpSavp;
			}
		}
		rtp_addr = sdp_message_c_addr_get (msg, i, 0);
		if (rtp_addr != NULL)
			strncpy(stream->rtp_addr,rtp_addr,sizeof(stream->rtp_addr));
		if (rtp_port)
			stream->rtp_port=atoi(rtp_port);
		if (stream->rtp_port > 0)
			desc->n_active_streams++;
		
		stream->ptime=_sdp_message_get_a_ptime(msg,i);
		if (strcasecmp("audio", mtype) == 0){
			stream->type=SalAudio;
		}else if (strcasecmp("video", mtype) == 0){
			stream->type=SalVideo;
		}else {
			stream->type=SalOther;
			strncpy(stream->typeother,mtype,sizeof(stream->typeother)-1);
		}
		for(j=0;(sbw=sdp_message_bandwidth_get(msg,i,j))!=NULL;++j){
			if (strcasecmp(sbw->b_bwtype,"AS")==0) stream->bandwidth=atoi(sbw->b_bandwidth);
		}
		stream->dir=_sdp_message_get_mline_dir(msg,i);
		/* for each payload type */
		for (j=0;((number=sdp_message_m_payload_get (msg, i,j)) != NULL); j++){
			const char *rtpmap,*fmtp;
			int ptn=atoi(number);
			PayloadType *pt=payload_type_new();
			payload_type_set_number(pt,ptn);
			/* get the rtpmap associated to this codec, if any */
			rtpmap=sdp_message_a_attr_value_get_with_pt(msg, i,ptn,"rtpmap");
			if (payload_type_fill_from_rtpmap(pt,rtpmap)==0){
				/* get the fmtp, if any */
				fmtp=sdp_message_a_attr_value_get_with_pt(msg, i, ptn,"fmtp");
				payload_type_set_send_fmtp(pt,fmtp);
				stream->payloads=ms_list_append(stream->payloads,pt);
				ms_message("Found payload %s/%i fmtp=%s",pt->mime_type,pt->clock_rate,
					pt->send_fmtp ? pt->send_fmtp : "");
			}
		}

		/* Get media specific RTCP attribute */
		stream->rtcp_port = stream->rtp_port + 1;
		snprintf(stream->rtcp_addr, sizeof(stream->rtcp_addr), "%s", stream->rtp_addr);
		for (j = 0; ((attr = sdp_message_attribute_get(msg, i, j)) != NULL); j++) {
			if ((keywordcmp("rtcp", attr->a_att_field) == 0) && (attr->a_att_value != NULL)) {
				char tmp[256];
				int nb = sscanf(attr->a_att_value, "%d IN IP4 %s", &stream->rtcp_port, tmp);
				if (nb == 1) {
					/* SDP rtcp attribute only contains the port */
				} else if (nb == 2) {
					strncpy(stream->rtcp_addr, tmp, sizeof(stream->rtcp_addr));
				} else {
					ms_warning("sdp has a strange a= line (%s) nb=%i", attr->a_att_value, nb);
				}
			}
		}

		/* read crypto lines if any */
		if (stream->proto == SalProtoRtpSavp) {
			int k, valid_count = 0;
				
			memset(&stream->crypto, 0, sizeof(stream->crypto));
			for (k=0;valid_count < SAL_CRYPTO_ALGO_MAX && (attr=sdp_message_attribute_get(msg,i,k))!=NULL;k++){
				char tmp[256], tmp2[256];
				if (keywordcmp("crypto",attr->a_att_field)==0 && attr->a_att_value!=NULL){
					int nb = sscanf(attr->a_att_value, "%d %255s inline:%255s",
						&stream->crypto[valid_count].tag,
						tmp,
						tmp2);
						ms_message("Found valid crypto line (tag:%d algo:'%s' key:'%s'", 
								stream->crypto[valid_count].tag, 
								tmp, 
								tmp2);
					if (nb == 3) {
						if (strcmp(tmp, "AES_CM_128_HMAC_SHA1_80") == 0)
							stream->crypto[valid_count].algo = AES_128_SHA1_80;
						else if (strcmp(tmp, "AES_CM_128_HMAC_SHA1_32") == 0)
							stream->crypto[valid_count].algo = AES_128_SHA1_32;
						else {
							ms_warning("Failed to parse crypto-algo: '%s'", tmp);
							stream->crypto[valid_count].algo = 0;
						}
						if (stream->crypto[valid_count].algo) {
							strncpy(stream->crypto[valid_count].master_key, tmp2, 41);
							stream->crypto[valid_count].master_key[40] = '\0';
							ms_message("Found valid crypto line (tag:%d algo:'%s' key:'%s'", 
								stream->crypto[valid_count].tag, 
								tmp, 
								stream->crypto[valid_count].master_key);
							valid_count++;
						}
					} else {
						ms_warning("sdp has a strange a= line (%s) nb=%i",attr->a_att_value,nb);
					}
				}
			}
			ms_message("Found: %d valid crypto lines", valid_count);
		}

		/* Get ICE candidate attributes if any */
		for (j = 0; (attr = sdp_message_attribute_get(msg, i, j)) != NULL; j++) {
			if ((keywordcmp("candidate", attr->a_att_field) == 0) && (attr->a_att_value != NULL)) {
				SalIceCandidate *candidate = &stream->ice_candidates[nb_ice_candidates];
				int nb = sscanf(attr->a_att_value, "%s %u UDP %u %s %d typ %s raddr %s rport %d",
					candidate->foundation, &candidate->componentID, &candidate->priority, candidate->addr, &candidate->port,
					candidate->type, candidate->raddr, &candidate->rport);
				if ((nb == 6) || (nb == 8)) nb_ice_candidates++;
				else memset(candidate, 0, sizeof(*candidate));
			} else if ((keywordcmp("remote-candidates", attr->a_att_field) == 0) && (attr->a_att_value != NULL)) {
				SalIceRemoteCandidate candidate;
				unsigned int componentID;
				int offset;
				char *ptr = attr->a_att_value;
				while (3 == sscanf(ptr, "%u %s %u%n", &componentID, candidate.addr, &candidate.port, &offset)) {
					if ((componentID > 0) && (componentID <= SAL_MEDIA_DESCRIPTION_MAX_ICE_REMOTE_CANDIDATES)) {
						SalIceRemoteCandidate *remote_candidate = &stream->ice_remote_candidates[componentID - 1];
						strncpy(remote_candidate->addr, candidate.addr, sizeof(remote_candidate->addr));
						remote_candidate->port = candidate.port;
					}
					ptr += offset;
					if (ptr[offset] == ' ') ptr += 1;
				}
			} else if ((keywordcmp("ice-ufrag", attr->a_att_field) == 0) && (attr->a_att_value != NULL)) {
				strncpy(stream->ice_ufrag, attr->a_att_value, sizeof(stream->ice_ufrag));
			} else if ((keywordcmp("ice-pwd", attr->a_att_field) == 0) && (attr->a_att_value != NULL)) {
				strncpy(stream->ice_pwd, attr->a_att_value, sizeof(stream->ice_pwd));
			} else if (keywordcmp("ice-mismatch", attr->a_att_field) == 0) {
				stream->ice_mismatch = TRUE;
			}
		}
	}
	desc->n_total_streams=i;
	return 0;
}
