
/*
linphone
Copyright (C) 2010  Belledonne Communications SARL
 (simon.morlat@linphone.org)

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
#ifdef WIN32
#include <time.h>
#endif
#include "linphonecore.h"
#include "sipsetup.h"
#include "lpconfig.h"
#include "private.h"
#include <ortp/event.h>
#include <ortp/b64.h>
#include <math.h>

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msvolume.h"
#include "mediastreamer2/msequalizer.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msjpegwriter.h"
#include "mediastreamer2/mseventqueue.h"
#include "mediastreamer2/mssndcard.h"

#ifdef VIDEO_ENABLED
static MSWebCam *get_nowebcam_device(){
	return ms_web_cam_manager_get_cam(ms_web_cam_manager_get(),"StaticImage: Static picture");
}
#endif

static bool_t generate_b64_crypto_key(int key_length, char* key_out, size_t key_out_size) {
	int b64_size;
	uint8_t* tmp = (uint8_t*) ms_malloc0(key_length);
	if (sal_get_random_bytes(tmp, key_length)==NULL) {
		ms_error("Failed to generate random key");
		ms_free(tmp);
		return FALSE;
	}

	b64_size = b64_encode((const char*)tmp, key_length, NULL, 0);
	if (b64_size == 0) {
		ms_error("Failed to get b64 result size");
		ms_free(tmp);
		return FALSE;
	}
	if (b64_size>=key_out_size){
		ms_error("Insufficient room for writing base64 SRTP key");
		ms_free(tmp);
		return FALSE;
	}
	b64_size=b64_encode((const char*)tmp, key_length, key_out, key_out_size);
	if (b64_size == 0) {
		ms_error("Failed to b64 encode key");
		ms_free(tmp);
		return FALSE;
	}
	key_out[b64_size] = '\0';
	ms_free(tmp);
	return TRUE;
}

LinphoneCore *linphone_call_get_core(const LinphoneCall *call){
	return call->core;
}

const char* linphone_call_get_authentication_token(LinphoneCall *call){
	return call->auth_token;
}

/**
 * Returns whether ZRTP authentication token is verified.
 * If not, it must be verified by users as described in ZRTP procedure.
 * Once done, the application must inform of the results with linphone_call_set_authentication_token_verified().
 * @param call the LinphoneCall
 * @return TRUE if authentication token is verifed, false otherwise.
 * @ingroup call_control
**/
bool_t linphone_call_get_authentication_token_verified(LinphoneCall *call){
	return call->auth_token_verified;
}

static bool_t linphone_call_are_all_streams_encrypted(LinphoneCall *call) {
	// Check ZRTP encryption in audiostream
	if (!call->audiostream_encrypted) {
		return FALSE;
	}

#ifdef VIDEO_ENABLED
	// If video enabled, check ZRTP encryption in videostream
	{
		const LinphoneCallParams *params=linphone_call_get_current_params(call);
		if (params->has_video && !call->videostream_encrypted) {
			return FALSE;
		}
	}
#endif

	return TRUE;
}

void propagate_encryption_changed(LinphoneCall *call){
	LinphoneCore *lc=call->core;
	if (!linphone_call_are_all_streams_encrypted(call)) {
		ms_message("Some streams are not encrypted");
		call->current_params.media_encryption=LinphoneMediaEncryptionNone;
		if (lc->vtable.call_encryption_changed)
			lc->vtable.call_encryption_changed(call->core, call, FALSE, call->auth_token);
	} else {
		ms_message("All streams are encrypted");
		call->current_params.media_encryption=LinphoneMediaEncryptionZRTP;
		if (lc->vtable.call_encryption_changed)
			lc->vtable.call_encryption_changed(call->core, call, TRUE, call->auth_token);
	}
}

#ifdef VIDEO_ENABLED
static void linphone_call_videostream_encryption_changed(void *data, bool_t encrypted){
	LinphoneCall *call = (LinphoneCall *)data;

	ms_message("Video stream is %s", encrypted ? "encrypted" : "not encrypted");
	call->videostream_encrypted=encrypted;
	propagate_encryption_changed(call);
}
#endif

static void linphone_call_audiostream_encryption_changed(void *data, bool_t encrypted) {
	char status[255]={0};
	LinphoneCall *call;
	ms_message("Audio stream is %s ", encrypted ? "encrypted" : "not encrypted");

	call = (LinphoneCall *)data;
	call->audiostream_encrypted=encrypted;

	if (encrypted && call->core->vtable.display_status != NULL) {
		snprintf(status,sizeof(status)-1,_("Authentication token is %s"),call->auth_token);
		 call->core->vtable.display_status(call->core, status);
	}

	propagate_encryption_changed(call);


#ifdef VIDEO_ENABLED
	// Enable video encryption
	{
		const LinphoneCallParams *params=linphone_call_get_current_params(call);
		if (params->has_video) {
			OrtpZrtpParams params;
			ms_message("Trying to enable encryption on video stream");
			params.zid_file=NULL; //unused
			video_stream_enable_zrtp(call->videostream,call->audiostream,&params);
		}
	}
#endif
}


static void linphone_call_audiostream_auth_token_ready(void *data, const char* auth_token, bool_t verified) {
	LinphoneCall *call=(LinphoneCall *)data;
	if (call->auth_token != NULL)
		ms_free(call->auth_token);

	call->auth_token=ms_strdup(auth_token);
	call->auth_token_verified=verified;

	ms_message("Authentication token is %s (%s)", auth_token, verified?"verified":"unverified");
}

/**
 * Set the result of ZRTP short code verification by user.
 * If remote party also does the same, it will update the ZRTP cache so that user's verification will not be required for the two users.
 * @param call the LinphoneCall
 * @param verified whether the ZRTP SAS is verified.
 * @ingroup call_control
**/
void linphone_call_set_authentication_token_verified(LinphoneCall *call, bool_t verified){
	if (call->audiostream==NULL){
		ms_error("linphone_call_set_authentication_token_verified(): No audio stream");
	}
	if (call->audiostream->ms.sessions.zrtp_context==NULL){
		ms_error("linphone_call_set_authentication_token_verified(): No zrtp context.");
	}
	if (!call->auth_token_verified && verified){
		ortp_zrtp_sas_verified(call->audiostream->ms.sessions.zrtp_context);
	}else if (call->auth_token_verified && !verified){
		ortp_zrtp_sas_reset_verified(call->audiostream->ms.sessions.zrtp_context);
	}
	call->auth_token_verified=verified;
	propagate_encryption_changed(call);
}

static MSList *make_codec_list(LinphoneCore *lc, const MSList *codecs, int bandwidth_limit,int* max_sample_rate, int nb_codecs_limit){
	MSList *l=NULL;
	const MSList *it;
	int nb = 0;
	if (max_sample_rate) *max_sample_rate=0;
	for(it=codecs;it!=NULL;it=it->next){
		PayloadType *pt=(PayloadType*)it->data;
		if (pt->flags & PAYLOAD_TYPE_ENABLED){
			if (bandwidth_limit>0 && !linphone_core_is_payload_type_usable_for_bandwidth(lc,pt,bandwidth_limit)){
				ms_message("Codec %s/%i eliminated because of audio bandwidth constraint of %i kbit/s",
					   pt->mime_type,pt->clock_rate,bandwidth_limit);
				continue;
			}
			if (linphone_core_check_payload_type_usability(lc,pt)){
				l=ms_list_append(l,payload_type_clone(pt));
				nb++;
				if (max_sample_rate && payload_type_get_rate(pt)>*max_sample_rate) *max_sample_rate=payload_type_get_rate(pt);
			}
		}
		if ((nb_codecs_limit > 0) && (nb >= nb_codecs_limit)) break;
	}
	return l;
}

static void update_media_description_from_stun(SalMediaDescription *md, const StunCandidate *ac, const StunCandidate *vc){
	int i;
	for (i = 0; i < md->n_active_streams; i++) {
		if ((md->streams[i].type == SalAudio) && (ac->port != 0)) {
			strcpy(md->streams[0].rtp_addr,ac->addr);
			md->streams[0].rtp_port=ac->port;
			if ((ac->addr[0]!='\0' && vc->addr[0]!='\0' && strcmp(ac->addr,vc->addr)==0) || md->n_active_streams==1){
				strcpy(md->addr,ac->addr);
			}
		}
		if ((md->streams[i].type == SalVideo) && (vc->port != 0)) {
			strcpy(md->streams[1].rtp_addr,vc->addr);
			md->streams[1].rtp_port=vc->port;
		}
	}
}

static void setup_encryption_keys(LinphoneCall *call, SalMediaDescription *md){
	LinphoneCore *lc=call->core;
	int i;
	SalMediaDescription *old_md=call->localdesc;
	bool_t keep_srtp_keys=lp_config_get_int(lc->config,"sip","keep_srtp_keys",1);

	for(i=0; i<md->n_active_streams; i++) {
		if (md->streams[i].proto == SalProtoRtpSavp) {
			if (keep_srtp_keys && old_md && old_md->streams[i].proto==SalProtoRtpSavp){
				int j;
				ms_message("Keeping same crypto keys.");
				for(j=0;j<SAL_CRYPTO_ALGO_MAX;++j){
					memcpy(&md->streams[i].crypto[j],&old_md->streams[i].crypto[j],sizeof(SalSrtpCryptoAlgo));
				}
			}else{
				md->streams[i].crypto[0].tag = 1;
				md->streams[i].crypto[0].algo = AES_128_SHA1_80;
				if (!generate_b64_crypto_key(30, md->streams[i].crypto[0].master_key, SAL_SRTP_KEY_SIZE))
					md->streams[i].crypto[0].algo = 0;
				md->streams[i].crypto[1].tag = 2;
				md->streams[i].crypto[1].algo = AES_128_SHA1_32;
				if (!generate_b64_crypto_key(30, md->streams[i].crypto[1].master_key, SAL_SRTP_KEY_SIZE))
					md->streams[i].crypto[1].algo = 0;
				md->streams[i].crypto[2].algo = 0;
			}
		}
	}
}

static void setup_rtcp_xr(LinphoneCall *call, SalMediaDescription *md) {
	LinphoneCore *lc = call->core;
	int i;

	md->rtcp_xr.enabled = lp_config_get_int(lc->config, "rtp", "rtcp_xr_enabled", 0);
	if (md->rtcp_xr.enabled == TRUE) {
		const char *rcvr_rtt_mode = lp_config_get_string(lc->config, "rtp", "rtcp_xr_rcvr_rtt_mode", "none");
		if (strcasecmp(rcvr_rtt_mode, "all") == 0) md->rtcp_xr.rcvr_rtt_mode = OrtpRtcpXrRcvrRttAll;
		else if (strcasecmp(rcvr_rtt_mode, "sender") == 0) md->rtcp_xr.rcvr_rtt_mode = OrtpRtcpXrRcvrRttSender;
		else md->rtcp_xr.rcvr_rtt_mode = OrtpRtcpXrRcvrRttNone;
		if (md->rtcp_xr.rcvr_rtt_mode != OrtpRtcpXrRcvrRttNone) {
			md->rtcp_xr.rcvr_rtt_max_size = lp_config_get_int(lc->config, "rtp", "rtcp_xr_rcvr_rtt_max_size", 0);
		}
		md->rtcp_xr.stat_summary_enabled = lp_config_get_int(lc->config, "rtp", "rtcp_xr_stat_summary_enabled", 0);
		if (md->rtcp_xr.stat_summary_enabled == TRUE) {
			md->rtcp_xr.stat_summary_flags = OrtpRtcpXrStatSummaryLoss | OrtpRtcpXrStatSummaryDup | OrtpRtcpXrStatSummaryJitt | OrtpRtcpXrStatSummaryTTL;
		}
		md->rtcp_xr.voip_metrics_enabled = lp_config_get_int(lc->config, "rtp", "rtcp_xr_voip_metrics_enabled", 0);
	}
	for (i = 0; i < md->n_active_streams; i++) {
		memcpy(&md->streams[i].rtcp_xr, &md->rtcp_xr, sizeof(md->streams[i].rtcp_xr));
	}
}

void linphone_call_make_local_media_description(LinphoneCore *lc, LinphoneCall *call){
	MSList *l;
	PayloadType *pt;
	SalMediaDescription *old_md=call->localdesc;
	int i;
	const char *me;
	SalMediaDescription *md=sal_media_description_new();
	LinphoneAddress *addr;
	char* local_ip=call->localip;
	const char *subject=linphone_call_params_get_session_name(&call->params);

	linphone_core_adapt_to_network(lc,call->ping_time,&call->params);

	if (call->dest_proxy)
		me=linphone_proxy_config_get_identity(call->dest_proxy);
	else
		me=linphone_core_get_identity(lc);
	addr=linphone_address_new(me);

	md->session_id=(old_md ? old_md->session_id : (rand() & 0xfff));
	md->session_ver=(old_md ? (old_md->session_ver+1) : (rand() & 0xfff));
	md->n_total_streams=(call->biggestdesc ? call->biggestdesc->n_total_streams : 1);

	strncpy(md->addr,local_ip,sizeof(md->addr));
	strncpy(md->username,linphone_address_get_username(addr),sizeof(md->username));
	if (subject) strncpy(md->name,subject,sizeof(md->name));

	if (call->params.down_bw)
		md->bandwidth=call->params.down_bw;
	else md->bandwidth=linphone_core_get_download_bandwidth(lc);

	/*set audio capabilities */
	md->n_active_streams=1;
	strncpy(md->streams[0].rtp_addr,local_ip,sizeof(md->streams[0].rtp_addr));
	strncpy(md->streams[0].rtcp_addr,local_ip,sizeof(md->streams[0].rtcp_addr));
	strncpy(md->streams[0].name,"Audio",sizeof(md->streams[0].name)-1);
	md->streams[0].rtp_port=call->media_ports[0].rtp_port;
	md->streams[0].rtcp_port=call->media_ports[0].rtcp_port;
	md->streams[0].proto=(call->params.media_encryption == LinphoneMediaEncryptionSRTP) ?
		SalProtoRtpSavp : SalProtoRtpAvp;
	md->streams[0].type=SalAudio;
	if (call->params.down_ptime)
		md->streams[0].ptime=call->params.down_ptime;
	else
		md->streams[0].ptime=linphone_core_get_download_ptime(lc);
	l=make_codec_list(lc,lc->codecs_conf.audio_codecs,call->params.audio_bw,&md->streams[0].max_rate,-1);
	pt=payload_type_clone(rtp_profile_get_payload_from_mime(lc->default_profile,"telephone-event"));
	l=ms_list_append(l,pt);
	md->streams[0].payloads=l;

	if (call->params.has_video){
		md->n_active_streams++;
		strncpy(md->streams[0].name,"Video",sizeof(md->streams[0].name)-1);
		md->streams[1].rtp_port=call->media_ports[1].rtp_port;
		md->streams[1].rtcp_port=call->media_ports[1].rtcp_port;
		md->streams[1].proto=md->streams[0].proto;
		md->streams[1].type=SalVideo;
		l=make_codec_list(lc,lc->codecs_conf.video_codecs,0,NULL,-1);
		md->streams[1].payloads=l;
	}
	if (md->n_total_streams < md->n_active_streams)
		md->n_total_streams = md->n_active_streams;

	/* Deactivate inactive streams. */
	for (i = md->n_active_streams; i < md->n_total_streams; i++) {
		md->streams[i].rtp_port = 0;
		md->streams[i].rtcp_port = 0;
		md->streams[i].proto = call->biggestdesc->streams[i].proto;
		md->streams[i].type = call->biggestdesc->streams[i].type;
		md->streams[i].dir = SalStreamInactive;
		l = make_codec_list(lc, lc->codecs_conf.video_codecs, 0, NULL, 1);
		md->streams[i].payloads = l;
	}

	setup_encryption_keys(call,md);
	setup_rtcp_xr(call, md);

	update_media_description_from_stun(md,&call->ac,&call->vc);
	if (call->ice_session != NULL) {
		linphone_core_update_local_media_description_from_ice(md, call->ice_session);
		linphone_core_update_ice_state_in_call_stats(call);
	}
#ifdef BUILD_UPNP
	if(call->upnp_session != NULL) {
		linphone_core_update_local_media_description_from_upnp(md, call->upnp_session);
		linphone_core_update_upnp_state_in_call_stats(call);
	}
#endif  //BUILD_UPNP
	linphone_address_destroy(addr);
	call->localdesc=md;
	if (old_md){
		call->localdesc_changed=sal_media_description_equals(md,old_md);
		sal_media_description_unref(old_md);
	}
}

