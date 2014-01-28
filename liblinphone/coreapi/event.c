/*
linphone
Copyright (C) 2000 - 2010 Simon MORLAT (simon.morlat@linphone.org)

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

#include "private.h"
#include "lpconfig.h"


LinphoneSubscriptionState linphone_subscription_state_from_sal(SalSubscribeStatus ss){
	switch(ss){
		case SalSubscribeNone: return LinphoneSubscriptionNone;
		case SalSubscribePending: return LinphoneSubscriptionPending;
		case SalSubscribeTerminated: return LinphoneSubscriptionTerminated;
		case SalSubscribeActive: return LinphoneSubscriptionActive;
	}
	return LinphoneSubscriptionNone;
}

const char *linphone_subscription_state_to_string(LinphoneSubscriptionState state){
	switch(state){
		case LinphoneSubscriptionNone: return "LinphoneSubscriptionNone";
		case LinphoneSubscriptionIncomingReceived: return "LinphoneSubscriptionIncomingReceived";
		case LinphoneSubscriptionOutoingInit: return "LinphoneSubscriptionOutoingInit";
		case LinphoneSubscriptionPending: return "LinphoneSubscriptionPending";
		case LinphoneSubscriptionActive: return "LinphoneSubscriptionActive";
		case LinphoneSubscriptionTerminated: return "LinphoneSubscriptionTerminated";
		case LinphoneSubscriptionError: return "LinphoneSubscriptionError";
		case LinphoneSubscriptionExpiring: return "LinphoneSubscriptionExpiring";
	}
	return NULL;
}

LINPHONE_PUBLIC const char *linphone_publish_state_to_string(LinphonePublishState state){
	switch(state){
		case LinphonePublishNone: return "LinphonePublishNone";
		case LinphonePublishProgress: return "LinphonePublishProgress";
		case LinphonePublishOk: return "LinphonePublishOk";
		case LinphonePublishError: return "LinphonePublishError";
		case LinphonePublishCleared: return "LinphonePublishCleared";
		case LinphonePublishExpiring: return "LinphonePublishExpiring";
	}
	return NULL;
}

static LinphoneEvent * linphone_event_new_base(LinphoneCore *lc, LinphoneSubscriptionDir dir, const char *name, SalOp *op){
	LinphoneEvent *lev=ms_new0(LinphoneEvent,1);
	lev->lc=lc;
	lev->dir=dir;
	lev->op=op;
	lev->refcnt=1;
	lev->name=ms_strdup(name);
	sal_op_set_user_pointer(lev->op,lev);
	return lev;
}

LinphoneEvent *linphone_event_new(LinphoneCore *lc, LinphoneSubscriptionDir dir, const char *name){
	LinphoneEvent *lev=linphone_event_new_base(lc, dir, name, sal_op_new(lc->sal));
	return lev;
}

LinphoneEvent *linphone_event_new_with_op(LinphoneCore *lc, SalOp *op, LinphoneSubscriptionDir dir, const char *name){
	LinphoneEvent *lev=linphone_event_new_base(lc, dir, name, op);
	if (dir==LinphoneSubscriptionIncoming){
		lev->resource_addr=linphone_address_clone((LinphoneAddress*)sal_op_get_to_address(op));
		lev->from=linphone_address_clone((LinphoneAddress*)sal_op_get_from_address(lev->op));
	}else{
		lev->resource_addr=linphone_address_clone((LinphoneAddress*)sal_op_get_from_address(op));
	}
	return lev;
}

void linphone_event_set_state(LinphoneEvent *lev, LinphoneSubscriptionState state){
	LinphoneCore *lc=lev->lc;
	if (lev->subscription_state!=state){
		ms_message("LinphoneEvent [%p] moving to subscription state %s",lev,linphone_subscription_state_to_string(state));
		lev->subscription_state=state;
		if (lc->vtable.subscription_state_changed){
			lc->vtable.subscription_state_changed(lev->lc,lev,state);
		}
		if (state==LinphoneSubscriptionTerminated){
			linphone_event_unref(lev);
		}
	}
}

void linphone_event_set_publish_state(LinphoneEvent *lev, LinphonePublishState state){
	LinphoneCore *lc=lev->lc;
	if (lev->publish_state!=state){
		ms_message("LinphoneEvent [%p] moving to publish state %s",lev,linphone_publish_state_to_string(state));
		lev->publish_state=state;
		if (lc->vtable.publish_state_changed){
			lc->vtable.publish_state_changed(lev->lc,lev,state);
		}
	}
}

LinphonePublishState linphone_event_get_publish_state(const LinphoneEvent *lev){
	return lev->publish_state;
}

void linphone_event_set_reason(LinphoneEvent *lev, LinphoneReason reason){
	lev->reason=reason;
}

LinphoneReason linphone_event_get_reason(const LinphoneEvent *lev){
	return lev->reason;
}

LinphoneEvent *linphone_core_subscribe(LinphoneCore *lc, const LinphoneAddress *resource, const char *event, int expires, const LinphoneContent *body){
	LinphoneEvent *lev=linphone_event_new(lc, LinphoneSubscriptionOutgoing, event);
	SalBody salbody;
	linphone_configure_op(lc,lev->op,resource,NULL,TRUE);
	sal_op_set_manual_refresher_mode(lev->op,!lp_config_get_int(lc->config,"sip","refresh_generic_subscribe",1));
	lev->resource_addr=linphone_address_clone(resource);
	lev->from=linphone_address_clone((LinphoneAddress*)sal_op_get_from_address(lev->op));
	sal_subscribe(lev->op,NULL,NULL,event,expires,sal_body_from_content(&salbody,body));
	linphone_event_set_state(lev,LinphoneSubscriptionOutoingInit);
	return lev;
}


int linphone_event_update_subscribe(LinphoneEvent *lev, const LinphoneContent *body){
	SalBody salbody;
	switch (lev->subscription_state){
		case LinphoneSubscriptionNone:
			ms_error("linphone_event_update_subscribe(): this is not a subscribed event.");
			return -1;
		break;
		case LinphoneSubscriptionIncomingReceived:
		case LinphoneSubscriptionOutoingInit:
		case LinphoneSubscriptionTerminated:
			ms_error("linphone_event_update_subscribe(): cannot update subscription while in state [%s]", linphone_subscription_state_to_string(lev->subscription_state));
			return -1;
		break;
		case LinphoneSubscriptionActive:
		case LinphoneSubscriptionExpiring:
		case LinphoneSubscriptionError:
		case LinphoneSubscriptionPending:
			/*those states are ok*/
		break;
	}
	if (lev->dir!=LinphoneSubscriptionOutgoing){
		ms_error("linphone_event_deny_subscription(): cannot update an incoming subscription.");
		return -1;
	}
	return sal_subscribe(lev->op,NULL,NULL,NULL,-1,sal_body_from_content(&salbody,body));
}

