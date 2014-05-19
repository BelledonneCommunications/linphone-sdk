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


#include "sal/sal.h"

#include "linphonecore.h"
#include "private.h"
#include "mediastreamer2/mediastream.h"
#include "lpconfig.h"

// stat
#ifndef WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static void register_failure(SalOp *op);

static int media_parameters_changed(LinphoneCall *call, SalMediaDescription *oldmd, SalMediaDescription *newmd) {
	if (call->params.in_conference != call->current_params.in_conference) return SAL_MEDIA_DESCRIPTION_CHANGED;
	if (call->up_bw != linphone_core_get_upload_bandwidth(call->core)) return SAL_MEDIA_DESCRIPTION_CHANGED;
	if (call->localdesc_changed) ms_message("Local description has changed: %i", call->localdesc_changed);
	return call->localdesc_changed | sal_media_description_equals(oldmd, newmd);
}

void linphone_core_update_streams_destinations(LinphoneCore *lc, LinphoneCall *call, SalMediaDescription *old_md, SalMediaDescription *new_md) {
	SalStreamDescription *new_audiodesc = NULL;
	SalStreamDescription *new_videodesc = NULL;
	char *rtp_addr, *rtcp_addr;
	int i;

	for (i = 0; i < new_md->n_active_streams; i++) {
		if (new_md->streams[i].type == SalAudio) {
			new_audiodesc = &new_md->streams[i];
		} else if (new_md->streams[i].type == SalVideo) {
			new_videodesc = &new_md->streams[i];
		}
	}
	if (call->audiostream && new_audiodesc) {
		rtp_addr = (new_audiodesc->rtp_addr[0] != '\0') ? new_audiodesc->rtp_addr : new_md->addr;
		rtcp_addr = (new_audiodesc->rtcp_addr[0] != '\0') ? new_audiodesc->rtcp_addr : new_md->addr;
		ms_message("Change audio stream destination: RTP=%s:%d RTCP=%s:%d", rtp_addr, new_audiodesc->rtp_port, rtcp_addr, new_audiodesc->rtcp_port);
		rtp_session_set_remote_addr_full(call->audiostream->ms.sessions.rtp_session, rtp_addr, new_audiodesc->rtp_port, rtcp_addr, new_audiodesc->rtcp_port);
	}
#ifdef VIDEO_ENABLED
	if (call->videostream && new_videodesc) {
		rtp_addr = (new_videodesc->rtp_addr[0] != '\0') ? new_videodesc->rtp_addr : new_md->addr;
		rtcp_addr = (new_videodesc->rtcp_addr[0] != '\0') ? new_videodesc->rtcp_addr : new_md->addr;
		ms_message("Change video stream destination: RTP=%s:%d RTCP=%s:%d", rtp_addr, new_videodesc->rtp_port, rtcp_addr, new_videodesc->rtcp_port);
		rtp_session_set_remote_addr_full(call->videostream->ms.sessions.rtp_session, rtp_addr, new_videodesc->rtp_port, rtcp_addr, new_videodesc->rtcp_port);
	}
#else
	(void)new_videodesc;
#endif
}

void linphone_core_update_streams(LinphoneCore *lc, LinphoneCall *call, SalMediaDescription *new_md){
	SalMediaDescription *oldmd=call->resultdesc;
	bool_t all_muted=FALSE;
	bool_t send_ringbacktone=FALSE;

	linphone_core_stop_ringing(lc);
	if (!new_md) {
		ms_error("linphone_core_update_streams() called with null media description");
		return;
	}
	if (call->biggestdesc==NULL || new_md->n_total_streams>call->biggestdesc->n_total_streams){
		/*we have been offered and now are ready to proceed, or we added a new stream*/
		/*store the media description to remember the mapping of calls*/
		if (call->biggestdesc){
			sal_media_description_unref(call->biggestdesc);
			call->biggestdesc=NULL;
		}
		if (sal_call_is_offerer(call->op))
			call->biggestdesc=sal_media_description_ref(call->localdesc);
		else
			call->biggestdesc=sal_media_description_ref(sal_call_get_remote_media_description(call->op));
	}
	sal_media_description_ref(new_md);
	call->expect_media_in_ack=FALSE;
	call->resultdesc=new_md;
	if ((call->audiostream && call->audiostream->ms.state==MSStreamStarted) || (call->videostream && call->videostream->ms.state==MSStreamStarted)){
		/* we already started media: check if we really need to restart it*/
		if (oldmd){
			int md_changed = media_parameters_changed(call, oldmd, new_md);
			if ((md_changed & SAL_MEDIA_DESCRIPTION_CODEC_CHANGED) || call->playing_ringbacktone) {
				ms_message("Media descriptions are different, need to restart the streams.");
			} else {
				if (md_changed == SAL_MEDIA_DESCRIPTION_UNCHANGED) {
					if (call->all_muted){
						ms_message("Early media finished, unmuting inputs...");
						/*we were in early media, now we want to enable real media */
						linphone_call_enable_camera (call,linphone_call_camera_enabled (call));
						if (call->audiostream)
							linphone_core_enable_mic(lc, linphone_core_mic_enabled(lc));
#ifdef VIDEO_ENABLED
						if (call->videostream && call->camera_enabled)
							video_stream_change_camera(call->videostream,lc->video_conf.device );
#endif
					}
					/*FIXME ZRTP, might be restarted in any cases ? */
					ms_message("No need to restart streams, SDP is unchanged.");
					goto end;
				}else {
					if (md_changed & SAL_MEDIA_DESCRIPTION_NETWORK_CHANGED) {
						ms_message("Network parameters have changed, update them.");
						linphone_core_update_streams_destinations(lc, call, oldmd, new_md);
					}
					if (md_changed & SAL_MEDIA_DESCRIPTION_CRYPTO_CHANGED) {
						ms_message("Crypto parameters have changed, update them.");
						linphone_call_update_crypto_parameters(call, oldmd, new_md);
					}
					goto end;
				}
			}
		}
		linphone_call_stop_media_streams (call);
		linphone_call_init_media_streams (call);
	}
	
	if (call->audiostream==NULL){
		/*this happens after pausing the call locally. The streams are destroyed and then we wait the 200Ok to recreate them*/
		linphone_call_init_media_streams (call);
	}
	if (call->state==LinphoneCallIncomingEarlyMedia && linphone_core_get_remote_ringback_tone (lc)!=NULL){
		send_ringbacktone=TRUE;
	}
	if ((call->state==LinphoneCallIncomingEarlyMedia || call->state==LinphoneCallOutgoingEarlyMedia) && !call->params.real_early_media){
		all_muted=TRUE;
	}
	linphone_call_start_media_streams(call,all_muted,send_ringbacktone);
	if (call->state==LinphoneCallPausing && call->paused_by_app && ms_list_size(lc->calls)==1){
		linphone_core_play_named_tone(lc,LinphoneToneCallOnHold);
	}
	end:
	if (oldmd) 
		sal_media_description_unref(oldmd);
	
}
#if 0
static bool_t is_duplicate_call(LinphoneCore *lc, const LinphoneAddress *from, const LinphoneAddress *to){
	MSList *elem;
	for(elem=lc->calls;elem!=NULL;elem=elem->next){
		LinphoneCall *call=(LinphoneCall*)elem->data;
		if (linphone_address_weak_equal(call->log->from,from) &&
		    linphone_address_weak_equal(call->log->to, to)){
			return TRUE;
		}
	}
	return FALSE;
}
#endif