static int find_port_offset(LinphoneCore *lc, int stream_index, int base_port){
	int offset;
	MSList *elem;
	int tried_port;
	int existing_port;
	bool_t already_used=FALSE;
	
	for(offset=0;offset<100;offset+=2){
		tried_port=base_port+offset;
		already_used=FALSE;
		for(elem=lc->calls;elem!=NULL;elem=elem->next){
			LinphoneCall *call=(LinphoneCall*)elem->data;
			existing_port=call->media_ports[stream_index].rtp_port;
			if (existing_port==tried_port) {
				already_used=TRUE;
				break;
			}
		}
		if (!already_used) break;
	}
	if (offset==100){
		ms_error("Could not find any free port !");
		return -1;
	}
	return offset;
}

static int select_random_port(LinphoneCore *lc, int stream_index, int min_port, int max_port) {
	MSList *elem;
	int nb_tries;
	int tried_port = 0;
	int existing_port = 0;
	bool_t already_used = FALSE;

	tried_port = (rand() % (max_port - min_port) + min_port) & ~0x1;
	if (tried_port < min_port) tried_port = min_port + 2;
	for (nb_tries = 0; nb_tries < 100; nb_tries++) {
		for (elem = lc->calls; elem != NULL; elem = elem->next) {
			LinphoneCall *call = (LinphoneCall *)elem->data;
			existing_port=call->media_ports[stream_index].rtp_port;
			if (existing_port == tried_port) {
				already_used = TRUE;
				break;
			}
		}
		if (!already_used) break;
	}
	if (nb_tries == 100) {
		ms_error("Could not find any free port!");
		return -1;
	}
	return tried_port;
}

static void port_config_set_random(LinphoneCall *call, int stream_index){
	call->media_ports[stream_index].rtp_port=-1;
	call->media_ports[stream_index].rtcp_port=-1;
}

static void port_config_set(LinphoneCall *call, int stream_index, int min_port, int max_port){
	int port_offset;
	if (min_port>0 && max_port>0){
		if (min_port == max_port) {
			/* Used fixed RTP audio port. */
			port_offset=find_port_offset(call->core, stream_index, min_port);
			if (port_offset==-1) {
				port_config_set_random(call, stream_index);
				return;
			}
			call->media_ports[stream_index].rtp_port=min_port+port_offset;
		} else {
			/* Select random RTP audio port in the specified range. */
			call->media_ports[stream_index].rtp_port = select_random_port(call->core, stream_index, min_port, max_port);
		}
		call->media_ports[stream_index].rtcp_port=call->media_ports[stream_index].rtp_port+1;
	}else port_config_set_random(call,stream_index);
}

static void linphone_call_init_common(LinphoneCall *call, LinphoneAddress *from, LinphoneAddress *to){
	int min_port, max_port;

	call->magic=linphone_call_magic;
	call->refcnt=1;
	call->state=LinphoneCallIdle;
	call->transfer_state = LinphoneCallIdle;
	call->media_start_time=0;
	call->log=linphone_call_log_new(call, from, to);
	call->owns_call_log=TRUE;
	call->camera_enabled=TRUE;

	call->log->reports[LINPHONE_CALL_STATS_AUDIO]=linphone_reporting_new();
	call->log->reports[LINPHONE_CALL_STATS_VIDEO]=linphone_reporting_new();

	linphone_core_get_audio_port_range(call->core, &min_port, &max_port);
	port_config_set(call,0,min_port,max_port);
	
	linphone_core_get_video_port_range(call->core, &min_port, &max_port);
	port_config_set(call,1,min_port,max_port);
	
	linphone_call_init_stats(&call->stats[LINPHONE_CALL_STATS_AUDIO], LINPHONE_CALL_STATS_AUDIO);
	linphone_call_init_stats(&call->stats[LINPHONE_CALL_STATS_VIDEO], LINPHONE_CALL_STATS_VIDEO);
}

void linphone_call_init_stats(LinphoneCallStats *stats, int type) {
	stats->type = type;
	stats->received_rtcp = NULL;
	stats->sent_rtcp = NULL;
	stats->ice_state = LinphoneIceStateNotActivated;
#ifdef BUILD_UPNP
	stats->upnp_state = LinphoneUpnpStateIdle;
#else
	stats->upnp_state = LinphoneUpnpStateNotAvailable;
#endif //BUILD_UPNP
}


static void discover_mtu(LinphoneCore *lc, const char *remote){
	int mtu;
	if (lc->net_conf.mtu==0	){
		/*attempt to discover mtu*/
		mtu=ms_discover_mtu(remote);
		if (mtu>0){
			ms_set_mtu(mtu);
			ms_message("Discovered mtu is %i, RTP payload max size is %i",
				mtu, ms_get_payload_max_size());
		}
	}
}

void linphone_call_create_op(LinphoneCall *call){
	if (call->op) sal_op_release(call->op);
	call->op=sal_op_new(call->core->sal);
	sal_op_set_user_pointer(call->op,call);
	if (call->params.referer)
		sal_call_set_referer(call->op,call->params.referer->op);
	linphone_configure_op(call->core,call->op,call->log->to,call->params.custom_headers,FALSE);
	if (call->params.privacy != LinphonePrivacyDefault)
		sal_op_set_privacy(call->op,(SalPrivacyMask)call->params.privacy);
	/*else privacy might be set by proxy */
}

/*
 * Choose IP version we are going to use for RTP socket.
 * The algorithm is as follows:
 * - if ipv6 is disabled at the core level, it is always AF_INET
 * - Otherwise, if the destination address for the call is an IPv6 address, use IPv6.
 * - Otherwise, if the call is done through a known proxy config, then use the information obtained during REGISTER
 * to know if IPv6 is supported by the server.
**/
static void linphone_call_outgoing_select_ip_version(LinphoneCall *call, LinphoneAddress *to, LinphoneProxyConfig *cfg){
	if (linphone_core_ipv6_enabled(call->core)){
		call->af=AF_INET;
		if (sal_address_is_ipv6((SalAddress*)to)){
			call->af=AF_INET6;
		}else if (cfg && cfg->op){
			call->af=sal_op_is_ipv6(cfg->op) ? AF_INET6 : AF_INET;
		}
	}else call->af=AF_INET;
}

LinphoneCall * linphone_call_new_outgoing(struct _LinphoneCore *lc, LinphoneAddress *from, LinphoneAddress *to, const LinphoneCallParams *params, LinphoneProxyConfig *cfg){
	LinphoneCall *call=ms_new0(LinphoneCall,1);

	call->dir=LinphoneCallOutgoing;
	call->core=lc;
	linphone_call_outgoing_select_ip_version(call,to,cfg);
	linphone_core_get_local_ip(lc,call->af,call->localip);
	linphone_call_init_common(call,from,to);
	_linphone_call_params_copy(&call->params,params);

	if (linphone_core_get_firewall_policy(call->core) == LinphonePolicyUseIce) {
		call->ice_session = ice_session_new();
		ice_session_set_role(call->ice_session, IR_Controlling);
	}
	if (linphone_core_get_firewall_policy(call->core) == LinphonePolicyUseStun) {
		call->ping_time=linphone_core_run_stun_tests(call->core,call);
	}
#ifdef BUILD_UPNP
	if (linphone_core_get_firewall_policy(call->core) == LinphonePolicyUseUpnp) {
		if(!lc->rtp_conf.disable_upnp) {
			call->upnp_session = linphone_upnp_session_new(call);
		}
	}
#endif //BUILD_UPNP

	discover_mtu(lc,linphone_address_get_domain (to));
	if (params->referer){
		call->referer=linphone_call_ref(params->referer);
	}
	call->dest_proxy=cfg;
	linphone_call_create_op(call);
	return call;
}

static void linphone_call_incoming_select_ip_version(LinphoneCall *call){
	if (linphone_core_ipv6_enabled(call->core)){
		call->af=sal_op_is_ipv6(call->op) ? AF_INET6 : AF_INET;
	}else call->af=AF_INET;
}