int linphone_event_accept_subscription(LinphoneEvent *lev){
	int err;
	if (lev->subscription_state!=LinphoneSubscriptionIncomingReceived){
		ms_error("linphone_event_accept_subscription(): cannot accept subscription if subscription wasn't just received.");
		return -1;
	}
	err=sal_subscribe_accept(lev->op);
	if (err==0){
		linphone_event_set_state(lev,LinphoneSubscriptionActive);
	}
	return err;
}

int linphone_event_deny_subscription(LinphoneEvent *lev, LinphoneReason reason){
	int err;
	if (lev->subscription_state!=LinphoneSubscriptionIncomingReceived){
		ms_error("linphone_event_deny_subscription(): cannot deny subscription if subscription wasn't just received.");
		return -1;
	}
	err=sal_subscribe_decline(lev->op,linphone_reason_to_sal(reason));
	linphone_event_set_state(lev,LinphoneSubscriptionTerminated);
	return err;
}

int linphone_event_notify(LinphoneEvent *lev, const LinphoneContent *body){
	SalBody salbody;
	if (lev->subscription_state!=LinphoneSubscriptionActive){
		ms_error("linphone_event_notify(): cannot notify if subscription is not active.");
		return -1;
	}
	if (lev->dir!=LinphoneSubscriptionIncoming){
		ms_error("linphone_event_notify(): cannot notify if not an incoming subscription.");
		return -1;
	}
	return sal_notify(lev->op,sal_body_from_content(&salbody,body));
}