static bool_t already_a_call_with_remote_address(const LinphoneCore *lc, const LinphoneAddress *remote) {
	MSList *elem;
	ms_warning(" searching for already_a_call_with_remote_address.");

	for(elem=lc->calls;elem!=NULL;elem=elem->next){
		const LinphoneCall *call=(LinphoneCall*)elem->data;
		const LinphoneAddress *cRemote=linphone_call_get_remote_address(call);
		if (linphone_address_weak_equal(cRemote,remote)) {
			ms_warning("already_a_call_with_remote_address found.");
			return TRUE;
		}
	}
	return FALSE;
}

static bool_t already_a_call_pending(LinphoneCore *lc){
	MSList *elem;
	for(elem=lc->calls;elem!=NULL;elem=elem->next){
		LinphoneCall *call=(LinphoneCall*)elem->data;
		if (call->state==LinphoneCallIncomingReceived
		    || call->state==LinphoneCallOutgoingInit
		    || call->state==LinphoneCallOutgoingProgress
		    || call->state==LinphoneCallOutgoingEarlyMedia
		    || call->state==LinphoneCallOutgoingRinging){
			return TRUE;
		}
	}
	return FALSE;
}

static void call_received(SalOp *h){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(h));
	LinphoneCall *call;
	const char *from,*to;
	char *alt_contact;
	LinphoneAddress *from_addr, *to_addr;
	/*this mode is deprcated because probably useless*/
	bool_t prevent_colliding_calls=lp_config_get_int(lc->config,"sip","prevent_colliding_calls",FALSE);
	
	/* first check if we can answer successfully to this invite */
	if (linphone_presence_model_get_basic_status(lc->presence_model) == LinphonePresenceBasicStatusClosed) {
		LinphonePresenceActivity *activity = linphone_presence_model_get_activity(lc->presence_model);
		switch (linphone_presence_activity_get_type(activity)) {
			case LinphonePresenceActivityBusy:
				sal_call_decline(h,SalReasonBusy,NULL);
				break;
			case LinphonePresenceActivityAppointment:
			case LinphonePresenceActivityMeeting:
			case LinphonePresenceActivityOffline:
			case LinphonePresenceActivityWorship:
				sal_call_decline(h,SalReasonTemporarilyUnavailable,NULL);
				break;
			case LinphonePresenceActivityPermanentAbsence:
				alt_contact = linphone_presence_model_get_contact(lc->presence_model);
				if (alt_contact != NULL) {
					sal_call_decline(h,SalReasonRedirect,alt_contact);
					ms_free(alt_contact);
				}
				break;
			default:
				break;
		}
		sal_op_release(h);
		return;
	}

	if (!linphone_core_can_we_add_call(lc)){/*busy*/
		sal_call_decline(h,SalReasonBusy,NULL);
		sal_op_release(h);
		return;
	}
	from=sal_op_get_from(h);
	to=sal_op_get_to(h);
	from_addr=linphone_address_new(from);
	to_addr=linphone_address_new(to);

	if ((already_a_call_with_remote_address(lc,from_addr) && prevent_colliding_calls) || already_a_call_pending(lc)){
		ms_warning("Receiving another call while one is ringing or initiated, refusing this one with busy message.");
		sal_call_decline(h,SalReasonBusy,NULL);
		sal_op_release(h);
		linphone_address_destroy(from_addr);
		linphone_address_destroy(to_addr);
		return;
	}
	
	call=linphone_call_new_incoming(lc,from_addr,to_addr,h);
	
	/* the call is acceptable so we can now add it to our list */
	linphone_core_add_call(lc,call);
	linphone_call_ref(call); /*prevent the call from being destroyed while we are notifying, if the user declines within the state callback */

	if ((linphone_core_get_firewall_policy(lc) == LinphonePolicyUseIce) && (call->ice_session != NULL)) {
		/* Defer ringing until the end of the ICE candidates gathering process. */
		ms_message("Defer ringing to gather ICE candidates");
		return;
	}