LinphoneCall * linphone_call_new_incoming(LinphoneCore *lc, LinphoneAddress *from, LinphoneAddress *to, SalOp *op){
	LinphoneCall *call=ms_new0(LinphoneCall,1);
	char *from_str;
	const SalMediaDescription *md;

	call->dir=LinphoneCallIncoming;
	sal_op_set_user_pointer(op,call);
	call->op=op;
	call->core=lc;
	linphone_call_incoming_select_ip_version(call);

	if (lc->sip_conf.ping_with_options){
#ifdef BUILD_UPNP
		if (lc->upnp != NULL && linphone_core_get_firewall_policy(lc)==LinphonePolicyUseUpnp &&
			linphone_upnp_context_get_state(lc->upnp) == LinphoneUpnpStateOk) {
#else //BUILD_UPNP
		{
#endif //BUILD_UPNP
			/*the following sends an option request back to the caller so that
			 we get a chance to discover our nat'd address before answering.*/
			call->ping_op=sal_op_new(lc->sal);
			from_str=linphone_address_as_string_uri_only(from);
			sal_op_set_route(call->ping_op,sal_op_get_network_origin(op));
			sal_op_set_user_pointer(call->ping_op,call);
			sal_ping(call->ping_op,linphone_core_find_best_identity(lc,from),from_str);
			ms_free(from_str);
		}
	}

	linphone_address_clean(from);
	linphone_core_get_local_ip(lc,call->af,call->localip);
	linphone_call_init_common(call, from, to);
	call->log->call_id=ms_strdup(sal_op_get_call_id(op)); /*must be known at that time*/
	linphone_core_init_default_params(lc, &call->params);

	/*
	 * Initialize call parameters according to incoming call parameters. This is to avoid to ask later (during reINVITEs) for features that the remote
	 * end apparently does not support. This features are: privacy, video
	 */
	/*set privacy*/
	call->current_params.privacy=(LinphonePrivacyMask)sal_op_get_privacy(call->op);
	/*set video support */
	md=sal_call_get_remote_media_description(op);
	call->params.has_video = !!lc->video_policy.automatically_accept;
	if (md) {
		// It is licit to receive an INVITE without SDP
		// In this case WE chose the media parameters according to policy.
		call->params.has_video &= linphone_core_media_description_contains_video_stream(md);
	}
	/*create the ice session now if ICE is required*/
	if (linphone_core_get_firewall_policy(call->core)==LinphonePolicyUseIce){
		call->ice_session = ice_session_new();
		ice_session_set_role(call->ice_session, IR_Controlled);
	}
	/*reserve the sockets immediately*/
	linphone_call_init_media_streams(call);
	switch (linphone_core_get_firewall_policy(call->core)) {
		case LinphonePolicyUseIce:
			linphone_call_prepare_ice(call,TRUE);
			break;
		case LinphonePolicyUseStun:
			call->ping_time=linphone_core_run_stun_tests(call->core,call);
			/* No break to also destroy ice session in this case. */
			break;
		case LinphonePolicyUseUpnp:
#ifdef BUILD_UPNP
			if(!lc->rtp_conf.disable_upnp) {
				call->upnp_session = linphone_upnp_session_new(call);
				if (call->upnp_session != NULL) {
					if (linphone_core_update_upnp_from_remote_media_description(call, sal_call_get_remote_media_description(op))<0) {
						/* uPnP port mappings failed, proceed with the call anyway. */
						linphone_call_delete_upnp_session(call);
					}
				}
			}
#endif //BUILD_UPNP
			break;
		default:
			break;
	}

	discover_mtu(lc,linphone_address_get_domain(from));
	return call;
}

/* this function is called internally to get rid of a call.
 It performs the following tasks:
 - remove the call from the internal list of calls
 - update the call logs accordingly
*/

static void linphone_call_set_terminated(LinphoneCall *call){
	LinphoneCore *lc=call->core;

	linphone_call_stop_media_streams(call);
	ms_media_stream_sessions_uninit(&call->sessions[0]);
	ms_media_stream_sessions_uninit(&call->sessions[1]);
	linphone_call_delete_upnp_session(call);
	linphone_call_delete_ice_session(call);
	linphone_core_update_allocated_audio_bandwidth(lc);

	call->owns_call_log=FALSE;
	linphone_call_log_completed(call);


	if (call == lc->current_call){
		ms_message("Resetting the current call");
		lc->current_call=NULL;
	}

	if (linphone_core_del_call(lc,call) != 0){
		ms_error("Could not remove the call from the list !!!");
	}

	linphone_core_conference_check_uninit(lc);
	if (call->ringing_beep){
		linphone_core_stop_dtmf(lc);
		call->ringing_beep=FALSE;
	}
}

void linphone_call_fix_call_parameters(LinphoneCall *call){
	call->params.has_video=call->current_params.has_video;
	if (call->params.media_encryption != LinphoneMediaEncryptionZRTP) /*in case of ZRTP call parameter are handle after zrtp negociation*/
		call->params.media_encryption=call->current_params.media_encryption;
}

const char *linphone_call_state_to_string(LinphoneCallState cs){
	switch (cs){
		case LinphoneCallIdle:
			return "LinphoneCallIdle";
		case LinphoneCallIncomingReceived:
			return "LinphoneCallIncomingReceived";
		case LinphoneCallOutgoingInit:
			return "LinphoneCallOutgoingInit";
		case LinphoneCallOutgoingProgress:
			return "LinphoneCallOutgoingProgress";
		case LinphoneCallOutgoingRinging:
			return "LinphoneCallOutgoingRinging";
		case LinphoneCallOutgoingEarlyMedia:
			return "LinphoneCallOutgoingEarlyMedia";
		case LinphoneCallConnected:
			return "LinphoneCallConnected";
		case LinphoneCallStreamsRunning:
			return "LinphoneCallStreamsRunning";
		case LinphoneCallPausing:
			return "LinphoneCallPausing";
		case LinphoneCallPaused:
			return "LinphoneCallPaused";
		case LinphoneCallResuming:
			return "LinphoneCallResuming";
		case LinphoneCallRefered:
			return "LinphoneCallRefered";
		case LinphoneCallError:
			return "LinphoneCallError";
		case LinphoneCallEnd:
			return "LinphoneCallEnd";
		case LinphoneCallPausedByRemote:
			return "LinphoneCallPausedByRemote";
		case LinphoneCallUpdatedByRemote:
			return "LinphoneCallUpdatedByRemote";
		case LinphoneCallIncomingEarlyMedia:
			return "LinphoneCallIncomingEarlyMedia";
		case LinphoneCallUpdating:
			return "LinphoneCallUpdating";
		case LinphoneCallReleased:
			return "LinphoneCallReleased";
	}
	return "undefined state";
}

void linphone_call_set_state(LinphoneCall *call, LinphoneCallState cstate, const char *message){
	LinphoneCore *lc=call->core;

	if (call->state!=cstate){
		call->prevstate=call->state;
		if (call->state==LinphoneCallEnd || call->state==LinphoneCallError){
			if (cstate!=LinphoneCallReleased){
				ms_warning("Spurious call state change from %s to %s, ignored.",linphone_call_state_to_string(call->state),
				   linphone_call_state_to_string(cstate));
				return;
			}
		}

		ms_message("Call %p: moving from state %s to %s",call,linphone_call_state_to_string(call->state),
							   linphone_call_state_to_string(cstate));

		if (cstate!=LinphoneCallRefered){
			/*LinphoneCallRefered is rather an event, not a state.
			 Indeed it does not change the state of the call (still paused or running)*/
			call->state=cstate;
		}
		
		if (cstate==LinphoneCallEnd || cstate==LinphoneCallError){
			switch(call->non_op_error.reason){
				case SalReasonDeclined:
					call->log->status=LinphoneCallDeclined;
					break;
				case SalReasonRequestTimeout:
					call->log->status=LinphoneCallMissed;
				break;
				default:
				break;
			}
			linphone_call_set_terminated(call);
		}
		if (cstate == LinphoneCallConnected) {
			call->log->status=LinphoneCallSuccess;
			call->media_start_time=time(NULL);
		}

		if (lc->vtable.call_state_changed)
			lc->vtable.call_state_changed(lc,call,cstate,message);
		if (cstate==LinphoneCallReleased){
			linphone_reporting_publish(call);

			if (call->op!=NULL) {
				/*transfer the last error so that it can be obtained even in Released state*/
				if (call->non_op_error.reason==SalReasonNone){
					const SalErrorInfo *ei=sal_op_get_error_info(call->op);
					sal_error_info_set(&call->non_op_error,ei->reason,ei->protocol_code,ei->status_string,ei->warnings);
				}
				/* so that we cannot have anymore upcalls for SAL
				 concerning this call*/
				sal_op_release(call->op);
				call->op=NULL;
			}
			/*it is necessary to reset pointers to other call to prevent circular references that would result in memory never freed.*/
			if (call->referer){
				linphone_call_unref(call->referer);
				call->referer=NULL;
			}
			if (call->transfer_target){
				linphone_call_unref(call->transfer_target);
				call->transfer_target=NULL;
			}
			linphone_call_unref(call);
		}
	}
}

static void linphone_call_destroy(LinphoneCall *obj)
{
	ms_message("Call [%p] freed.",obj);
	if (obj->op!=NULL) {
		sal_op_release(obj->op);
		obj->op=NULL;
	}
	if (obj->biggestdesc!=NULL){
		sal_media_description_unref(obj->biggestdesc);
		obj->biggestdesc=NULL;
	}
	if (obj->resultdesc!=NULL) {
		sal_media_description_unref(obj->resultdesc);
		obj->resultdesc=NULL;
	}
	if (obj->localdesc!=NULL) {
		sal_media_description_unref(obj->localdesc);
		obj->localdesc=NULL;
	}
	if (obj->ping_op) {
		sal_op_release(obj->ping_op);
	}
	if (obj->refer_to){
		ms_free(obj->refer_to);
	}
	if (obj->referer){
		linphone_call_unref(obj->referer);
		obj->referer=NULL;
	}
	if (obj->transfer_target){
		linphone_call_unref(obj->transfer_target);
	}
	if (obj->owns_call_log)
		linphone_call_log_destroy(obj->log);
	if (obj->auth_token) {
		ms_free(obj->auth_token);
	}
	linphone_call_params_uninit(&obj->params);
	linphone_call_params_uninit(&obj->current_params);
	sal_error_info_reset(&obj->non_op_error);
	ms_free(obj);
}

/**
 * @addtogroup call_control
 * @{
**/

/**
 * Increments the call 's reference count.
 * An application that wishes to retain a pointer to call object
 * must use this function to unsure the pointer remains
 * valid. Once the application no more needs this pointer,
 * it must call linphone_call_unref().
**/
LinphoneCall * linphone_call_ref(LinphoneCall *obj){
	obj->refcnt++;
	return obj;
}

/**
 * Decrements the call object reference count.
 * See linphone_call_ref().
**/
void linphone_call_unref(LinphoneCall *obj){
	obj->refcnt--;
	if (obj->refcnt==0){
		linphone_call_destroy(obj);
	}
}

/**
 * Returns current parameters associated to the call.
**/
const LinphoneCallParams * linphone_call_get_current_params(LinphoneCall *call){
#ifdef VIDEO_ENABLED
	VideoStream *vstream;
#endif
	MS_VIDEO_SIZE_ASSIGN(call->current_params.sent_vsize, UNKNOWN);
	MS_VIDEO_SIZE_ASSIGN(call->current_params.recv_vsize, UNKNOWN);
#ifdef VIDEO_ENABLED
	vstream = call->videostream;
	if (vstream != NULL) {
		call->current_params.sent_vsize = video_stream_get_sent_video_size(vstream);
		call->current_params.recv_vsize = video_stream_get_received_video_size(vstream);
	}
#endif

	return &call->current_params;
}

static bool_t is_video_active(const SalStreamDescription *sd){
	return sd->rtp_port!=0 && sd->dir!=SalStreamInactive;
}

/**
 * Returns call parameters proposed by remote.
 *
 * This is useful when receiving an incoming call, to know whether the remote party
 * supports video, encryption or whatever.
**/
const LinphoneCallParams * linphone_call_get_remote_params(LinphoneCall *call){
	LinphoneCallParams *cp=&call->remote_params;
	memset(cp,0,sizeof(*cp));
	if (call->op){
		SalMediaDescription *md=sal_call_get_remote_media_description(call->op);
		if (md){
			SalStreamDescription *asd,*vsd,*secure_asd,*secure_vsd;

			asd=sal_media_description_find_stream(md,SalProtoRtpAvp,SalAudio);
			vsd=sal_media_description_find_stream(md,SalProtoRtpAvp,SalVideo);
			secure_asd=sal_media_description_find_stream(md,SalProtoRtpSavp,SalAudio);
			secure_vsd=sal_media_description_find_stream(md,SalProtoRtpSavp,SalVideo);
			if (secure_vsd){
				cp->has_video=is_video_active(secure_vsd);
				if (secure_asd || asd==NULL)
					cp->media_encryption=LinphoneMediaEncryptionSRTP;
			}else if (vsd){
				cp->has_video=is_video_active(vsd);
			}
			if (!cp->has_video){
				if (md->bandwidth>0 && md->bandwidth<=linphone_core_get_edge_bw(call->core)){
					cp->low_bandwidth=TRUE;
				}
			}
			if (md->name[0]!='\0') linphone_call_params_set_session_name(cp,md->name);
		}
		cp->custom_headers=(SalCustomHeader*)sal_op_get_recv_custom_header(call->op);
		return cp;
	}
	return NULL;
}

/**
 * Returns the remote address associated to this call
 *
**/
const LinphoneAddress * linphone_call_get_remote_address(const LinphoneCall *call){
	return call->dir==LinphoneCallIncoming ? call->log->from : call->log->to;
}

/**
 * Returns the remote address associated to this call as a string.
 *
 * The result string must be freed by user using ms_free().
**/
char *linphone_call_get_remote_address_as_string(const LinphoneCall *call){
	return linphone_address_as_string(linphone_call_get_remote_address(call));
}

/**
 * Retrieves the call's current state.
**/
LinphoneCallState linphone_call_get_state(const LinphoneCall *call){
	return call->state;
}

/**
 * Returns the reason for a call termination (either error or normal termination)
**/
LinphoneReason linphone_call_get_reason(const LinphoneCall *call){
	return linphone_error_info_get_reason(linphone_call_get_error_info(call));
}

/**
 * Returns full details about call errors or termination reasons.
**/
const LinphoneErrorInfo *linphone_call_get_error_info(const LinphoneCall *call){
	if (call->non_op_error.reason!=SalReasonNone){
		return (const LinphoneErrorInfo*)&call->non_op_error;
	}else return linphone_error_info_from_sal_op(call->op);
}

/**
 * Get the user_pointer in the LinphoneCall
 *
 * @ingroup call_control
 *
 * return user_pointer an opaque user pointer that can be retrieved at any time
**/
void *linphone_call_get_user_pointer(LinphoneCall *call)
{
	return call->user_pointer;
}

/**
 * Set the user_pointer in the LinphoneCall
 *
 * @ingroup call_control
 *
 * the user_pointer is an opaque user pointer that can be retrieved at any time in the LinphoneCall
**/
void linphone_call_set_user_pointer(LinphoneCall *call, void *user_pointer)
{
	call->user_pointer = user_pointer;
}

/**
 * Returns the call log associated to this call.
**/
LinphoneCallLog *linphone_call_get_call_log(const LinphoneCall *call){
	return call->log;
}

/**
 * Returns the refer-to uri (if the call was transfered).
**/
const char *linphone_call_get_refer_to(const LinphoneCall *call){
	return call->refer_to;
}

/**
 * Returns the transferer if this call was started automatically as a result of an incoming transfer request.
 * The call in which the transfer request was received is returned in this case.
**/
LinphoneCall *linphone_call_get_transferer_call(const LinphoneCall *call){
	return call->referer;
}

/**
 * When this call has received a transfer request, returns the new call that was automatically created as a result of the transfer.
**/
LinphoneCall *linphone_call_get_transfer_target_call(const LinphoneCall *call){
	return call->transfer_target;
}

/**
 * Returns direction of the call (incoming or outgoing).
**/
LinphoneCallDir linphone_call_get_dir(const LinphoneCall *call){
	return call->log->dir;
}

/**
 * Returns the far end's user agent description string, if available.
**/
const char *linphone_call_get_remote_user_agent(LinphoneCall *call){
	if (call->op){
		return sal_op_get_remote_ua (call->op);
	}
	return NULL;
}

/**
 * Returns the far end's sip contact as a string, if available.
**/
const char *linphone_call_get_remote_contact(LinphoneCall *call){
	const LinphoneCallParams* lcp = linphone_call_get_remote_params(call);
	if( lcp ){
		// we're not using sal_op_get_remote_contact() here because the returned value is stripped from
		// params that we need, like the instanceid. Getting it from the headers will make sure we
		// get everything
		return linphone_call_params_get_custom_header(lcp, "Contact");
	}
	return NULL;
}


/**
 * Returns true if this calls has received a transfer that has not been
 * executed yet.
 * Pending transfers are executed when this call is being paused or closed,
 * locally or by remote endpoint.
 * If the call is already paused while receiving the transfer request, the
 * transfer immediately occurs.
**/
bool_t linphone_call_has_transfer_pending(const LinphoneCall *call){
	return call->refer_pending;
}

/**
 * Returns call's duration in seconds.
**/
int linphone_call_get_duration(const LinphoneCall *call){
	if (call->media_start_time==0) return 0;
	return time(NULL)-call->media_start_time;
}

/**
 * Returns the call object this call is replacing, if any.
 * Call replacement can occur during call transfers.
 * By default, the core automatically terminates the replaced call and accept the new one.
 * This function allows the application to know whether a new incoming call is a one that replaces another one.
**/
LinphoneCall *linphone_call_get_replaced_call(LinphoneCall *call){
	SalOp *op=sal_call_get_replaces(call->op);
	if (op){
		return (LinphoneCall*)sal_op_get_user_pointer(op);
	}
	return NULL;
}

/**
 * Indicate whether camera input should be sent to remote end.
**/
void linphone_call_enable_camera (LinphoneCall *call, bool_t enable){
#ifdef VIDEO_ENABLED
	if ((call->state==LinphoneCallStreamsRunning || call->state==LinphoneCallOutgoingEarlyMedia || call->state==LinphoneCallIncomingEarlyMedia)
		&& call->videostream!=NULL ){
		LinphoneCore *lc=call->core;
		MSWebCam *nowebcam=get_nowebcam_device();
		if (call->camera_enabled!=enable && lc->video_conf.device!=nowebcam){
			video_stream_change_camera(call->videostream,
						 enable ? lc->video_conf.device : nowebcam);
		}
	}
	call->camera_enabled=enable;
#endif
}

/**
 * Request remote side to send us a Video Fast Update.
**/
void linphone_call_send_vfu_request(LinphoneCall *call)
{
#ifdef VIDEO_ENABLED
	if (LinphoneCallStreamsRunning == linphone_call_get_state(call))
		sal_call_send_vfu_request(call->op);
#endif
}


/**
 * Take a photo of currently received video and write it into a jpeg file.
**/
int linphone_call_take_video_snapshot(LinphoneCall *call, const char *file){
#ifdef VIDEO_ENABLED
	if (call->videostream!=NULL && call->videostream->jpegwriter!=NULL){
		return ms_filter_call_method(call->videostream->jpegwriter,MS_JPEG_WRITER_TAKE_SNAPSHOT,(void*)file);
	}
	ms_warning("Cannot take snapshot: no currently running video stream on this call.");
	return -1;
#endif
	return -1;
}

/**
 * Returns TRUE if camera pictures are allowed to be sent to the remote party.
**/
bool_t linphone_call_camera_enabled (const LinphoneCall *call){
	return call->camera_enabled;
}

/**
 * Enable video stream.
**/
void linphone_call_params_enable_video(LinphoneCallParams *cp, bool_t enabled){
	cp->has_video=enabled;
}

/**
 * Returns the audio codec used in the call, described as a PayloadType structure.
**/
const PayloadType* linphone_call_params_get_used_audio_codec(const LinphoneCallParams *cp) {
	return cp->audio_codec;
}


/**
 * Returns the video codec used in the call, described as a PayloadType structure.
**/
const PayloadType* linphone_call_params_get_used_video_codec(const LinphoneCallParams *cp) {
	return cp->video_codec;
}

MSVideoSize linphone_call_params_get_sent_video_size(const LinphoneCallParams *cp) {
	return cp->sent_vsize;
}

MSVideoSize linphone_call_params_get_received_video_size(const LinphoneCallParams *cp) {
	return cp->recv_vsize;
}

/**
 * @ingroup call_control
 * Use to know if this call has been configured in low bandwidth mode.
 * This mode can be automatically discovered thanks to a stun server when activate_edge_workarounds=1 in section [net] of configuration file.
 * An application that would have reliable way to know network capacity may not use activate_edge_workarounds=1 but instead manually configure
 * low bandwidth mode with linphone_call_params_enable_low_bandwidth().
 * <br> When enabled, this param may transform a call request with video in audio only mode.
 * @return TRUE if low bandwidth has been configured/detected
 */
bool_t linphone_call_params_low_bandwidth_enabled(const LinphoneCallParams *cp) {
	return cp->low_bandwidth;
}

/**
 * @ingroup call_control
 * Indicate low bandwith mode.
 * Configuring a call to low bandwidth mode will result in the core to activate several settings for the call in order to ensure that bitrate usage
 * is lowered to the minimum possible. Typically, ptime (packetization time) will be increased, audio codec's output bitrate will be targetted to 20kbit/s provided
 * that it is achievable by the codec selected after SDP handshake. Video is automatically disabled.
 *
**/
void linphone_call_params_enable_low_bandwidth(LinphoneCallParams *cp, bool_t enabled){
	cp->low_bandwidth=enabled;
}

/**
 * Returns whether video is enabled.
**/
bool_t linphone_call_params_video_enabled(const LinphoneCallParams *cp){
	return cp->has_video;
}

/**
 * Returns kind of media encryption selected for the call.
**/
LinphoneMediaEncryption linphone_call_params_get_media_encryption(const LinphoneCallParams *cp) {
	return cp->media_encryption;
}

/**
 * Set requested media encryption for a call.
**/
void linphone_call_params_set_media_encryption(LinphoneCallParams *cp, LinphoneMediaEncryption e) {
	cp->media_encryption = e;
}


/**
 * Enable sending of real early media (during outgoing calls).
**/
void linphone_call_params_enable_early_media_sending(LinphoneCallParams *cp, bool_t enabled){
	cp->real_early_media=enabled;
}

/**
 * Indicates whether sending of early media was enabled.
**/
bool_t linphone_call_params_early_media_sending_enabled(const LinphoneCallParams *cp){
	return cp->real_early_media;
}

/**
 * Returns true if the call is part of the locally managed conference.
**/
bool_t linphone_call_params_local_conference_mode(const LinphoneCallParams *cp){
	return cp->in_conference;
}

/**
 * Refine bandwidth settings for this call by setting a bandwidth limit for audio streams.
 * As a consequence, codecs whose bitrates are not compatible with this limit won't be used.
**/
void linphone_call_params_set_audio_bandwidth_limit(LinphoneCallParams *cp, int bandwidth){
	cp->audio_bw=bandwidth;
}

void linphone_call_params_add_custom_header(LinphoneCallParams *params, const char *header_name, const char *header_value){
	params->custom_headers=sal_custom_header_append(params->custom_headers,header_name,header_value);
}

const char *linphone_call_params_get_custom_header(const LinphoneCallParams *params, const char *header_name){
	return sal_custom_header_find(params->custom_headers,header_name);
}

/**
 * Returns the session name of the media session (ie in SDP). Subject from the SIP message can be retrieved using linphone_call_params_get_custom_header() and is different.
 * @param cp the call parameters.
**/
const char *linphone_call_params_get_session_name(const LinphoneCallParams *cp){
	return cp->session_name;
}

/**
 * Set the session name of the media session (ie in SDP). Subject from the SIP message (which is different) can be set using linphone_call_params_set_custom_header().
 * @param cp the call parameters.
 * @param name the session name 
**/
void linphone_call_params_set_session_name(LinphoneCallParams *cp, const char *name){
	if (cp->session_name){
		ms_free(cp->session_name);
		cp->session_name=NULL;
	}
	if (name) cp->session_name=ms_strdup(name);
}

void _linphone_call_params_copy(LinphoneCallParams *ncp, const LinphoneCallParams *cp){
	if (ncp==cp) return;
	memcpy(ncp,cp,sizeof(LinphoneCallParams));
	if (cp->record_file) ncp->record_file=ms_strdup(cp->record_file);
	if (cp->session_name) ncp->session_name=ms_strdup(cp->session_name);
	/*
	 * The management of the custom headers is not optimal. We copy everything while ref counting would be more efficient.
	 */
	if (cp->custom_headers) ncp->custom_headers=sal_custom_header_clone(cp->custom_headers);
}

/**
 * @ingroup call_control
 * Set requested level of privacy for the call.
 * \xmlonly <language-tags>javascript</language-tags> \endxmlonly
 * @param params the call parameters to be modified
 * @param LinphonePrivacy to configure privacy
 * */
void linphone_call_params_set_privacy(LinphoneCallParams *params, LinphonePrivacyMask privacy) {
	params->privacy=privacy;
}

/**
 * @ingroup call_control
 * Get requested level of privacy for the call.
 * @param params the call parameters
 * @return Privacy mode
 * */
LinphonePrivacyMask linphone_call_params_get_privacy(const LinphoneCallParams *params) {
	return params->privacy;
}

/**
 * @ingroup call_control
 * @return string value of LinphonePrivacy enum
 **/
const char* linphone_privacy_to_string(LinphonePrivacy privacy) {
	switch(privacy) {
	case LinphonePrivacyDefault: return "LinphonePrivacyDefault";
	case LinphonePrivacyUser: return "LinphonePrivacyUser";
	case LinphonePrivacyHeader: return "LinphonePrivacyHeader";
	case LinphonePrivacySession: return "LinphonePrivacySession";
	case LinphonePrivacyId: return "LinphonePrivacyId";
	case LinphonePrivacyNone: return "LinphonePrivacyNone";
	case LinphonePrivacyCritical: return "LinphonePrivacyCritical";
	default: return "Unknown privacy mode";
	}
}
/**
 * Copy existing LinphoneCallParams to a new LinphoneCallParams object.
**/
LinphoneCallParams * linphone_call_params_copy(const LinphoneCallParams *cp){
	LinphoneCallParams *ncp=ms_new0(LinphoneCallParams,1);
	_linphone_call_params_copy(ncp,cp);
	return ncp;
}

void linphone_call_params_uninit(LinphoneCallParams *p){
	if (p->record_file) ms_free(p->record_file);
	if (p->custom_headers) sal_custom_header_free(p->custom_headers);
}

/**
 * Destroy LinphoneCallParams.
**/
void linphone_call_params_destroy(LinphoneCallParams *p){
	linphone_call_params_uninit(p);
	ms_free(p);
}


/**
 * @}
**/


#ifdef TEST_EXT_RENDERER
static void rendercb(void *data, const MSPicture *local, const MSPicture *remote){
	ms_message("rendercb, local buffer=%p, remote buffer=%p",
			   local ? local->planes[0] : NULL, remote? remote->planes[0] : NULL);
}
#endif

#ifdef VIDEO_ENABLED
static void video_stream_event_cb(void *user_pointer, const MSFilter *f, const unsigned int event_id, const void *args){
	LinphoneCall* call = (LinphoneCall*) user_pointer;
	ms_warning("In linphonecall.c: video_stream_event_cb");
	switch (event_id) {
		case MS_VIDEO_DECODER_DECODING_ERRORS:
			ms_warning("Case is MS_VIDEO_DECODER_DECODING_ERRORS");
			linphone_call_send_vfu_request(call);
			break;
		case MS_VIDEO_DECODER_FIRST_IMAGE_DECODED:
			ms_message("First video frame decoded successfully");
			if (call->nextVideoFrameDecoded._func != NULL)
			call->nextVideoFrameDecoded._func(call, call->nextVideoFrameDecoded._user_data);
			break;
		default:
			ms_warning("Unhandled event %i", event_id);
			break;
	}
}
#endif

void linphone_call_set_next_video_frame_decoded_callback(LinphoneCall *call, LinphoneCallCbFunc cb, void* user_data) {
	call->nextVideoFrameDecoded._func = cb;
	call->nextVideoFrameDecoded._user_data = user_data;
#ifdef VIDEO_ENABLED
	if (call->videostream && call->videostream->ms.decoder)
		ms_filter_call_method_noarg(call->videostream->ms.decoder, MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION);
#endif
}

static void port_config_set_random_choosed(LinphoneCall *call, int stream_index, RtpSession *session){
	call->media_ports[stream_index].rtp_port=rtp_session_get_local_port(session);
	call->media_ports[stream_index].rtcp_port=rtp_session_get_local_rtcp_port(session);
}

static void _linphone_call_prepare_ice_for_stream(LinphoneCall *call, int stream_index, bool_t create_checklist){
	MediaStream *ms=stream_index == 0 ? (MediaStream*)call->audiostream : (MediaStream*)call->videostream;
	if ((linphone_core_get_firewall_policy(call->core) == LinphonePolicyUseIce) && (call->ice_session != NULL)){
		IceCheckList *cl;
		rtp_session_set_pktinfo(ms->sessions.rtp_session, TRUE);
		rtp_session_set_symmetric_rtp(ms->sessions.rtp_session, FALSE);
		cl=ice_session_check_list(call->ice_session, stream_index);
		if (cl == NULL && create_checklist) {
			cl=ice_check_list_new();
			ice_session_add_check_list(call->ice_session, cl);
			ms_message("Created new ICE check list for stream [%i]",stream_index);
		}
		if (cl){
			ms->ice_check_list = cl;
			ice_check_list_set_rtp_session(ms->ice_check_list, ms->sessions.rtp_session);
		}
	}
}

int linphone_call_prepare_ice(LinphoneCall *call, bool_t incoming_offer){
	SalMediaDescription *remote;
	bool_t has_video=FALSE;
	
	if ((linphone_core_get_firewall_policy(call->core) == LinphonePolicyUseIce) && (call->ice_session != NULL)){
		if (incoming_offer){
			remote=sal_call_get_remote_media_description(call->op);
			has_video=linphone_core_media_description_contains_video_stream(remote);
		}else has_video=call->params.has_video;
		
		_linphone_call_prepare_ice_for_stream(call,0,TRUE);
		if (has_video) _linphone_call_prepare_ice_for_stream(call,1,TRUE);
		/*start ICE gathering*/
		if (incoming_offer) 
			linphone_core_update_ice_from_remote_media_description(call,remote);
		if (!ice_session_candidates_gathered(call->ice_session)){
			if (call->audiostream->ms.state==MSStreamInitialized)
				audio_stream_prepare_sound(call->audiostream, NULL, NULL);
#ifdef VIDEO_ENABLED
			if (has_video && call->videostream && call->videostream->ms.state==MSStreamInitialized) {
				video_stream_prepare_video(call->videostream);
			}
#endif
			if (linphone_core_gather_ice_candidates(call->core,call)<0) {
				/* Ice candidates gathering failed, proceed with the call anyway. */
				linphone_call_delete_ice_session(call);
				linphone_call_stop_media_streams_for_ice_gathering(call);
				return -1;
			}
			return 1;/*gathering in progress, wait*/
		}
	}
	return 0;
}

void linphone_call_init_audio_stream(LinphoneCall *call){
	LinphoneCore *lc=call->core;
	AudioStream *audiostream;
	int dscp;

	if (call->audiostream != NULL) return;
	if (call->sessions[0].rtp_session==NULL){
		call->audiostream=audiostream=audio_stream_new(call->media_ports[0].rtp_port,call->media_ports[0].rtcp_port,call->af==AF_INET6);
	}else{
		call->audiostream=audio_stream_new_with_sessions(&call->sessions[0]);
	}
	audiostream=call->audiostream;
	if (call->media_ports[0].rtp_port==-1){
		port_config_set_random_choosed(call,0,audiostream->ms.sessions.rtp_session);
	}
	dscp=linphone_core_get_audio_dscp(lc);
	if (dscp!=-1)
		audio_stream_set_dscp(audiostream,dscp);
	if (linphone_core_echo_limiter_enabled(lc)){
		const char *type=lp_config_get_string(lc->config,"sound","el_type","mic");
		if (strcasecmp(type,"mic")==0)
			audio_stream_enable_echo_limiter(audiostream,ELControlMic);
		else if (strcasecmp(type,"full")==0)
			audio_stream_enable_echo_limiter(audiostream,ELControlFull);
	}
	audio_stream_enable_gain_control(audiostream,TRUE);
	if (linphone_core_echo_cancellation_enabled(lc)){
		int len,delay,framesize;
		const char *statestr=lp_config_get_string(lc->config,"sound","ec_state",NULL);
		len=lp_config_get_int(lc->config,"sound","ec_tail_len",0);
		delay=lp_config_get_int(lc->config,"sound","ec_delay",0);
		framesize=lp_config_get_int(lc->config,"sound","ec_framesize",0);
		audio_stream_set_echo_canceller_params(audiostream,len,delay,framesize);
		if (statestr && audiostream->ec){
			ms_filter_call_method(audiostream->ec,MS_ECHO_CANCELLER_SET_STATE_STRING,(void*)statestr);
		}
	}
	audio_stream_enable_automatic_gain_control(audiostream,linphone_core_agc_enabled(lc));
	{
		int enabled=lp_config_get_int(lc->config,"sound","noisegate",0);
		audio_stream_enable_noise_gate(audiostream,enabled);
	}

	audio_stream_set_features(audiostream,linphone_core_get_audio_features(lc));

	if (lc->rtptf){
		RtpTransport *artp=lc->rtptf->audio_rtp_func(lc->rtptf->audio_rtp_func_data, call->media_ports[0].rtp_port);
		RtpTransport *artcp=lc->rtptf->audio_rtcp_func(lc->rtptf->audio_rtcp_func_data, call->media_ports[0].rtcp_port);
		rtp_session_set_transports(audiostream->ms.sessions.rtp_session,artp,artcp);
	}

	call->audiostream_app_evq = ortp_ev_queue_new();
	rtp_session_register_event_queue(audiostream->ms.sessions.rtp_session,call->audiostream_app_evq);
	
	_linphone_call_prepare_ice_for_stream(call,0,FALSE);
}

void linphone_call_init_video_stream(LinphoneCall *call){
#ifdef VIDEO_ENABLED
	LinphoneCore *lc=call->core;

	if (call->videostream == NULL){
		int video_recv_buf_size=lp_config_get_int(lc->config,"video","recv_buf_size",0);
		int dscp=linphone_core_get_video_dscp(lc);
		const char *display_filter=linphone_core_get_video_display_filter(lc);
		
		if (call->sessions[1].rtp_session==NULL){
			call->videostream=video_stream_new(call->media_ports[1].rtp_port,call->media_ports[1].rtcp_port, call->af==AF_INET6);
		}else{
			call->videostream=video_stream_new_with_sessions(&call->sessions[1]);
		}
		if (call->media_ports[1].rtp_port==-1){
			port_config_set_random_choosed(call,1,call->videostream->ms.sessions.rtp_session);
		}
		if (dscp!=-1)
			video_stream_set_dscp(call->videostream,dscp);
		video_stream_enable_display_filter_auto_rotate(call->videostream, lp_config_get_int(lc->config,"video","display_filter_auto_rotate",0));
		if (video_recv_buf_size>0) rtp_session_set_recv_buf_size(call->videostream->ms.sessions.rtp_session,video_recv_buf_size);

		if (display_filter != NULL)
			video_stream_set_display_filter_name(call->videostream,display_filter);
		video_stream_set_event_callback(call->videostream,video_stream_event_cb, call);
		if (lc->rtptf){
			RtpTransport *vrtp=lc->rtptf->video_rtp_func(lc->rtptf->video_rtp_func_data, call->media_ports[1].rtp_port);
			RtpTransport *vrtcp=lc->rtptf->video_rtcp_func(lc->rtptf->video_rtcp_func_data, call->media_ports[1].rtcp_port);
			rtp_session_set_transports(call->videostream->ms.sessions.rtp_session,vrtp,vrtcp);
		}
			call->videostream_app_evq = ortp_ev_queue_new();
		rtp_session_register_event_queue(call->videostream->ms.sessions.rtp_session,call->videostream_app_evq);
		_linphone_call_prepare_ice_for_stream(call,1,FALSE);
#ifdef TEST_EXT_RENDERER
		video_stream_set_render_callback(call->videostream,rendercb,NULL);
#endif
	}
#else
	call->videostream=NULL;
#endif
}

void linphone_call_init_media_streams(LinphoneCall *call){
	linphone_call_init_audio_stream(call);
	linphone_call_init_video_stream(call);
}


static int dtmf_tab[16]={'0','1','2','3','4','5','6','7','8','9','*','#','A','B','C','D'};

static void linphone_core_dtmf_received(LinphoneCore *lc, int dtmf){
	if (dtmf<0 || dtmf>15){
		ms_warning("Bad dtmf value %i",dtmf);
		return;
	}
	if (lc->vtable.dtmf_received != NULL)
		lc->vtable.dtmf_received(lc, linphone_core_get_current_call(lc), dtmf_tab[dtmf]);
}

static void parametrize_equalizer(LinphoneCore *lc, AudioStream *st){
	if (st->equalizer){
		MSFilter *f=st->equalizer;
		int enabled=lp_config_get_int(lc->config,"sound","eq_active",0);
		const char *gains=lp_config_get_string(lc->config,"sound","eq_gains",NULL);
		ms_filter_call_method(f,MS_EQUALIZER_SET_ACTIVE,&enabled);
		if (enabled){
			if (gains){
				do{
					int bytes;
					MSEqualizerGain g;
					if (sscanf(gains,"%f:%f:%f %n",&g.frequency,&g.gain,&g.width,&bytes)==3){
						ms_message("Read equalizer gains: %f(~%f) --> %f",g.frequency,g.width,g.gain);
						ms_filter_call_method(f,MS_EQUALIZER_SET_GAIN,&g);
						gains+=bytes;
					}else break;
				}while(1);
			}
		}
	}
}

void _post_configure_audio_stream(AudioStream *st, LinphoneCore *lc, bool_t muted){
	float mic_gain=lc->sound_conf.soft_mic_lev;
	float thres = 0;
	float recv_gain;
	float ng_thres=lp_config_get_float(lc->config,"sound","ng_thres",0.05);
	float ng_floorgain=lp_config_get_float(lc->config,"sound","ng_floorgain",0);
	int dc_removal=lp_config_get_int(lc->config,"sound","dc_removal",0);
	float speed;
	float force;
	int sustain;
	float transmit_thres;
	MSFilter *f=NULL;
	float floorgain;
	int spk_agc;

	if (!muted)
		linphone_core_set_mic_gain_db (lc, mic_gain);
	else
		audio_stream_set_mic_gain(st,0);

	recv_gain = lc->sound_conf.soft_play_lev;
	if (recv_gain != 0) {
		linphone_core_set_playback_gain_db (lc,recv_gain);
	}

	if (st->volsend){
		ms_filter_call_method(st->volsend,MS_VOLUME_REMOVE_DC,&dc_removal);
		speed=lp_config_get_float(lc->config,"sound","el_speed",-1);
		thres=lp_config_get_float(lc->config,"sound","el_thres",-1);
		force=lp_config_get_float(lc->config,"sound","el_force",-1);
		sustain=lp_config_get_int(lc->config,"sound","el_sustain",-1);
		transmit_thres=lp_config_get_float(lc->config,"sound","el_transmit_thres",-1);
		f=st->volsend;
		if (speed==-1) speed=0.03;
		if (force==-1) force=25;
		ms_filter_call_method(f,MS_VOLUME_SET_EA_SPEED,&speed);
		ms_filter_call_method(f,MS_VOLUME_SET_EA_FORCE,&force);
		if (thres!=-1)
			ms_filter_call_method(f,MS_VOLUME_SET_EA_THRESHOLD,&thres);
		if (sustain!=-1)
			ms_filter_call_method(f,MS_VOLUME_SET_EA_SUSTAIN,&sustain);
		if (transmit_thres!=-1)
				ms_filter_call_method(f,MS_VOLUME_SET_EA_TRANSMIT_THRESHOLD,&transmit_thres);

		ms_filter_call_method(st->volsend,MS_VOLUME_SET_NOISE_GATE_THRESHOLD,&ng_thres);
		ms_filter_call_method(st->volsend,MS_VOLUME_SET_NOISE_GATE_FLOORGAIN,&ng_floorgain);
	}
	if (st->volrecv){
		/* parameters for a limited noise-gate effect, using echo limiter threshold */
		floorgain = 1/pow(10,(mic_gain)/10);
		spk_agc=lp_config_get_int(lc->config,"sound","speaker_agc_enabled",0);
		ms_filter_call_method(st->volrecv, MS_VOLUME_ENABLE_AGC, &spk_agc);
		ms_filter_call_method(st->volrecv,MS_VOLUME_SET_NOISE_GATE_THRESHOLD,&ng_thres);
		ms_filter_call_method(st->volrecv,MS_VOLUME_SET_NOISE_GATE_FLOORGAIN,&floorgain);
	}
	parametrize_equalizer(lc,st);
}

static void post_configure_audio_streams(LinphoneCall*call){
	AudioStream *st=call->audiostream;
	LinphoneCore *lc=call->core;
	_post_configure_audio_stream(st,lc,call->audio_muted);
	if (lc->vtable.dtmf_received!=NULL){
		audio_stream_play_received_dtmfs(call->audiostream,FALSE);
	}
	if (call->record_active)
		linphone_call_start_recording(call);
}

static RtpProfile *make_profile(LinphoneCall *call, const SalMediaDescription *md, const SalStreamDescription *desc, int *used_pt){
	int bw;
	const MSList *elem;
	RtpProfile *prof=rtp_profile_new("Call profile");
	bool_t first=TRUE;
	int remote_bw=0;
	LinphoneCore *lc=call->core;
	int up_ptime=0;
	const LinphoneCallParams *params=&call->params;
	*used_pt=-1;

	for(elem=desc->payloads;elem!=NULL;elem=elem->next){
		PayloadType *pt=(PayloadType*)elem->data;
		int number;
		/* make a copy of the payload type, so that we left the ones from the SalStreamDescription unchanged.
		 If the SalStreamDescription is freed, this will have no impact on the running streams*/
		pt=payload_type_clone(pt);

		if ((pt->flags & PAYLOAD_TYPE_FLAG_CAN_SEND) && first) {
			if (desc->type==SalAudio){
				linphone_core_update_allocated_audio_bandwidth_in_call(call,pt);
				if (params->up_ptime)
					up_ptime=params->up_ptime;
				else up_ptime=linphone_core_get_upload_ptime(lc);
			}
			*used_pt=payload_type_get_number(pt);
			first=FALSE;
		}
		if (desc->bandwidth>0) remote_bw=desc->bandwidth;
		else if (md->bandwidth>0) {
			/*case where b=AS is given globally, not per stream*/
			remote_bw=md->bandwidth;
			if (desc->type==SalVideo){
				remote_bw=get_video_bandwidth(remote_bw,call->audio_bw);
			}
		}

		if (desc->type==SalAudio){
			int audio_bw=call->audio_bw;
			if (params->up_bw){
				if (params->up_bw< audio_bw)
					audio_bw=params->up_bw;
			}
			bw=get_min_bandwidth(audio_bw,remote_bw);
		}else bw=get_min_bandwidth(get_video_bandwidth(linphone_core_get_upload_bandwidth (lc),call->audio_bw),remote_bw);
		if (bw>0) pt->normal_bitrate=bw*1000;
		else if (desc->type==SalAudio){
			pt->normal_bitrate=-1;
		}
		if (desc->ptime>0){
			up_ptime=desc->ptime;
		}
		if (up_ptime>0){
			char tmp[40];
			snprintf(tmp,sizeof(tmp),"ptime=%i",up_ptime);
			payload_type_append_send_fmtp(pt,tmp);
		}
		number=payload_type_get_number(pt);
		if (rtp_profile_get_payload(prof,number)!=NULL){
			ms_warning("A payload type with number %i already exists in profile !",number);
		}else
			rtp_profile_set_payload(prof,number,pt);
	}
	return prof;
}


static void setup_ring_player(LinphoneCore *lc, LinphoneCall *call){
	int pause_time=3000;
	audio_stream_play(call->audiostream,lc->sound_conf.ringback_tone);
	ms_filter_call_method(call->audiostream->soundread,MS_FILE_PLAYER_LOOP,&pause_time);
}

static bool_t linphone_call_sound_resources_available(LinphoneCall *call){
	LinphoneCore *lc=call->core;
	LinphoneCall *current=linphone_core_get_current_call(lc);
	return !linphone_core_is_in_conference(lc) &&
		(current==NULL || current==call);
}

static int find_crypto_index_from_tag(const SalSrtpCryptoAlgo crypto[],unsigned char tag) {
	int i;
	for(i=0; i<SAL_CRYPTO_ALGO_MAX; i++) {
		if (crypto[i].tag == tag) {
			return i;
		}
	}
	return -1;
}

static void configure_rtp_session_for_rtcp_xr(LinphoneCore *lc, LinphoneCall *call, SalStreamType type) {
	RtpSession *session;
	const OrtpRtcpXrConfiguration *localconfig;
	const OrtpRtcpXrConfiguration *remoteconfig;
	OrtpRtcpXrConfiguration currentconfig;
	const SalStreamDescription *localstream;
	const SalStreamDescription *remotestream;

	localstream = sal_media_description_find_stream(call->localdesc, SalProtoRtpSavp, type);
	if (!localstream) localstream = sal_media_description_find_stream(call->localdesc, SalProtoRtpAvp, type);
	if (!localstream) return;
	localconfig = &localstream->rtcp_xr;
	remotestream = sal_media_description_find_stream(sal_call_get_remote_media_description(call->op), SalProtoRtpSavp, type);
	if (!remotestream) remotestream = sal_media_description_find_stream(sal_call_get_remote_media_description(call->op), SalProtoRtpAvp, type);
	if (!remotestream) return;
	remoteconfig = &remotestream->rtcp_xr;

	if (localstream->dir == SalStreamInactive) return;
	else if (localstream->dir == SalStreamRecvOnly) {
		/* Use local config for unilateral parameters and remote config for collaborative parameters. */
		memcpy(&currentconfig, localconfig, sizeof(currentconfig));
		currentconfig.rcvr_rtt_mode = remoteconfig->rcvr_rtt_mode;
		currentconfig.rcvr_rtt_max_size = remoteconfig->rcvr_rtt_max_size;
	} else {
		memcpy(&currentconfig, remoteconfig, sizeof(currentconfig));
	}
	if (type == SalAudio) {
		session = call->audiostream->ms.sessions.rtp_session;
	} else {
		session = call->videostream->ms.sessions.rtp_session;
	}
	rtp_session_configure_rtcp_xr(session, &currentconfig);
	if (currentconfig.rcvr_rtt_mode != OrtpRtcpXrRcvrRttNone) {
		rtp_session_set_rtcp_xr_rcvr_rtt_interval(session, lp_config_get_int(lc->config, "rtp", "rtcp_xr_rcvr_rtt_interval_duration", 5000));
	}
	if (currentconfig.stat_summary_enabled == TRUE) {
		rtp_session_set_rtcp_xr_stat_summary_interval(session, lp_config_get_int(lc->config, "rtp", "rtcp_xr_stat_summary_interval_duration", 5000));
	}
	if (currentconfig.voip_metrics_enabled == TRUE) {
		rtp_session_set_rtcp_xr_voip_metrics_interval(session, lp_config_get_int(lc->config, "rtp", "rtcp_xr_voip_metrics_interval_duration", 5000));
	}
}

static void linphone_call_start_audio_stream(LinphoneCall *call, const char *cname, bool_t muted, bool_t send_ringbacktone, bool_t use_arc){
	LinphoneCore *lc=call->core;
	int used_pt=-1;
	char rtcp_tool[128]={0};
	const SalStreamDescription *stream;
	MSSndCard *playcard;
	MSSndCard *captcard;
	bool_t use_ec;
	bool_t mute;
	const char *playfile;
	const char *recfile;
	const SalStreamDescription *local_st_desc;
	int crypto_idx;

	snprintf(rtcp_tool,sizeof(rtcp_tool)-1,"%s-%s",linphone_core_get_user_agent_name(),linphone_core_get_user_agent_version());
	/* look for savp stream first */
	stream=sal_media_description_find_stream(call->resultdesc,
							SalProtoRtpSavp,SalAudio);
	/* no savp audio stream, use avp */
	if (!stream)
		stream=sal_media_description_find_stream(call->resultdesc,
							SalProtoRtpAvp,SalAudio);

	if (stream && stream->dir!=SalStreamInactive && stream->rtp_port!=0){
		playcard=lc->sound_conf.lsd_card ?
			lc->sound_conf.lsd_card : lc->sound_conf.play_sndcard;
		captcard=lc->sound_conf.capt_sndcard;
		playfile=lc->play_file;
		recfile=lc->rec_file;
		call->audio_profile=make_profile(call,call->resultdesc,stream,&used_pt);

		if (used_pt!=-1){
			call->current_params.audio_codec = rtp_profile_get_payload(call->audio_profile, used_pt);
			if (playcard==NULL) {
				ms_warning("No card defined for playback !");
			}
			if (captcard==NULL) {
				ms_warning("No card defined for capture !");
			}
			/*Replace soundcard filters by inactive file players or recorders
			 when placed in recvonly or sendonly mode*/
			if (stream->rtp_port==0 || stream->dir==SalStreamRecvOnly){
				captcard=NULL;
				playfile=NULL;
			}else if (stream->dir==SalStreamSendOnly){
				playcard=NULL;
				captcard=NULL;
				recfile=NULL;
				/*And we will eventually play "playfile" if set by the user*/
				/*playfile=NULL;*/
			}
			if (send_ringbacktone){
				captcard=NULL;
				playfile=NULL;/* it is setup later*/
			}
			/*if playfile are supplied don't use soundcards*/
			if (lc->use_files) {
				captcard=NULL;
				playcard=NULL;
			}
			if (call->params.in_conference){
				/* first create the graph without soundcard resources*/
				captcard=playcard=NULL;
			}
			if (!linphone_call_sound_resources_available(call)){
				ms_message("Sound resources are used by another call, not using soundcard.");
				captcard=playcard=NULL;
			}
			use_ec=captcard==NULL ? FALSE : linphone_core_echo_cancellation_enabled(lc);
			if (playcard &&  stream->max_rate>0) ms_snd_card_set_preferred_sample_rate(playcard, stream->max_rate);
			if (captcard &&  stream->max_rate>0) ms_snd_card_set_preferred_sample_rate(captcard, stream->max_rate);
			audio_stream_enable_adaptive_bitrate_control(call->audiostream,use_arc);
			audio_stream_enable_adaptive_jittcomp(call->audiostream, linphone_core_audio_adaptive_jittcomp_enabled(lc));
			if (!call->params.in_conference && call->params.record_file){
				audio_stream_mixed_record_open(call->audiostream,call->params.record_file);
				call->current_params.record_file=ms_strdup(call->params.record_file);
			}
			/* valid local tags are > 0 */
			if (stream->proto == SalProtoRtpSavp) {
				local_st_desc=sal_media_description_find_stream(call->localdesc,SalProtoRtpSavp,SalAudio);
				crypto_idx = find_crypto_index_from_tag(local_st_desc->crypto, stream->crypto_local_tag);

				if (crypto_idx >= 0) {
					media_stream_set_srtp_recv_key(&call->audiostream->ms,stream->crypto[0].algo,stream->crypto[0].master_key);
					media_stream_set_srtp_send_key(&call->audiostream->ms,stream->crypto[0].algo,local_st_desc->crypto[crypto_idx].master_key);
					call->audiostream_encrypted=TRUE;
				} else {
					ms_warning("Failed to find local crypto algo with tag: %d", stream->crypto_local_tag);
					call->audiostream_encrypted=FALSE;
				}
			}else call->audiostream_encrypted=FALSE;
			configure_rtp_session_for_rtcp_xr(lc, call, SalAudio);
			audio_stream_start_full(
				call->audiostream,
				call->audio_profile,
				stream->rtp_addr[0]!='\0' ? stream->rtp_addr : call->resultdesc->addr,
				stream->rtp_port,
				stream->rtcp_addr[0]!='\0' ? stream->rtcp_addr : call->resultdesc->addr,
				linphone_core_rtcp_enabled(lc) ? (stream->rtcp_port ? stream->rtcp_port : stream->rtp_port+1) : 0,
				used_pt,
				linphone_core_get_audio_jittcomp(lc),
				playfile,
				recfile,
				playcard,
				captcard,
				use_ec
				);
			post_configure_audio_streams(call);
			if (muted && !send_ringbacktone){
				audio_stream_set_mic_gain(call->audiostream,0);
			}
			if (stream->dir==SalStreamSendOnly && playfile!=NULL){
				int pause_time=500;
				ms_filter_call_method(call->audiostream->soundread,MS_FILE_PLAYER_LOOP,&pause_time);
			}
			if (send_ringbacktone){
				setup_ring_player(lc,call);
			}
			audio_stream_set_rtcp_information(call->audiostream, cname, rtcp_tool);

			if (call->params.in_conference){
				/*transform the graph to connect it to the conference filter */
				mute=stream->dir==SalStreamRecvOnly;
				linphone_call_add_to_conf(call, mute);
			}
			call->current_params.in_conference=call->params.in_conference;
			call->current_params.low_bandwidth=call->params.low_bandwidth;
		}else ms_warning("No audio stream accepted ?");
	}
}

static void linphone_call_start_video_stream(LinphoneCall *call, const char *cname,bool_t all_inputs_muted){
#ifdef VIDEO_ENABLED
	LinphoneCore *lc=call->core;
	int used_pt=-1;
	
	/* look for savp stream first */
	const SalStreamDescription *vstream=sal_media_description_find_stream(call->resultdesc,
							SalProtoRtpSavp,SalVideo);
	char rtcp_tool[128]={0};
	snprintf(rtcp_tool,sizeof(rtcp_tool)-1,"%s-%s",linphone_core_get_user_agent_name(),linphone_core_get_user_agent_version());

	/* no savp audio stream, use avp */
	if (!vstream)
		vstream=sal_media_description_find_stream(call->resultdesc,
							SalProtoRtpAvp,SalVideo);

	/* shutdown preview */
	if (lc->previewstream!=NULL) {
		video_preview_stop(lc->previewstream);
		lc->previewstream=NULL;
	}

	if (vstream!=NULL && vstream->dir!=SalStreamInactive && vstream->rtp_port!=0) {
		const char *rtp_addr=vstream->rtp_addr[0]!='\0' ? vstream->rtp_addr : call->resultdesc->addr;
		const char *rtcp_addr=vstream->rtcp_addr[0]!='\0' ? vstream->rtcp_addr : call->resultdesc->addr;
		const SalStreamDescription *local_st_desc=sal_media_description_find_stream(call->localdesc,vstream->proto,SalVideo);
		
		call->video_profile=make_profile(call,call->resultdesc,vstream,&used_pt);

		if (used_pt!=-1){
			VideoStreamDir dir=VideoStreamSendRecv;
			MSWebCam *cam=lc->video_conf.device;
			bool_t is_inactive=FALSE;

			call->current_params.video_codec = rtp_profile_get_payload(call->video_profile, used_pt);
			call->current_params.has_video=TRUE;

			video_stream_enable_adaptive_bitrate_control(call->videostream,
													  linphone_core_adaptive_rate_control_enabled(lc));
			video_stream_enable_adaptive_jittcomp(call->videostream, linphone_core_video_adaptive_jittcomp_enabled(lc));
			video_stream_set_sent_video_size(call->videostream,linphone_core_get_preferred_video_size(lc));
			video_stream_enable_self_view(call->videostream,lc->video_conf.selfview);
			if (lc->video_window_id!=0)
				video_stream_set_native_window_id(call->videostream,lc->video_window_id);
			if (lc->preview_window_id!=0)
				video_stream_set_native_preview_window_id (call->videostream,lc->preview_window_id);
			video_stream_use_preview_video_window (call->videostream,lc->use_preview_window);

			if (vstream->dir==SalStreamSendOnly && lc->video_conf.capture ){
				if (local_st_desc->dir==SalStreamSendOnly){
					/* localdesc stream dir to SendOnly is when we want to put on hold, so use nowebcam in this case*/
					cam=get_nowebcam_device();
				}
				dir=VideoStreamSendOnly;
			}else if (vstream->dir==SalStreamRecvOnly && lc->video_conf.display ){
				dir=VideoStreamRecvOnly;
			}else if (vstream->dir==SalStreamSendRecv){
				if (lc->video_conf.display && lc->video_conf.capture)
					dir=VideoStreamSendRecv;
				else if (lc->video_conf.display)
					dir=VideoStreamRecvOnly;
				else
					dir=VideoStreamSendOnly;
			}else{
				ms_warning("video stream is inactive.");
				/*either inactive or incompatible with local capabilities*/
				is_inactive=TRUE;
			}
			if (call->camera_enabled==FALSE || all_inputs_muted){
				cam=get_nowebcam_device();
			}
			if (!is_inactive){
				if (vstream->proto == SalProtoRtpSavp) {
					int crypto_idx = find_crypto_index_from_tag(local_st_desc->crypto, vstream->crypto_local_tag);

					if (crypto_idx >= 0) {
						media_stream_set_srtp_recv_key(&call->videostream->ms,vstream->crypto[0].algo,vstream->crypto[0].master_key);
						media_stream_set_srtp_send_key(&call->videostream->ms,vstream->crypto[0].algo,local_st_desc->crypto[crypto_idx].master_key);
						call->videostream_encrypted=TRUE;
					}else call->videostream_encrypted=FALSE;
				}else{
					call->videostream_encrypted=FALSE;
				}
				configure_rtp_session_for_rtcp_xr(lc, call, SalVideo);

				call->log->video_enabled = TRUE;
				video_stream_set_direction (call->videostream, dir);
				ms_message("%s lc rotation:%d\n", __FUNCTION__, lc->device_rotation);
				video_stream_set_device_rotation(call->videostream, lc->device_rotation);
				video_stream_start(call->videostream,
					call->video_profile, rtp_addr, vstream->rtp_port,
					rtcp_addr,
					linphone_core_rtcp_enabled(lc) ? (vstream->rtcp_port ? vstream->rtcp_port : vstream->rtp_port+1) : 0,
					used_pt, linphone_core_get_video_jittcomp(lc), cam);
				video_stream_set_rtcp_information(call->videostream, cname,rtcp_tool);
			}
		}else ms_warning("No video stream accepted.");
	}else{
		ms_warning("No valid video stream defined.");
	}
#endif
}

void linphone_call_start_media_streams(LinphoneCall *call, bool_t all_inputs_muted, bool_t send_ringbacktone){
	LinphoneCore *lc=call->core;
	LinphoneAddress *me=linphone_core_get_primary_contact_parsed(lc);
	char *cname;
	bool_t use_arc=linphone_core_adaptive_rate_control_enabled(lc);
#ifdef VIDEO_ENABLED
	const SalStreamDescription *vstream=sal_media_description_find_stream(call->resultdesc,
								SalProtoRtpAvp,SalVideo);
#endif

	call->current_params.audio_codec = NULL;
	call->current_params.video_codec = NULL;

	if ((call->audiostream == NULL) && (call->videostream == NULL)) {
		ms_fatal("start_media_stream() called without prior init !");
		return;
	}
	cname=linphone_address_as_string_uri_only(me);

#if defined(VIDEO_ENABLED)
	if (vstream!=NULL && vstream->dir!=SalStreamInactive && vstream->payloads!=NULL){
		/*when video is used, do not make adaptive rate control on audio, it is stupid.*/
		use_arc=FALSE;
	}
#endif
	if (call->audiostream!=NULL) {
		linphone_call_start_audio_stream(call,cname,all_inputs_muted,send_ringbacktone,use_arc);
	}
	call->current_params.has_video=FALSE;
	if (call->videostream!=NULL) {
		linphone_call_start_video_stream(call,cname,all_inputs_muted);
	}

	call->all_muted=all_inputs_muted;
	call->playing_ringbacktone=send_ringbacktone;
	call->up_bw=linphone_core_get_upload_bandwidth(lc);

	if (call->params.media_encryption==LinphoneMediaEncryptionZRTP) {
		OrtpZrtpParams params;
		/*will be set later when zrtp is activated*/
		call->current_params.media_encryption=LinphoneMediaEncryptionNone;

		params.zid_file=lc->zrtp_secrets_cache;
		audio_stream_enable_zrtp(call->audiostream,&params);
	}else{
		call->current_params.media_encryption=linphone_call_are_all_streams_encrypted(call) ?
			LinphoneMediaEncryptionSRTP : LinphoneMediaEncryptionNone;
	}

	if ((call->ice_session != NULL) && (ice_session_state(call->ice_session) != IS_Completed)) {
		ice_session_start_connectivity_checks(call->ice_session);
	}

	goto end;
	end:
		ms_free(cname);
		linphone_address_destroy(me);
}

void linphone_call_stop_media_streams_for_ice_gathering(LinphoneCall *call){
	if(call->audiostream && call->audiostream->ms.state==MSStreamPreparing) audio_stream_unprepare_sound(call->audiostream);
#ifdef VIDEO_ENABLED
	if (call->videostream && call->videostream->ms.state==MSStreamPreparing) {
		video_stream_unprepare_video(call->videostream);
	}
#endif
}

static bool_t update_stream_crypto_params(LinphoneCall *call, const SalStreamDescription *local_st_desc, SalStreamDescription *old_stream, SalStreamDescription *new_stream, MediaStream *ms){
	int crypto_idx = find_crypto_index_from_tag(local_st_desc->crypto, new_stream->crypto_local_tag);
	if (crypto_idx >= 0) {
		if (call->localdesc_changed & SAL_MEDIA_DESCRIPTION_CRYPTO_CHANGED)
			media_stream_set_srtp_send_key(ms,new_stream->crypto[0].algo,local_st_desc->crypto[crypto_idx].master_key);
		if (strcmp(old_stream->crypto[0].master_key,new_stream->crypto[0].master_key)!=0){
			media_stream_set_srtp_recv_key(ms,new_stream->crypto[0].algo,new_stream->crypto[0].master_key);
		}
		return TRUE;
	} else {
		ms_warning("Failed to find local crypto algo with tag: %d", new_stream->crypto_local_tag);
	}
	return FALSE;
}

void linphone_call_update_crypto_parameters(LinphoneCall *call, SalMediaDescription *old_md, SalMediaDescription *new_md) {
	SalStreamDescription *old_stream;
	SalStreamDescription *new_stream;
	const SalStreamDescription *local_st_desc;
	
	local_st_desc = sal_media_description_find_stream(call->localdesc, SalProtoRtpSavp, SalAudio);
	old_stream = sal_media_description_find_stream(old_md, SalProtoRtpSavp, SalAudio);
	new_stream = sal_media_description_find_stream(new_md, SalProtoRtpSavp, SalAudio);
	if (call->audiostream && local_st_desc && old_stream && new_stream &&
		update_stream_crypto_params(call,local_st_desc,old_stream,new_stream,&call->audiostream->ms)){
		call->audiostream_encrypted = TRUE;
	}else call->audiostream_encrypted = FALSE;

#ifdef VIDEO_ENABLED
	local_st_desc = sal_media_description_find_stream(call->localdesc, SalProtoRtpSavp, SalVideo);
	old_stream = sal_media_description_find_stream(old_md, SalProtoRtpSavp, SalVideo);
	new_stream = sal_media_description_find_stream(new_md, SalProtoRtpSavp, SalVideo);
	if (call->videostream && local_st_desc && old_stream && new_stream &&
		update_stream_crypto_params(call,local_st_desc,old_stream,new_stream,&call->videostream->ms)){
		call->videostream_encrypted = TRUE;
	}
	call->videostream_encrypted = FALSE;
#endif
}

void linphone_call_update_remote_session_id_and_ver(LinphoneCall *call) {
	SalMediaDescription *remote_desc = sal_call_get_remote_media_description(call->op);
	if (remote_desc) {
		call->remote_session_id = remote_desc->session_id;
		call->remote_session_ver = remote_desc->session_ver;
	}
}

void linphone_call_delete_ice_session(LinphoneCall *call){
	if (call->ice_session != NULL) {
		ice_session_destroy(call->ice_session);
		call->ice_session = NULL;
		if (call->audiostream != NULL) call->audiostream->ms.ice_check_list = NULL;
		if (call->videostream != NULL) call->videostream->ms.ice_check_list = NULL;
		call->stats[LINPHONE_CALL_STATS_AUDIO].ice_state = LinphoneIceStateNotActivated;
		call->stats[LINPHONE_CALL_STATS_VIDEO].ice_state = LinphoneIceStateNotActivated;
	}
}


void linphone_call_delete_upnp_session(LinphoneCall *call){
#ifdef BUILD_UPNP
	if(call->upnp_session!=NULL) {
		linphone_upnp_session_destroy(call->upnp_session);
		call->upnp_session=NULL;
	}
#endif //BUILD_UPNP
}


static void linphone_call_log_fill_stats(LinphoneCallLog *log, MediaStream *st){
	float quality=media_stream_get_average_quality_rating(st);
	if (quality>=0){
		if (log->quality!=-1){
			log->quality*=quality/5.0;
		}else log->quality=quality;
	}
}

void linphone_call_stop_audio_stream(LinphoneCall *call) {
	if (call->audiostream!=NULL) {
		linphone_reporting_update(call, LINPHONE_CALL_STATS_AUDIO);
		media_stream_reclaim_sessions(&call->audiostream->ms,&call->sessions[0]);
		rtp_session_unregister_event_queue(call->audiostream->ms.sessions.rtp_session,call->audiostream_app_evq);
		ortp_ev_queue_flush(call->audiostream_app_evq);
		ortp_ev_queue_destroy(call->audiostream_app_evq);
		call->audiostream_app_evq=NULL;

		if (call->audiostream->ec){
			const char *state_str=NULL;
			ms_filter_call_method(call->audiostream->ec,MS_ECHO_CANCELLER_GET_STATE_STRING,&state_str);
			if (state_str){
				ms_message("Writing echo canceler state, %i bytes",(int)strlen(state_str));
				lp_config_set_string(call->core->config,"sound","ec_state",state_str);
			}
		}
		audio_stream_get_local_rtp_stats(call->audiostream,&call->log->local_stats);
		linphone_call_log_fill_stats (call->log,(MediaStream*)call->audiostream);
		if (call->endpoint){
			linphone_call_remove_from_conf(call);
		}
		audio_stream_stop(call->audiostream);
		call->audiostream=NULL;
		call->current_params.audio_codec = NULL;
	}
}

void linphone_call_stop_video_stream(LinphoneCall *call) {
#ifdef VIDEO_ENABLED
	if (call->videostream!=NULL){
		linphone_reporting_update(call, LINPHONE_CALL_STATS_VIDEO);
		media_stream_reclaim_sessions(&call->videostream->ms,&call->sessions[1]);
		rtp_session_unregister_event_queue(call->videostream->ms.sessions.rtp_session,call->videostream_app_evq);
		ortp_ev_queue_flush(call->videostream_app_evq);
		ortp_ev_queue_destroy(call->videostream_app_evq);
		call->videostream_app_evq=NULL;
		linphone_call_log_fill_stats(call->log,(MediaStream*)call->videostream);
		video_stream_stop(call->videostream);
		call->videostream=NULL;
		call->current_params.video_codec = NULL;
	}
#endif
}

static void unset_rtp_profile(LinphoneCall *call, int i){
	if (call->sessions[i].rtp_session)
		rtp_session_set_profile(call->sessions[i].rtp_session,&av_profile);
}

void linphone_call_stop_media_streams(LinphoneCall *call){
	if (call->audiostream || call->videostream) {
		linphone_call_stop_audio_stream(call);
		linphone_call_stop_video_stream(call);

		if (call->core->msevq != NULL) {
			ms_event_queue_skip(call->core->msevq);
		}
	}

	if (call->audio_profile){
		rtp_profile_destroy(call->audio_profile);
		call->audio_profile=NULL;
		unset_rtp_profile(call,0);
	}
	if (call->video_profile){
		rtp_profile_destroy(call->video_profile);
		call->video_profile=NULL;
		unset_rtp_profile(call,1);
	}
}



void linphone_call_enable_echo_cancellation(LinphoneCall *call, bool_t enable) {
	if (call!=NULL && call->audiostream!=NULL && call->audiostream->ec){
		bool_t bypass_mode = !enable;
		ms_filter_call_method(call->audiostream->ec,MS_ECHO_CANCELLER_SET_BYPASS_MODE,&bypass_mode);
	}
}
bool_t linphone_call_echo_cancellation_enabled(LinphoneCall *call) {
	if (call!=NULL && call->audiostream!=NULL && call->audiostream->ec){
		bool_t val;
		ms_filter_call_method(call->audiostream->ec,MS_ECHO_CANCELLER_GET_BYPASS_MODE,&val);
		return !val;
	} else {
		return linphone_core_echo_cancellation_enabled(call->core);
	}
}

void linphone_call_enable_echo_limiter(LinphoneCall *call, bool_t val){
	if (call!=NULL && call->audiostream!=NULL ) {
		if (val) {
		const char *type=lp_config_get_string(call->core->config,"sound","el_type","mic");
		if (strcasecmp(type,"mic")==0)
			audio_stream_enable_echo_limiter(call->audiostream,ELControlMic);
		else if (strcasecmp(type,"full")==0)
			audio_stream_enable_echo_limiter(call->audiostream,ELControlFull);
		} else {
			audio_stream_enable_echo_limiter(call->audiostream,ELInactive);
		}
	}
}

bool_t linphone_call_echo_limiter_enabled(const LinphoneCall *call){
	if (call!=NULL && call->audiostream!=NULL ){
		return call->audiostream->el_type !=ELInactive ;
	} else {
		return linphone_core_echo_limiter_enabled(call->core);
	}
}

/**
 * @addtogroup call_misc
 * @{
**/

/**
 * Returns the measured sound volume played locally (received from remote).
 * It is expressed in dbm0.
**/
float linphone_call_get_play_volume(LinphoneCall *call){
	AudioStream *st=call->audiostream;
	if (st && st->volrecv){
		float vol=0;
		ms_filter_call_method(st->volrecv,MS_VOLUME_GET,&vol);
		return vol;

	}
	return LINPHONE_VOLUME_DB_LOWEST;
}

/**
 * Returns the measured sound volume recorded locally (sent to remote).
 * It is expressed in dbm0.
**/
float linphone_call_get_record_volume(LinphoneCall *call){
	AudioStream *st=call->audiostream;
	if (st && st->volsend && !call->audio_muted && call->state==LinphoneCallStreamsRunning){
		float vol=0;
		ms_filter_call_method(st->volsend,MS_VOLUME_GET,&vol);
		return vol;

	}
	return LINPHONE_VOLUME_DB_LOWEST;
}

/**
 * Obtain real-time quality rating of the call
 *
 * Based on local RTP statistics and RTCP feedback, a quality rating is computed and updated
 * during all the duration of the call. This function returns its value at the time of the function call.
 * It is expected that the rating is updated at least every 5 seconds or so.
 * The rating is a floating point number comprised between 0 and 5.
 *
 * 4-5 = good quality <br>
 * 3-4 = average quality <br>
 * 2-3 = poor quality <br>
 * 1-2 = very poor quality <br>
 * 0-1 = can't be worse, mostly unusable <br>
 *
 * @returns The function returns -1 if no quality measurement is available, for example if no
 * active audio stream exist. Otherwise it returns the quality rating.
**/
float linphone_call_get_current_quality(LinphoneCall *call){
	float audio_rating=-1;
	float video_rating=-1;
	float result;
	if (call->audiostream){
		audio_rating=media_stream_get_quality_rating((MediaStream*)call->audiostream)/5.0;
	}
	if (call->videostream){
		video_rating=media_stream_get_quality_rating((MediaStream*)call->videostream)/5.0;
	}
	if (audio_rating<0 && video_rating<0) result=-1;
	else if (audio_rating<0) result=video_rating*5.0;
	else if (video_rating<0) result=audio_rating*5.0;
	else result=audio_rating*video_rating*5.0;
	return result;
}

/**
 * Returns call quality averaged over all the duration of the call.
 *
 * See linphone_call_get_current_quality() for more details about quality measurement.
**/
float linphone_call_get_average_quality(LinphoneCall *call){
	if (call->audiostream){
		return audio_stream_get_average_quality_rating(call->audiostream);
	}
	return -1;
}

static void update_local_stats(LinphoneCallStats *stats, MediaStream *stream){
	const MSQualityIndicator *qi=media_stream_get_quality_indicator(stream);
	if (qi) {
		stats->local_late_rate=ms_quality_indicator_get_local_late_rate(qi);
		stats->local_loss_rate=ms_quality_indicator_get_local_loss_rate(qi);
	}
}

/**
 * Access last known statistics for audio stream, for a given call.
**/
const LinphoneCallStats *linphone_call_get_audio_stats(LinphoneCall *call) {
	LinphoneCallStats *stats=&call->stats[LINPHONE_CALL_STATS_AUDIO];
	if (call->audiostream){
		update_local_stats(stats,(MediaStream*)call->audiostream);
	}
	return stats;
}

/**
 * Access last known statistics for video stream, for a given call.
**/
const LinphoneCallStats *linphone_call_get_video_stats(LinphoneCall *call) {
	LinphoneCallStats *stats=&call->stats[LINPHONE_CALL_STATS_VIDEO];
	if (call->videostream){
		update_local_stats(stats,(MediaStream*)call->videostream);
	}
	return stats;
}

/**
 * Get the local loss rate since last report
 * @return The sender loss rate
**/
float linphone_call_stats_get_sender_loss_rate(const LinphoneCallStats *stats) {
	const report_block_t *srb = NULL;

	if (!stats || !stats->sent_rtcp)
		return 0.0;
	/* Perform msgpullup() to prevent crashes in rtcp_is_SR() or rtcp_is_RR() if the RTCP packet is composed of several mblk_t structure */
	if (stats->sent_rtcp->b_cont != NULL)
		msgpullup(stats->sent_rtcp, -1);
	if (rtcp_is_SR(stats->sent_rtcp))
		srb = rtcp_SR_get_report_block(stats->sent_rtcp, 0);
	else if (rtcp_is_RR(stats->sent_rtcp))
		srb = rtcp_RR_get_report_block(stats->sent_rtcp, 0);
	if (!srb)
		return 0.0;
	return 100.0 * report_block_get_fraction_lost(srb) / 256.0;
}

/**
 * Gets the remote reported loss rate since last report
 * @return The receiver loss rate
**/
float linphone_call_stats_get_receiver_loss_rate(const LinphoneCallStats *stats) {
	const report_block_t *rrb = NULL;

	if (!stats || !stats->received_rtcp)
		return 0.0;
	/* Perform msgpullup() to prevent crashes in rtcp_is_SR() or rtcp_is_RR() if the RTCP packet is composed of several mblk_t structure */
	if (stats->received_rtcp->b_cont != NULL)
		msgpullup(stats->received_rtcp, -1);
	if (rtcp_is_RR(stats->received_rtcp))
		rrb = rtcp_RR_get_report_block(stats->received_rtcp, 0);
	else if (rtcp_is_SR(stats->received_rtcp))
		rrb = rtcp_SR_get_report_block(stats->received_rtcp, 0);
	if (!rrb)
		return 0.0;
	return 100.0 * report_block_get_fraction_lost(rrb) / 256.0;
}

/**
 * Gets the local interarrival jitter
 * @return The interarrival jitter at last emitted sender report
**/
float linphone_call_stats_get_sender_interarrival_jitter(const LinphoneCallStats *stats, LinphoneCall *call) {
	const LinphoneCallParams *params;
	const PayloadType *pt;
	const report_block_t *srb = NULL;

	if (!stats || !call || !stats->sent_rtcp)
		return 0.0;
	params = linphone_call_get_current_params(call);
	if (!params)
		return 0.0;
	/* Perform msgpullup() to prevent crashes in rtcp_is_SR() or rtcp_is_RR() if the RTCP packet is composed of several mblk_t structure */
	if (stats->sent_rtcp->b_cont != NULL)
		msgpullup(stats->sent_rtcp, -1);
	if (rtcp_is_SR(stats->sent_rtcp))
		srb = rtcp_SR_get_report_block(stats->sent_rtcp, 0);
	else if (rtcp_is_RR(stats->sent_rtcp))
		srb = rtcp_RR_get_report_block(stats->sent_rtcp, 0);
	if (!srb)
		return 0.0;
	if (stats->type == LINPHONE_CALL_STATS_AUDIO)
		pt = linphone_call_params_get_used_audio_codec(params);
	else
		pt = linphone_call_params_get_used_video_codec(params);
	if (!pt || (pt->clock_rate == 0))
		return 0.0;
	return (float)report_block_get_interarrival_jitter(srb) / (float)pt->clock_rate;
}

/**
 * Gets the remote reported interarrival jitter
 * @return The interarrival jitter at last received receiver report
**/
float linphone_call_stats_get_receiver_interarrival_jitter(const LinphoneCallStats *stats, LinphoneCall *call) {
	const LinphoneCallParams *params;
	const PayloadType *pt;
	const report_block_t *rrb = NULL;

	if (!stats || !call || !stats->received_rtcp)
		return 0.0;
	params = linphone_call_get_current_params(call);
	if (!params)
		return 0.0;
	/* Perform msgpullup() to prevent crashes in rtcp_is_SR() or rtcp_is_RR() if the RTCP packet is composed of several mblk_t structure */
	if (stats->received_rtcp->b_cont != NULL)
		msgpullup(stats->received_rtcp, -1);
	if (rtcp_is_SR(stats->received_rtcp))
		rrb = rtcp_SR_get_report_block(stats->received_rtcp, 0);
	else if (rtcp_is_RR(stats->received_rtcp))
		rrb = rtcp_RR_get_report_block(stats->received_rtcp, 0);
	if (!rrb)
		return 0.0;
	if (stats->type == LINPHONE_CALL_STATS_AUDIO)
		pt = linphone_call_params_get_used_audio_codec(params);
	else
		pt = linphone_call_params_get_used_video_codec(params);
	if (!pt || (pt->clock_rate == 0))
		return 0.0;
	return (float)report_block_get_interarrival_jitter(rrb) / (float)pt->clock_rate;
}

/**
 * Gets the cumulative number of late packets
 * @return The cumulative number of late packets
**/
uint64_t linphone_call_stats_get_late_packets_cumulative_number(const LinphoneCallStats *stats, LinphoneCall *call) {
	rtp_stats_t rtp_stats;

	if (!stats || !call)
		return 0;
	memset(&rtp_stats, 0, sizeof(rtp_stats));
	if (stats->type == LINPHONE_CALL_STATS_AUDIO)
		audio_stream_get_local_rtp_stats(call->audiostream, &rtp_stats);
#ifdef VIDEO_ENABLED
	else
		video_stream_get_local_rtp_stats(call->videostream, &rtp_stats);
#endif
	return rtp_stats.outoftime;
}

/**
 * Enable recording of the call (voice-only).
 * This function must be used before the call parameters are assigned to the call.
 * The call recording can be started and paused after the call is established with
 * linphone_call_start_recording() and linphone_call_pause_recording().
 * @param cp the call parameters
 * @param path path and filename of the file where audio is written.
**/
void linphone_call_params_set_record_file(LinphoneCallParams *cp, const char *path){
	if (cp->record_file){
		ms_free(cp->record_file);
		cp->record_file=NULL;
	}
	if (path) cp->record_file=ms_strdup(path);
}

/**
 * Retrieves the path for the audio recoding of the call.
**/
const char *linphone_call_params_get_record_file(const LinphoneCallParams *cp){
	return cp->record_file;
}

/**
 * Start call recording.
 * The output file where audio is recorded must be previously specified with linphone_call_params_set_record_file().
**/
void linphone_call_start_recording(LinphoneCall *call){
	if (!call->params.record_file){
		ms_error("linphone_call_start_recording(): no output file specified. Use linphone_call_params_set_record_file().");
		return;
	}
	if (call->audiostream && !call->params.in_conference){
		audio_stream_mixed_record_start(call->audiostream);
	}
	call->record_active=TRUE;
}

/**
 * Stop call recording.
**/
void linphone_call_stop_recording(LinphoneCall *call){
	if (call->audiostream && !call->params.in_conference){
		audio_stream_mixed_record_stop(call->audiostream);
	}
	call->record_active=FALSE;
}

/**
 * @}
**/

static void report_bandwidth(LinphoneCall *call, MediaStream *as, MediaStream *vs){
	call->stats[LINPHONE_CALL_STATS_AUDIO].download_bandwidth=(as!=NULL) ? (media_stream_get_down_bw(as)*1e-3) : 0;
	call->stats[LINPHONE_CALL_STATS_AUDIO].upload_bandwidth=(as!=NULL) ? (media_stream_get_up_bw(as)*1e-3) : 0;
	call->stats[LINPHONE_CALL_STATS_VIDEO].download_bandwidth=(vs!=NULL) ? (media_stream_get_down_bw(vs)*1e-3) : 0;
	call->stats[LINPHONE_CALL_STATS_VIDEO].upload_bandwidth=(vs!=NULL) ? (media_stream_get_up_bw(vs)*1e-3) : 0;
	ms_message("bandwidth usage for call [%p]: audio=[d=%.1f,u=%.1f] video=[d=%.1f,u=%.1f] kbit/sec",
		call,
		call->stats[LINPHONE_CALL_STATS_AUDIO].download_bandwidth,
		call->stats[LINPHONE_CALL_STATS_AUDIO].upload_bandwidth ,
		call->stats[LINPHONE_CALL_STATS_VIDEO].download_bandwidth,
		call->stats[LINPHONE_CALL_STATS_VIDEO].upload_bandwidth
	);
}

static void linphone_core_disconnected(LinphoneCore *lc, LinphoneCall *call){
	char temp[256];
	char *from=NULL;
	if(call)
		from = linphone_call_get_remote_address_as_string(call);
	if (from)
	{
		snprintf(temp,sizeof(temp),"Remote end %s seems to have disconnected, the call is going to be closed.",from);
		ms_free(from);
	}
	else
	{
		snprintf(temp,sizeof(temp),"Remote end seems to have disconnected, the call is going to be closed.");
	}
	ms_message("On call [%p] %s",call,temp);
	if (lc->vtable.display_warning!=NULL)
		lc->vtable.display_warning(lc,temp);
	linphone_core_terminate_call(lc,call);
	linphone_core_play_named_tone(lc,LinphoneToneCallLost);
}

static void handle_ice_events(LinphoneCall *call, OrtpEvent *ev){
	OrtpEventType evt=ortp_event_get_type(ev);
	OrtpEventData *evd=ortp_event_get_data(ev);
	int ping_time;

	if (evt == ORTP_EVENT_ICE_SESSION_PROCESSING_FINISHED) {
		switch (ice_session_state(call->ice_session)) {
			case IS_Completed:
				ice_session_select_candidates(call->ice_session);
				if (ice_session_role(call->ice_session) == IR_Controlling) {
					linphone_core_update_call(call->core, call, &call->current_params);
				}
				break;
			case IS_Failed:
				if (ice_session_has_completed_check_list(call->ice_session) == TRUE) {
					ice_session_select_candidates(call->ice_session);
					if (ice_session_role(call->ice_session) == IR_Controlling) {
						/* At least one ICE session has succeeded, so perform a call update. */
						linphone_core_update_call(call->core, call, &call->current_params);
					}
				}
				break;
			default:
				break;
		}
		linphone_core_update_ice_state_in_call_stats(call);
	} else if (evt == ORTP_EVENT_ICE_GATHERING_FINISHED) {

		if (evd->info.ice_processing_successful==TRUE) {
			ice_session_compute_candidates_foundations(call->ice_session);
			ice_session_eliminate_redundant_candidates(call->ice_session);
			ice_session_choose_default_candidates(call->ice_session);
			ping_time = ice_session_average_gathering_round_trip_time(call->ice_session);
			if (ping_time >=0) {
				call->ping_time=ping_time;
			}
		} else {
			ms_warning("No STUN answer from [%s], disabling ICE",linphone_core_get_stun_server(call->core));
			linphone_call_delete_ice_session(call);
		}
		switch (call->state) {
			case LinphoneCallUpdating:
				linphone_core_start_update_call(call->core, call);
				break;
			case LinphoneCallUpdatedByRemote:
				linphone_core_start_accept_call_update(call->core, call);
				break;
			case LinphoneCallOutgoingInit:
				linphone_call_stop_media_streams_for_ice_gathering(call);
				linphone_core_proceed_with_invite_if_ready(call->core, call, NULL);
				break;
			case LinphoneCallIdle:
				linphone_call_stop_media_streams_for_ice_gathering(call);
				linphone_core_notify_incoming_call(call->core, call);
				break;
			default:
				break;
		}
	} else if (evt == ORTP_EVENT_ICE_LOSING_PAIRS_COMPLETED) {
		if (call->state==LinphoneCallUpdatedByRemote){
			linphone_core_start_accept_call_update(call->core, call);
			linphone_core_update_ice_state_in_call_stats(call);
		}
	} else if (evt == ORTP_EVENT_ICE_RESTART_NEEDED) {
		ice_session_restart(call->ice_session);
		ice_session_set_role(call->ice_session, IR_Controlling);
		linphone_core_update_call(call->core, call, &call->current_params);
	}
}

void linphone_call_background_tasks(LinphoneCall *call, bool_t one_second_elapsed){
	LinphoneCore* lc = call->core;
	int disconnect_timeout = linphone_core_get_nortp_timeout(call->core);
	bool_t disconnected=FALSE;

	if ((call->state==LinphoneCallStreamsRunning || call->state==LinphoneCallOutgoingEarlyMedia || call->state==LinphoneCallIncomingEarlyMedia) && one_second_elapsed){
		float audio_load=0, video_load=0;
		if (call->audiostream!=NULL){
			if (call->audiostream->ms.sessions.ticker)
				audio_load=ms_ticker_get_average_load(call->audiostream->ms.sessions.ticker);
		}
		if (call->videostream!=NULL){
			if (call->videostream->ms.sessions.ticker)
				video_load=ms_ticker_get_average_load(call->videostream->ms.sessions.ticker);
		}
		report_bandwidth(call,(MediaStream*)call->audiostream,(MediaStream*)call->videostream);
		ms_message("Thread processing load: audio=%f\tvideo=%f",audio_load,video_load);
	}

#ifdef BUILD_UPNP
	linphone_upnp_call_process(call);
#endif //BUILD_UPNP

#ifdef VIDEO_ENABLED
	if (call->videostream!=NULL) {
		OrtpEvent *ev;

		/* Ensure there is no dangling ICE check list. */
		if (call->ice_session == NULL) call->videostream->ms.ice_check_list = NULL;

		// Beware that the application queue should not depend on treatments fron the
		// mediastreamer queue.
		video_stream_iterate(call->videostream);

		while (call->videostream_app_evq && (NULL != (ev=ortp_ev_queue_get(call->videostream_app_evq)))){
			OrtpEventType evt=ortp_event_get_type(ev);
			OrtpEventData *evd=ortp_event_get_data(ev);
			if (evt == ORTP_EVENT_ZRTP_ENCRYPTION_CHANGED){
				linphone_call_videostream_encryption_changed(call, evd->info.zrtp_stream_encrypted);
			} else if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
				call->stats[LINPHONE_CALL_STATS_VIDEO].round_trip_delay = rtp_session_get_round_trip_propagation(call->videostream->ms.sessions.rtp_session);
				if(call->stats[LINPHONE_CALL_STATS_VIDEO].received_rtcp != NULL)
					freemsg(call->stats[LINPHONE_CALL_STATS_VIDEO].received_rtcp);
				call->stats[LINPHONE_CALL_STATS_VIDEO].received_rtcp = evd->packet;
				evd->packet = NULL;
				call->stats[LINPHONE_CALL_STATS_VIDEO].updated = LINPHONE_CALL_STATS_RECEIVED_RTCP_UPDATE;
				update_local_stats(&call->stats[LINPHONE_CALL_STATS_VIDEO],(MediaStream*)call->videostream);
				linphone_reporting_call_stats_updated(call, LINPHONE_CALL_STATS_VIDEO);
				if (lc->vtable.call_stats_updated)
					lc->vtable.call_stats_updated(lc, call, &call->stats[LINPHONE_CALL_STATS_VIDEO]);
			} else if (evt == ORTP_EVENT_RTCP_PACKET_EMITTED) {
				memcpy(&call->stats[LINPHONE_CALL_STATS_VIDEO].jitter_stats, rtp_session_get_jitter_stats(call->videostream->ms.sessions.rtp_session), sizeof(jitter_stats_t));
				if(call->stats[LINPHONE_CALL_STATS_VIDEO].sent_rtcp != NULL)
					freemsg(call->stats[LINPHONE_CALL_STATS_VIDEO].sent_rtcp);
				call->stats[LINPHONE_CALL_STATS_VIDEO].sent_rtcp = evd->packet;
				evd->packet = NULL;
				call->stats[LINPHONE_CALL_STATS_VIDEO].updated = LINPHONE_CALL_STATS_SENT_RTCP_UPDATE;
				update_local_stats(&call->stats[LINPHONE_CALL_STATS_VIDEO],(MediaStream*)call->videostream);
				linphone_reporting_call_stats_updated(call, LINPHONE_CALL_STATS_VIDEO);
				if (lc->vtable.call_stats_updated)
					lc->vtable.call_stats_updated(lc, call, &call->stats[LINPHONE_CALL_STATS_VIDEO]);
			} else if ((evt == ORTP_EVENT_ICE_SESSION_PROCESSING_FINISHED) || (evt == ORTP_EVENT_ICE_GATHERING_FINISHED)
				|| (evt == ORTP_EVENT_ICE_LOSING_PAIRS_COMPLETED) || (evt == ORTP_EVENT_ICE_RESTART_NEEDED)) {
				handle_ice_events(call, ev);
			}
			ortp_event_destroy(ev);
		}
	}
#endif
	if (call->audiostream!=NULL) {
		OrtpEvent *ev;

		/* Ensure there is no dangling ICE check list. */
		if (call->ice_session == NULL) call->audiostream->ms.ice_check_list = NULL;

		// Beware that the application queue should not depend on treatments fron the
		// mediastreamer queue.
		audio_stream_iterate(call->audiostream);

		while (call->audiostream_app_evq && (NULL != (ev=ortp_ev_queue_get(call->audiostream_app_evq)))){
			OrtpEventType evt=ortp_event_get_type(ev);
			OrtpEventData *evd=ortp_event_get_data(ev);
			if (evt == ORTP_EVENT_ZRTP_ENCRYPTION_CHANGED){
				linphone_call_audiostream_encryption_changed(call, evd->info.zrtp_stream_encrypted);
			} else if (evt == ORTP_EVENT_ZRTP_SAS_READY) {
				linphone_call_audiostream_auth_token_ready(call, evd->info.zrtp_sas.sas, evd->info.zrtp_sas.verified);
			} else if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
				call->stats[LINPHONE_CALL_STATS_AUDIO].round_trip_delay = rtp_session_get_round_trip_propagation(call->audiostream->ms.sessions.rtp_session);
				if(call->stats[LINPHONE_CALL_STATS_AUDIO].received_rtcp != NULL)
					freemsg(call->stats[LINPHONE_CALL_STATS_AUDIO].received_rtcp);
				call->stats[LINPHONE_CALL_STATS_AUDIO].received_rtcp = evd->packet;
				evd->packet = NULL;
				call->stats[LINPHONE_CALL_STATS_AUDIO].updated = LINPHONE_CALL_STATS_RECEIVED_RTCP_UPDATE;
				update_local_stats(&call->stats[LINPHONE_CALL_STATS_AUDIO],(MediaStream*)call->audiostream);
				linphone_reporting_call_stats_updated(call, LINPHONE_CALL_STATS_AUDIO);
				if (lc->vtable.call_stats_updated)
					lc->vtable.call_stats_updated(lc, call, &call->stats[LINPHONE_CALL_STATS_AUDIO]);
			} else if (evt == ORTP_EVENT_RTCP_PACKET_EMITTED) {
				memcpy(&call->stats[LINPHONE_CALL_STATS_AUDIO].jitter_stats, rtp_session_get_jitter_stats(call->audiostream->ms.sessions.rtp_session), sizeof(jitter_stats_t));
				if(call->stats[LINPHONE_CALL_STATS_AUDIO].sent_rtcp != NULL)
					freemsg(call->stats[LINPHONE_CALL_STATS_AUDIO].sent_rtcp);
				call->stats[LINPHONE_CALL_STATS_AUDIO].sent_rtcp = evd->packet;
				evd->packet = NULL;
				call->stats[LINPHONE_CALL_STATS_AUDIO].updated = LINPHONE_CALL_STATS_SENT_RTCP_UPDATE;
				update_local_stats(&call->stats[LINPHONE_CALL_STATS_AUDIO],(MediaStream*)call->audiostream);
				linphone_reporting_call_stats_updated(call, LINPHONE_CALL_STATS_AUDIO);
				if (lc->vtable.call_stats_updated)
					lc->vtable.call_stats_updated(lc, call, &call->stats[LINPHONE_CALL_STATS_AUDIO]);
			} else if ((evt == ORTP_EVENT_ICE_SESSION_PROCESSING_FINISHED) || (evt == ORTP_EVENT_ICE_GATHERING_FINISHED)
				|| (evt == ORTP_EVENT_ICE_LOSING_PAIRS_COMPLETED) || (evt == ORTP_EVENT_ICE_RESTART_NEEDED)) {
				handle_ice_events(call, ev);
			} else if (evt==ORTP_EVENT_TELEPHONE_EVENT){
				linphone_core_dtmf_received(lc,evd->info.telephone_event);
			}
			ortp_event_destroy(ev);
		}
	}
	if (call->state==LinphoneCallStreamsRunning && one_second_elapsed && call->audiostream!=NULL && disconnect_timeout>0 )
		disconnected=!audio_stream_alive(call->audiostream,disconnect_timeout);
	if (disconnected)
		linphone_core_disconnected(call->core,call);
}