LinphoneEvent *linphone_core_publish(LinphoneCore *lc, const LinphoneAddress *resource, const char *event, int expires, const LinphoneContent *body){
	SalBody salbody;
	int err;
	LinphoneEvent *lev=linphone_event_new(lc,LinphoneSubscriptionInvalidDir, event);
	linphone_configure_op(lc,lev->op,resource,NULL,lp_config_get_int(lc->config,"sip","publish_msg_with_contact",0));
	sal_op_set_manual_refresher_mode(lev->op,!lp_config_get_int(lc->config,"sip","refresh_generic_publish",1));
	err=sal_publish(lev->op,NULL,NULL,event,expires,sal_body_from_content(&salbody,body));
	if (err==0){
		linphone_event_set_publish_state(lev,LinphonePublishProgress);
	}else{
		linphone_event_unref(lev);
		lev=NULL;
	}
	return lev;
}

int linphone_event_update_publish(LinphoneEvent *lev, const LinphoneContent *body){
	SalBody salbody;
	int err;
	
	if (lev->publish_state==LinphonePublishNone){
		ms_error("linphone_event_update_publish(): this is not a PUBLISH event.");
		return -1;
	}
	
	err=sal_publish(lev->op,NULL,NULL,NULL,-1,sal_body_from_content(&salbody,body));
	if (err==0){
		linphone_event_set_publish_state(lev,LinphonePublishProgress);
	}else{
		linphone_event_set_publish_state(lev,LinphonePublishError);
	}
	return err;
}

void linphone_event_set_user_data(LinphoneEvent *ev, void *up){
	ev->userdata=up;
}

void *linphone_event_get_user_data(const LinphoneEvent *ev){
	return ev->userdata;
}

void linphone_event_terminate(LinphoneEvent *lev){
	lev->terminating=TRUE;
	if (lev->dir==LinphoneSubscriptionIncoming){
		sal_notify_close(lev->op);
	}else if (lev->dir==LinphoneSubscriptionOutgoing){
		sal_unsubscribe(lev->op);
	}
	
	if (lev->publish_state!=LinphonePublishNone){
		if (lev->publish_state==LinphonePublishOk){
			sal_publish(lev->op,NULL,NULL,NULL,0,NULL);
		}
		return;
	}
	
	if (lev->subscription_state!=LinphoneSubscriptionNone){
		linphone_event_set_state(lev,LinphoneSubscriptionTerminated);
		return;
	}
	
}


LinphoneEvent *linphone_event_ref(LinphoneEvent *lev){
	lev->refcnt++;
	return lev;
}

static void linphone_event_destroy(LinphoneEvent *lev){
	if (lev->op)
		sal_op_release(lev->op);
	ms_free(lev->name);
	if (lev->resource_addr) linphone_address_destroy(lev->resource_addr);
	if (lev->from) linphone_address_destroy(lev->from);
	ms_free(lev);
}

void linphone_event_unref(LinphoneEvent *lev){
	lev->refcnt--;
	if (lev->refcnt==0) linphone_event_destroy(lev);
}

LinphoneSubscriptionDir linphone_event_get_subscription_dir(LinphoneEvent *lev){
	return lev->dir;
}

LinphoneSubscriptionState linphone_event_get_subscription_state(const LinphoneEvent *lev){
	return lev->subscription_state;
}

const char *linphone_event_get_name(const LinphoneEvent *lev){
	return lev->name;
}

const LinphoneAddress *linphone_event_get_from(const LinphoneEvent *lev){
	return lev->from;
}

const LinphoneAddress *linphone_event_get_resource(const LinphoneEvent *lev){
	return lev->resource_addr;
}

LinphoneCore *linphone_event_get_core(const LinphoneEvent *lev){
	return lev->lc;
}