#ifdef BUILD_UPNP
	if ((linphone_core_get_firewall_policy(lc) == LinphonePolicyUseUpnp) && (call->upnp_session != NULL)) {
		/* Defer ringing until the end of the ICE candidates gathering process. */
		ms_message("Defer ringing to gather uPnP candidates");
		return;
	}
#endif //BUILD_UPNP

	linphone_core_notify_incoming_call(lc,call);
}

static void call_ringing(SalOp *h){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(h));
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(h);
	SalMediaDescription *md;
	
	if (call==NULL) return;
	
	/*set privacy*/
	call->current_params.privacy=(LinphonePrivacyMask)sal_op_get_privacy(call->op);

	if (lc->vtable.display_status)
		lc->vtable.display_status(lc,_("Remote ringing."));
	
	md=sal_call_get_final_media_description(h);
	if (md==NULL){
		linphone_core_stop_dtmf_stream(lc);
		if (lc->ringstream!=NULL) return;/*already ringing !*/
		if (lc->sound_conf.play_sndcard!=NULL){
			MSSndCard *ringcard=lc->sound_conf.lsd_card ? lc->sound_conf.lsd_card : lc->sound_conf.play_sndcard;
			if (call->localdesc->streams[0].max_rate>0) ms_snd_card_set_preferred_sample_rate(ringcard, call->localdesc->streams[0].max_rate);
			/*we release sound before playing ringback tone*/
			if (call->audiostream)
				audio_stream_unprepare_sound(call->audiostream);
			lc->ringstream=ring_start(lc->sound_conf.remote_ring,2000,ringcard);
		}
		ms_message("Remote ringing...");
		if (lc->vtable.display_status) 
			lc->vtable.display_status(lc,_("Remote ringing..."));
		linphone_call_set_state(call,LinphoneCallOutgoingRinging,"Remote ringing");
	}else{
		/*accept early media */
		if (call->audiostream && audio_stream_started(call->audiostream)){
			/*streams already started */
			ms_message("Early media already started.");
			return;
		}
		if (lc->vtable.show) lc->vtable.show(lc);
		if (lc->vtable.display_status) 
			lc->vtable.display_status(lc,_("Early media."));
		linphone_call_set_state(call,LinphoneCallOutgoingEarlyMedia,"Early media");
		linphone_core_stop_ringing(lc);
		ms_message("Doing early media...");
		linphone_core_update_streams(lc,call,md);
	}
}

/*
 * could be reach :
 *  - when the call is accepted
 *  - when a request is accepted (pause, resume)
 */
static void call_accepted(SalOp *op){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(op);
	SalMediaDescription *md;
	
	if (call==NULL){
		ms_warning("No call to accept.");
		return ;
	}
	/*set privacy*/
	call->current_params.privacy=(LinphonePrivacyMask)sal_op_get_privacy(call->op);

	/* Handle remote ICE attributes if any. */
	if (call->ice_session != NULL) {
		linphone_core_update_ice_from_remote_media_description(call, sal_call_get_remote_media_description(op));
	}
#ifdef BUILD_UPNP
	if (call->upnp_session != NULL) {
		linphone_core_update_upnp_from_remote_media_description(call, sal_call_get_remote_media_description(op));
	}
#endif //BUILD_UPNP

	md=sal_call_get_final_media_description(op);
	if (md) /*make sure re-invite will not propose video again*/
		call->params.has_video &= linphone_core_media_description_contains_video_stream(md);
	
	if (call->state==LinphoneCallOutgoingProgress ||
	    call->state==LinphoneCallOutgoingRinging ||
	    call->state==LinphoneCallOutgoingEarlyMedia){
		linphone_call_set_state(call,LinphoneCallConnected,"Connected");
		if (call->referer) linphone_core_notify_refer_state(lc,call->referer,call);
	}
	if (md && !sal_media_description_empty(md) && !linphone_core_incompatible_security(lc,md)){
		linphone_call_update_remote_session_id_and_ver(call);
		if (sal_media_description_has_dir(md,SalStreamSendOnly) ||
		    sal_media_description_has_dir(md,SalStreamInactive)){
			if (lc->vtable.display_status){
				char *tmp=linphone_call_get_remote_address_as_string (call);
				char *msg=ms_strdup_printf(_("Call with %s is paused."),tmp);
				lc->vtable.display_status(lc,msg);
				ms_free(tmp);
				ms_free(msg);
			}
			linphone_core_update_streams (lc,call,md);
			linphone_call_set_state(call,LinphoneCallPaused,"Call paused");
			if (call->refer_pending)
				linphone_core_start_refered_call(lc,call,NULL);
		}else if (sal_media_description_has_dir(md,SalStreamRecvOnly)){
			/*we are put on hold when the call is initially accepted */
			if (lc->vtable.display_status){
				char *tmp=linphone_call_get_remote_address_as_string (call);
				char *msg=ms_strdup_printf(_("Call answered by %s - on hold."),tmp);
				lc->vtable.display_status(lc,msg);
				ms_free(tmp);
				ms_free(msg);
			}
			linphone_core_update_streams (lc,call,md);
			linphone_call_set_state(call,LinphoneCallPausedByRemote,"Call paused by remote");
		}else{
			if (call->state!=LinphoneCallUpdating){
				if (call->state==LinphoneCallResuming){
					if (lc->vtable.display_status){
						lc->vtable.display_status(lc,_("Call resumed."));
					}
				}else{
					if (lc->vtable.display_status){
						char *tmp=linphone_call_get_remote_address_as_string (call);
						char *msg=ms_strdup_printf(_("Call answered by %s."),tmp);
						lc->vtable.display_status(lc,msg);
						ms_free(tmp);
						ms_free(msg);
					}
				}
			}
			linphone_core_update_streams(lc,call,md);
			/*also reflect the change if the "wished" params, in order to avoid to propose SAVP or video again
			* further in the call, for example during pause,resume, conferencing reINVITEs*/
			linphone_call_fix_call_parameters(call);
			if (!call->current_params.in_conference)
				lc->current_call=call;
			if (call->prevstate != LinphoneCallIncomingEarlyMedia) /*don't change state in aswer to a SIP UPDATE in early media*/
				linphone_call_set_state(call, LinphoneCallStreamsRunning, "Streams running");
		}
	}else{
		/*send a bye*/
		ms_error("Incompatible SDP offer received in 200Ok, need to abort the call");
		linphone_core_abort_call(lc,call,_("Incompatible, check codecs or security settings..."));
	}
}