void linphone_call_log_completed(LinphoneCall *call){
	LinphoneCore *lc=call->core;

	call->log->duration=time(NULL)-call->log->start_date_time;

	if (call->log->status==LinphoneCallMissed){
		char *info;
		lc->missed_calls++;
		info=ortp_strdup_printf(ngettext("You have missed %i call.",
										 "You have missed %i calls.", lc->missed_calls),
								lc->missed_calls);
		if (lc->vtable.display_status!=NULL)
			lc->vtable.display_status(lc,info);
		ms_free(info);
	}
	lc->call_logs=ms_list_prepend(lc->call_logs,(void *)call->log);
	if (ms_list_size(lc->call_logs)>lc->max_call_logs){
		MSList *elem,*prevelem=NULL;
		/*find the last element*/
		for(elem=lc->call_logs;elem!=NULL;elem=elem->next){
			prevelem=elem;
		}
		elem=prevelem;
		linphone_call_log_destroy((LinphoneCallLog*)elem->data);
		lc->call_logs=ms_list_remove_link(lc->call_logs,elem);
	}
	if (lc->vtable.call_log_updated!=NULL){
		lc->vtable.call_log_updated(lc,call->log);
	}
	call_logs_write_to_config_file(lc);
}

/**
 * Returns the current transfer state, if a transfer has been initiated from this call.
 * @see linphone_core_transfer_call() , linphone_core_transfer_call_to_another()
**/
LinphoneCallState linphone_call_get_transfer_state(LinphoneCall *call) {
	return call->transfer_state;
}