static void call_ack(SalOp *op){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(op);
	if (call==NULL){
		ms_warning("No call to be ACK'd");
		return ;
	}
	if (call->expect_media_in_ack){
		SalMediaDescription *md=sal_call_get_final_media_description(op);
		if (md && !sal_media_description_empty(md)){
			linphone_core_update_streams(lc,call,md);
			linphone_call_set_state (call,LinphoneCallStreamsRunning,"Connected (streams running)");
		}else{
			/*send a bye*/
			ms_error("Incompatible SDP response received in ACK, need to abort the call");
			linphone_core_abort_call(lc,call,"No codec intersection");
			return;
		}
	}
}

static void call_accept_update(LinphoneCore *lc, LinphoneCall *call){
	SalMediaDescription *md;
	SalMediaDescription *rmd=sal_call_get_remote_media_description(call->op);
	if (rmd!=NULL && call->ice_session!=NULL) {
		linphone_core_update_ice_from_remote_media_description(call,rmd);
		linphone_core_update_local_media_description_from_ice(call->localdesc,call->ice_session);
	}
#ifdef BUILD_UPNP
	if(call->upnp_session != NULL) {
		linphone_core_update_upnp_from_remote_media_description(call, rmd);
		linphone_core_update_local_media_description_from_upnp(call->localdesc,call->upnp_session);
	}
#endif //BUILD_UPNP
	linphone_call_update_remote_session_id_and_ver(call);
	sal_call_accept(call->op);
	md=sal_call_get_final_media_description(call->op);
	if (md && !sal_media_description_empty(md)){
		linphone_core_update_streams(lc,call,md);
	}
}

static void call_resumed(LinphoneCore *lc, LinphoneCall *call){
	call_accept_update(lc,call);
	if(lc->vtable.display_status)
		lc->vtable.display_status(lc,_("We have been resumed."));
	linphone_call_set_state(call,LinphoneCallStreamsRunning,"Connected (streams running)");
}

static void call_paused_by_remote(LinphoneCore *lc, LinphoneCall *call){
	call_accept_update(lc,call);
	/* we are being paused */
	if(lc->vtable.display_status)
		lc->vtable.display_status(lc,_("We are paused by other party."));
	linphone_call_set_state (call,LinphoneCallPausedByRemote,"Call paused by remote");
}

static void call_updated_by_remote(LinphoneCore *lc, LinphoneCall *call,bool_t notify_application){
	/*first check if media capabilities are compatible*/
	SalMediaDescription* md;
	linphone_call_make_local_media_description(lc,call);
	sal_call_set_local_media_description(call->op,call->localdesc);
	md=sal_call_get_final_media_description(call->op);
	if (md && (sal_media_description_empty(md) || linphone_core_incompatible_security(lc,md))){
		sal_call_decline(call->op,SalReasonNotAcceptable,NULL);
		return;
	}

	if (notify_application) {
		if(lc->vtable.display_status)
			lc->vtable.display_status(lc,_("Call is updated by remote."));
		call->defer_update=FALSE;
		linphone_call_set_state(call, LinphoneCallUpdatedByRemote,"Call updated by remote");
		if (call->defer_update==FALSE){
			linphone_core_accept_call_update(lc,call,NULL);
		}
	} else { /*SIP UPDATE case*/
		/*can be call from any state*/
		_linphone_core_accept_call_update(lc,call,NULL);
	}
}

/* this callback is called when an incoming re-INVITE/ SIP UPDATE modifies the session*/
static void call_updating(SalOp *op){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(op);
	SalMediaDescription *rmd=sal_call_get_remote_media_description(op);

	if (rmd==NULL){
		/* case of a reINVITE without SDP */
		call_accept_update(lc,call);
		call->expect_media_in_ack=TRUE;
		return;
	}

	switch(call->state){
		case LinphoneCallPausedByRemote:
			if (sal_media_description_has_dir(rmd,SalStreamSendRecv) || sal_media_description_has_dir(rmd,SalStreamRecvOnly)){
				call_resumed(lc,call);
			}else call_paused_by_remote(lc,call);
		break;
		/*SIP UPDATE CASE*/
		case LinphoneCallOutgoingEarlyMedia:
			call_updated_by_remote(lc,call,FALSE);
			break;
		case LinphoneCallStreamsRunning:
		case LinphoneCallConnected:
			if (sal_media_description_has_dir(rmd,SalStreamSendOnly) || sal_media_description_has_dir(rmd,SalStreamInactive)){
				call_paused_by_remote(lc,call);
			}else{
				call_updated_by_remote(lc,call,TRUE);
			}
		break;
		default:
			call_accept_update(lc,call);
	}
}

static void call_terminated(SalOp *op, const char *from){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(op);

	if (call==NULL) return;
	
	switch(linphone_call_get_state(call)){
		case LinphoneCallEnd:
		case LinphoneCallError:
			ms_warning("call_terminated: ignoring.");
			return;
		break;
		case LinphoneCallIncomingReceived:
		case LinphoneCallIncomingEarlyMedia:
			sal_error_info_set(&call->non_op_error,SalReasonRequestTimeout,0,"Incoming call cancelled",NULL);
		break;
		default:
		break;
	}
	ms_message("Current call terminated...");
	if (call->refer_pending){
		linphone_core_start_refered_call(lc,call,NULL);
	}
	//we stop the call only if we have this current call or if we are in call
	if (lc->ringstream!=NULL && ( (ms_list_size(lc->calls)  == 1) || linphone_core_in_call(lc) )) {
		linphone_core_stop_ringing(lc);
	}
	linphone_call_stop_media_streams(call);
	if (lc->vtable.show!=NULL)
		lc->vtable.show(lc);
	if (lc->vtable.display_status!=NULL)
		lc->vtable.display_status(lc,_("Call terminated."));

#ifdef BUILD_UPNP
	linphone_call_delete_upnp_session(call);
#endif //BUILD_UPNP

	linphone_call_set_state(call, LinphoneCallEnd,"Call ended");
}

static int resume_call_after_failed_transfer(LinphoneCall *call){
	ms_message("!!!!!!!!!!resume_call_after_failed_transfer");
	if (call->was_automatically_paused && call->state==LinphoneCallPausing)
		return BELLE_SIP_CONTINUE; /*was still in pausing state*/
	
	if (call->was_automatically_paused && call->state==LinphoneCallPaused){
		if (sal_op_is_idle(call->op)){
			linphone_core_resume_call(call->core,call);
		}else {
			ms_message("!!!!!!!!!!resume_call_after_failed_transfer, salop was busy");
			return BELLE_SIP_CONTINUE;
		}
	}
	linphone_call_unref(call);
	return BELLE_SIP_STOP;
}

static void call_failure(SalOp *op){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	const SalErrorInfo *ei=sal_op_get_error_info(op);
	char *msg486=_("User is busy.");
	char *msg480=_("User is temporarily unavailable.");
	/*char *retrymsg=_("%s. Retry after %i minute(s).");*/
	char *msg600=_("User does not want to be disturbed.");
	char *msg603=_("Call declined.");
	const char *msg=ei->full_string;
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(op);
	LinphoneCall *referer=call->referer;

	if (call==NULL){
		ms_warning("Call faillure reported on already terminated call.");
		return ;
	}
	
	if (lc->vtable.show) lc->vtable.show(lc);
	switch(ei->reason){
		case SalReasonNone:
		break;
		case SalReasonRequestTimeout:
			msg=_("Request timeout.");
			if (lc->vtable.display_status)
				lc->vtable.display_status(lc,msg);
		break;
		case SalReasonDeclined:
			msg=msg603;
			if (lc->vtable.display_status)
				lc->vtable.display_status(lc,msg603);
		break;
		case SalReasonBusy:
			msg=msg486;
			if (lc->vtable.display_status)
				lc->vtable.display_status(lc,msg486);
		break;
		case SalReasonRedirect:
		{
			linphone_call_stop_media_streams(call);
			if (	call->state==LinphoneCallOutgoingInit
					|| call->state==LinphoneCallOutgoingProgress
					|| call->state==LinphoneCallOutgoingRinging /*push case*/
					|| call->state==LinphoneCallOutgoingEarlyMedia){
				LinphoneAddress* redirection_to = (LinphoneAddress*)sal_op_get_remote_contact_address(call->op);
				if( redirection_to ){
					char* url = linphone_address_as_string(redirection_to);
					ms_warning("Redirecting call [%p] to %s",call, url);
					ms_free(url);
					linphone_call_create_op(call);
					linphone_core_start_invite(lc, call, redirection_to);
					return;
				}
			}
			msg=_("Redirected");
			if (lc->vtable.display_status)
				lc->vtable.display_status(lc,msg);
		}
		break;
		case SalReasonTemporarilyUnavailable:
			msg=msg480;
			if (lc->vtable.display_status)
				lc->vtable.display_status(lc,msg480);
		break;
		case SalReasonNotFound:
			if (lc->vtable.display_status)
				lc->vtable.display_status(lc,msg);
		break;
		case SalReasonDoNotDisturb:
			msg=msg600;
			if (lc->vtable.display_status)
				lc->vtable.display_status(lc,msg600);
		break;
		case SalReasonUnsupportedContent: /*<this is for compatibility: linphone sent 415 because of SDP offer answer failure*/
		case SalReasonNotAcceptable:
		//media_encryption_mandatory
			if (call->params.media_encryption == LinphoneMediaEncryptionSRTP && 
				!linphone_core_is_media_encryption_mandatory(lc)) {
				int i;
				ms_message("Outgoing call [%p] failed with SRTP (SAVP) enabled",call);
				if (call->state==LinphoneCallOutgoingInit
						|| call->state==LinphoneCallOutgoingProgress
						|| call->state==LinphoneCallOutgoingRinging /*push case*/
						|| call->state==LinphoneCallOutgoingEarlyMedia){
					ms_message("Retrying call [%p] with AVP",call);
					/* clear SRTP local params */
					call->params.media_encryption = LinphoneMediaEncryptionNone;
					for(i=0; i<call->localdesc->n_active_streams; i++) {
						call->localdesc->streams[i].proto = SalProtoRtpAvp;
						memset(call->localdesc->streams[i].crypto, 0, sizeof(call->localdesc->streams[i].crypto));
					}
					linphone_core_restart_invite(lc, call);
					return;
				}
			}
			msg=_("Incompatible media parameters.");
			if (lc->vtable.display_status)
				lc->vtable.display_status(lc,msg);
		break;
		case SalReasonRequestPending:
			/*restore previous state, the application will decide to resubmit the action if relevant*/
			linphone_call_set_state(call,call->prevstate,msg);
			return;
		break;
		default:
			if (lc->vtable.display_status)
				lc->vtable.display_status(lc,_("Call failed."));
	}

	/*some call error are not fatal*/
	switch (call->state) {
	case LinphoneCallUpdating:
	case LinphoneCallPausing:
	case LinphoneCallResuming:
		ms_message("Call error on state [%s], restoring previous state",linphone_call_state_to_string(call->prevstate));
		linphone_call_set_state(call, call->prevstate,ei->full_string);
		return;
	default:
		break; /*nothing to do*/
	}

	linphone_core_stop_ringing(lc);
	linphone_call_stop_media_streams(call);

#ifdef BUILD_UPNP
	linphone_call_delete_upnp_session(call);
#endif //BUILD_UPNP
	
	if (call->state!=LinphoneCallEnd && call->state!=LinphoneCallError){
		if (ei->reason==SalReasonDeclined){
			linphone_call_set_state(call,LinphoneCallEnd,"Call declined.");
		}else{
			linphone_call_set_state(call,LinphoneCallError,ei->full_string);
		}
		if (ei->reason!=SalReasonNone) linphone_core_play_call_error_tone(lc,linphone_reason_from_sal(ei->reason));
	}
	
	if (referer){
		/*notify referer of the failure*/
		linphone_core_notify_refer_state(lc,referer,call);
		/*schedule automatic resume of the call. This must be done only after the notifications are completed due to dialog serialization of requests.*/
		linphone_core_queue_task(lc,(belle_sip_source_func_t)resume_call_after_failed_transfer,linphone_call_ref(referer),"Automatic call resuming after failed transfer");
	}
}