void linphone_call_set_transfer_state(LinphoneCall* call, LinphoneCallState state) {
	if (state != call->transfer_state) {
		LinphoneCore* lc = call->core;
		ms_message("Transfer state for call [%p] changed  from [%s] to [%s]",call
																			,linphone_call_state_to_string(call->transfer_state)
																			,linphone_call_state_to_string(state));
		call->transfer_state = state;
		if (lc->vtable.transfer_state_changed)
			lc->vtable.transfer_state_changed(lc, call, state);
	}
}

bool_t linphone_call_is_in_conference(const LinphoneCall *call) {
	return call->params.in_conference;
}

/**
 * Perform a zoom of the video displayed during a call.
 * @param call the call.
 * @param zoom_factor a floating point number describing the zoom factor. A value 1.0 corresponds to no zoom applied.
 * @param cx a floating point number pointing the horizontal center of the zoom to be applied. This value should be between 0.0 and 1.0.
 * @param cy a floating point number pointing the vertical center of the zoom to be applied. This value should be between 0.0 and 1.0.
 *
 * cx and cy are updated in return in case their coordinates were too excentrated for the requested zoom factor. The zoom ensures that all the screen is fullfilled with the video.
**/
void linphone_call_zoom_video(LinphoneCall* call, float zoom_factor, float* cx, float* cy) {
	VideoStream* vstream = call->videostream;
	if (vstream && vstream->output) {
		float zoom[3];
		float halfsize;

		if (zoom_factor < 1)
			zoom_factor = 1;
		halfsize = 0.5 * 1.0 / zoom_factor;

		if ((*cx - halfsize) < 0)
			*cx = 0 + halfsize;
		if ((*cx + halfsize) > 1)
			*cx = 1 - halfsize;
		if ((*cy - halfsize) < 0)
			*cy = 0 + halfsize;
		if ((*cy + halfsize) > 1)
			*cy = 1 - halfsize;

		zoom[0] = zoom_factor;
		zoom[1] = *cx;
		zoom[2] = *cy;
			ms_filter_call_method(vstream->output, MS_VIDEO_DISPLAY_ZOOM, &zoom);
	}else ms_warning("Could not apply zoom: video output wasn't activated.");
}