static void call_released(SalOp *op){
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(op);
	if (call!=NULL){
		linphone_call_set_state(call,LinphoneCallReleased,"Call released");
	}else ms_error("call_released() for already destroyed call ?");
}

static void auth_failure(SalOp *op, SalAuthInfo* info) {
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	LinphoneAuthInfo *ai=(LinphoneAuthInfo*)linphone_core_find_auth_info(lc,info->realm,info->username,info->domain);
	if (ai){
		ms_message("%s/%s/%s authentication fails.",info->realm,info->username,info->domain);
		/*ask again for password if auth info was already supplied but apparently not working*/
		if (lc->vtable.auth_info_requested) {
			lc->vtable.auth_info_requested(lc,info->realm,info->username,info->domain);
		}
	}
	
}

static void register_success(SalOp *op, bool_t registered){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	LinphoneProxyConfig *cfg=(LinphoneProxyConfig*)sal_op_get_user_pointer(op);
	char *msg;
	
	if (!cfg || cfg->deletion_date!=0){
		ms_message("Registration success for removed proxy config, ignored");
		return;
	}
	linphone_proxy_config_set_state(cfg, registered ? LinphoneRegistrationOk : LinphoneRegistrationCleared ,
	                                registered ? "Registration successful" : "Unregistration done");
	if (lc->vtable.display_status){
		if (registered) msg=ms_strdup_printf(_("Registration on %s successful."),sal_op_get_proxy(op));
		else msg=ms_strdup_printf(_("Unregistration on %s done."),sal_op_get_proxy(op));
		lc->vtable.display_status(lc,msg);
		ms_free(msg);
	}
	
}

static void register_failure(SalOp *op){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	LinphoneProxyConfig *cfg=(LinphoneProxyConfig*)sal_op_get_user_pointer(op);
	const SalErrorInfo *ei=sal_op_get_error_info(op);
	const char *details=ei->full_string;

	if (cfg==NULL){
		ms_warning("Registration failed for unknown proxy config.");
		return ;
	}
	if (cfg->deletion_date!=0){
		ms_message("Registration failed for removed proxy config, ignored");
		return;
	}
	if (details==NULL)
		details=_("no response timeout");
	
	if (lc->vtable.display_status) {
		char *msg=ortp_strdup_printf(_("Registration on %s failed: %s"),sal_op_get_proxy(op), details);
		lc->vtable.display_status(lc,msg);
		ms_free(msg);
	}

	if ((ei->reason == SalReasonServiceUnavailable || ei->reason == SalReasonIOError)
			&& linphone_proxy_config_get_state(cfg) == LinphoneRegistrationOk) {
		linphone_proxy_config_set_state(cfg,LinphoneRegistrationProgress,_("Service unavailable, retrying"));
	} else {
		linphone_proxy_config_set_state(cfg,LinphoneRegistrationFailed,details);
	}
	if (cfg->publish_op){
		/*prevent publish to be sent now until registration gets successful*/
		sal_op_release(cfg->publish_op);
		cfg->publish_op=NULL;
		cfg->send_publish=cfg->publish;
	}
}

static void vfu_request(SalOp *op){
#ifdef VIDEO_ENABLED
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer (op);
	if (call==NULL){
		ms_warning("VFU request but no call !");
		return ;
	}
	if (call->videostream)
		video_stream_send_vfu(call->videostream);
#endif
}

static void dtmf_received(SalOp *op, char dtmf){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(op);
	if (lc->vtable.dtmf_received != NULL)
		lc->vtable.dtmf_received(lc, call, dtmf);
}

static void refer_received(Sal *sal, SalOp *op, const char *referto){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal);
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(op);
	if (call){
		if (call->refer_to!=NULL){
			ms_free(call->refer_to);
		}
		call->refer_to=ms_strdup(referto);
		call->refer_pending=TRUE;
		linphone_call_set_state(call,LinphoneCallRefered,"Refered");
		if (lc->vtable.display_status){
			char *msg=ms_strdup_printf(_("We are transferred to %s"),referto);
			lc->vtable.display_status(lc,msg);
			ms_free(msg);
		}
		if (call->refer_pending) linphone_core_start_refered_call(lc,call,NULL);
	}else if (lc->vtable.refer_received){
		lc->vtable.refer_received(lc,referto);
	}
}

static bool_t is_duplicate_msg(LinphoneCore *lc, const char *msg_id){
	MSList *elem=lc->last_recv_msg_ids;
	MSList *tail=NULL;
	int i;
	bool_t is_duplicate=FALSE;
	for(i=0;elem!=NULL;elem=elem->next,i++){
		if (strcmp((const char*)elem->data,msg_id)==0){
			is_duplicate=TRUE;
		}
		tail=elem;
	}
	if (!is_duplicate){
		lc->last_recv_msg_ids=ms_list_prepend(lc->last_recv_msg_ids,ms_strdup(msg_id));
	}
	if (i>=10){
		ms_free(tail->data);
		lc->last_recv_msg_ids=ms_list_remove_link(lc->last_recv_msg_ids,tail);
	}
	return is_duplicate;
}


static void text_received(SalOp *op, const SalMessage *msg){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(op);
	if (lc->chat_deny_code==LinphoneReasonNone && is_duplicate_msg(lc,msg->message_id)==FALSE){
		linphone_core_message_received(lc,op,msg);
	}
	sal_message_reply(op,linphone_reason_to_sal(lc->chat_deny_code));
	if (!call) sal_op_release(op);
}

static void is_composing_received(SalOp *op, const SalIsComposing *is_composing) {
	LinphoneCore *lc = (LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	linphone_core_is_composing_received(lc, op, is_composing);
}

static void parse_presence_requested(SalOp *op, const char *content_type, const char *content_subtype, const char *body, SalPresenceModel **result) {
	linphone_notify_parse_presence(op, content_type, content_subtype, body, result);
}

static void convert_presence_to_xml_requested(SalOp *op, SalPresenceModel *presence, const char *contact, char **content) {
	linphone_notify_convert_presence_to_xml(op, presence, contact, content);
}

static void notify_presence(SalOp *op, SalSubscribeStatus ss, SalPresenceModel *model, const char *msg){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	linphone_notify_recv(lc,op,ss,model);
}

static void subscribe_presence_received(SalOp *op, const char *from){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	linphone_subscription_new(lc,op,from);
}

static void subscribe_presence_closed(SalOp *op, const char *from){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	linphone_subscription_closed(lc,op);
}

static void ping_reply(SalOp *op){
	LinphoneCall *call=(LinphoneCall*) sal_op_get_user_pointer(op);
	ms_message("ping reply !");
	if (call){
		if (call->state==LinphoneCallOutgoingInit){
			call->ping_replied=TRUE;
			linphone_core_proceed_with_invite_if_ready(call->core,call,NULL);
		}
	}
	else
	{
		ms_warning("ping reply without call attached...");
	}
}

static bool_t fill_auth_info_with_client_certificate(LinphoneCore *lc, SalAuthInfo* sai) {
		const char *chain_file = lp_config_get_string(lc->config,"sip","client_cert_chain", 0);
		const char *key_file = lp_config_get_string(lc->config,"sip","client_cert_key", 0);;

#ifndef WIN32
		{
		// optinal check for files
		struct stat st;
		if (stat(key_file,&st)) {
			ms_warning("No client certificate key found in %s", key_file);
			return FALSE;
		}
		if (stat(chain_file,&st)) {
			ms_warning("No client certificate chain found in %s", chain_file);
			return FALSE;
		}
		}
#endif

		sal_certificates_chain_parse_file(sai, chain_file, SAL_CERTIFICATE_RAW_FORMAT_PEM );
		sal_signing_key_parse_file(sai, key_file, "");
		return sai->certificates && sai->key;
}

static bool_t fill_auth_info(LinphoneCore *lc, SalAuthInfo* sai) {
	LinphoneAuthInfo *ai=(LinphoneAuthInfo*)linphone_core_find_auth_info(lc,sai->realm,sai->username,sai->domain);
	if (ai) {
		sai->userid=ms_strdup(ai->userid?ai->userid:ai->username);
		sai->password=ai->passwd?ms_strdup(ai->passwd):NULL;
		sai->ha1=ai->ha1?ms_strdup(ai->ha1):NULL;
		return TRUE;
	} else {
		return FALSE;
	}
}
static bool_t auth_requested(Sal* sal, SalAuthInfo* sai) {
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal);
	if (sai->mode == SalAuthModeHttpDigest) {
		if (fill_auth_info(lc,sai)) {
			return TRUE;
		} else {
			if (lc->vtable.auth_info_requested) {
				lc->vtable.auth_info_requested(lc,sai->realm,sai->username,sai->domain);
				if (fill_auth_info(lc,sai)) {
					return TRUE;
				}
			}
			return FALSE;
		}
	} else if (sai->mode == SalAuthModeTls) {
		return fill_auth_info_with_client_certificate(lc,sai);
	} else {
		return FALSE;
	}
}

static void notify_refer(SalOp *op, SalReferStatus status){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	LinphoneCall *call=(LinphoneCall*) sal_op_get_user_pointer(op);
	LinphoneCallState cstate;
	if (call==NULL) {
		ms_warning("Receiving notify_refer for unknown call.");
		return ;
	}
	switch(status){
		case SalReferTrying:
			cstate=LinphoneCallOutgoingProgress;
		break;
		case SalReferSuccess:
			cstate=LinphoneCallConnected;
		break;
		case SalReferFailed:
			cstate=LinphoneCallError;
		break;
		default:
			cstate=LinphoneCallError;
	}
	linphone_call_set_transfer_state(call, cstate);
	if (cstate==LinphoneCallConnected){
		/*automatically terminate the call as the transfer is complete.*/
		linphone_core_terminate_call(lc,call);
	}
}