static LinphoneAddress *get_fixed_contact(LinphoneCore *lc, LinphoneCall *call , LinphoneProxyConfig *dest_proxy){
	LinphoneAddress *ctt=NULL;
	LinphoneAddress *ret=NULL;
	const char *localip=call->localip;

	/* first use user's supplied ip address if asked*/
	if (linphone_core_get_firewall_policy(lc)==LinphonePolicyUseNatAddress){
		ctt=linphone_core_get_primary_contact_parsed(lc);
		linphone_address_set_domain(ctt,linphone_core_get_nat_address_resolved(lc));
		ret=ctt;
	} else if (call->op && sal_op_get_contact_address(call->op)!=NULL){
		/* if already choosed, don't change it */
		return NULL;
	} else if (call->ping_op && sal_op_get_contact_address(call->ping_op)) {
		/* if the ping OPTIONS request succeeded use the contact guessed from the
		 received, rport*/
		ms_message("Contact has been fixed using OPTIONS"/* to %s",guessed*/);
		ret=linphone_address_clone(sal_op_get_contact_address(call->ping_op));;
	} else 	if (dest_proxy && dest_proxy->op && sal_op_get_contact_address(dest_proxy->op)){
	/*if using a proxy, use the contact address as guessed with the REGISTERs*/
		ms_message("Contact has been fixed using proxy" /*to %s",fixed_contact*/);
		ret=linphone_address_clone(sal_op_get_contact_address(dest_proxy->op));
	} else {
		ctt=linphone_core_get_primary_contact_parsed(lc);
		if (ctt!=NULL){
			/*otherwise use supllied localip*/
			linphone_address_set_domain(ctt,localip);
			linphone_address_set_port(ctt,linphone_core_get_sip_port(lc));
			ms_message("Contact has been fixed using local ip"/* to %s",ret*/);
			ret=ctt;
		}
	}
	return ret;
}

void linphone_call_set_contact_op(LinphoneCall* call) {
	LinphoneAddress *contact;

	if (call->dest_proxy == NULL) {
		/* Try to define the destination proxy if it has not already been done to have a correct contact field in the SIP messages */
		call->dest_proxy = linphone_core_lookup_known_proxy(call->core, call->log->to);
	}

	contact=get_fixed_contact(call->core,call,call->dest_proxy);
	if (contact){
		SalTransport tport=sal_address_get_transport((SalAddress*)contact);
		sal_address_clean((SalAddress*)contact); /* clean out contact_params that come from proxy config*/
		sal_address_set_transport((SalAddress*)contact,tport);
		sal_op_set_contact_address(call->op, contact);
		linphone_address_destroy(contact);
	}
}