static LinphoneChatMessageState chatStatusSal2Linphone(SalTextDeliveryStatus status){
	switch(status){
		case SalTextDeliveryInProgress:
			return LinphoneChatMessageStateInProgress;
		case SalTextDeliveryDone:
			return LinphoneChatMessageStateDelivered;
		case SalTextDeliveryFailed:
			return LinphoneChatMessageStateNotDelivered;
	}
	return LinphoneChatMessageStateIdle;
}

static void text_delivery_update(SalOp *op, SalTextDeliveryStatus status){
	LinphoneChatMessage *chat_msg=(LinphoneChatMessage* )sal_op_get_user_pointer(op);

	if (chat_msg == NULL) {
		// Do not handle delivery status for isComposing messages.
		return;
	}

	chat_msg->state=chatStatusSal2Linphone(status);
	linphone_chat_message_store_state(chat_msg);
	if (chat_msg && chat_msg->cb) {
		ms_message("Notifying text delivery with status %i",chat_msg->state);
		chat_msg->cb(chat_msg
			,chat_msg->state
			,chat_msg->cb_ud);
	}
	if (status != SalTextDeliveryInProgress) { /*don't release op if progress*/
		linphone_chat_message_destroy(chat_msg);
	}
}

static void info_received(SalOp *op, const SalBody *body){
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	linphone_core_notify_info_message(lc,op,body);
}

static void subscribe_response(SalOp *op, SalSubscribeStatus status){
	LinphoneEvent *lev=(LinphoneEvent*)sal_op_get_user_pointer(op);
	const SalErrorInfo *ei=sal_op_get_error_info(op);
	
	if (lev==NULL) return;
	
	if (status==SalSubscribeActive){
		linphone_event_set_state(lev,LinphoneSubscriptionActive);
	}else if (status==SalSubscribePending){
		linphone_event_set_state(lev,LinphoneSubscriptionPending);
	}else{
		if (lev->subscription_state==LinphoneSubscriptionActive && ei->reason==SalReasonIOError){
			linphone_event_set_state(lev,LinphoneSubscriptionOutgoingProgress);
		}
		else linphone_event_set_state(lev,LinphoneSubscriptionError);
	}
}

static void notify(SalOp *op, SalSubscribeStatus st, const char *eventname, const SalBody *body){
	LinphoneEvent *lev=(LinphoneEvent*)sal_op_get_user_pointer(op);
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	LinphoneContent content;
	
	if (lev==NULL) {
		/*out of subscribe notify */
		lev=linphone_event_new_with_out_of_dialog_op(lc,op,LinphoneSubscriptionOutgoing,eventname);
	}
	if (lc->vtable.notify_received){
		const LinphoneContent *ct=linphone_content_from_sal_body(&content,body);
		if (ct) lc->vtable.notify_received(lc,lev,eventname,ct);
	}
	if (st!=SalSubscribeNone){
		linphone_event_set_state(lev,linphone_subscription_state_from_sal(st));
	}
}

static void subscribe_received(SalOp *op, const char *eventname, const SalBody *body){
	LinphoneEvent *lev=(LinphoneEvent*)sal_op_get_user_pointer(op);
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal_op_get_sal(op));
	
	if (lev==NULL) {
		lev=linphone_event_new_with_op(lc,op,LinphoneSubscriptionIncoming,eventname);
		linphone_event_set_state(lev,LinphoneSubscriptionIncomingReceived);
	}else{
		/*subscribe refresh, unhandled*/
	}
	
}

static void subscribe_closed(SalOp *op){
	LinphoneEvent *lev=(LinphoneEvent*)sal_op_get_user_pointer(op);
	
	linphone_event_set_state(lev,LinphoneSubscriptionTerminated);
}

static void on_publish_response(SalOp* op){
	LinphoneEvent *lev=(LinphoneEvent*)sal_op_get_user_pointer(op);
	const SalErrorInfo *ei=sal_op_get_error_info(op);
	
	if (lev==NULL) return;
	if (ei->reason==SalReasonNone){
		if (!lev->terminating)
			linphone_event_set_publish_state(lev,LinphonePublishOk);
		else 
			linphone_event_set_publish_state(lev,LinphonePublishCleared);
	}else{
		if (lev->publish_state==LinphonePublishOk){
			linphone_event_set_publish_state(lev,LinphonePublishProgress);
		}else{
			linphone_event_set_publish_state(lev,LinphonePublishError);
		}
	}
}

static void on_expire(SalOp *op){
	LinphoneEvent *lev=(LinphoneEvent*)sal_op_get_user_pointer(op);
	
	if (lev==NULL) return;
	
	if (linphone_event_get_publish_state(lev)==LinphonePublishOk){
		linphone_event_set_publish_state(lev,LinphonePublishExpiring);
	}else if (linphone_event_get_subscription_state(lev)==LinphoneSubscriptionActive){
		linphone_event_set_state(lev,LinphoneSubscriptionExpiring);
	}
}

SalCallbacks linphone_sal_callbacks={
	call_received,
	call_ringing,
	call_accepted,
	call_ack,
	call_updating,
	call_terminated,
	call_failure,
	call_released,
	auth_failure,
	register_success,
	register_failure,
	vfu_request,
	dtmf_received,
	refer_received,
	text_received,
	text_delivery_update,
	is_composing_received,
	notify_refer,
	subscribe_received,
	subscribe_closed,
	subscribe_response,
	notify,
	subscribe_presence_received,
	subscribe_presence_closed,
	parse_presence_requested,
	convert_presence_to_xml_requested,
	notify_presence,
	ping_reply,
	auth_requested,
	info_received,
	on_publish_response,
	on_expire
};


