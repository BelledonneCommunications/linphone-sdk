/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2012  Belledonne Communications SARL

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

#include "belle_sip_internal.h"
#include "belle-sip/refresher.h"

#define DEFAULT_RETRY_AFTER 60000
#define DEFAULT_INITIAL_RETRY_AFTER_ON_IO_ERROR 500

typedef enum belle_sip_refresher_state {
	started,
	stopped
}belle_sip_refresher_state_t;

typedef enum timer_purpose{
	NORMAL_REFRESH,
	RETRY
}timer_purpose_t;

struct belle_sip_refresher {
	belle_sip_object_t obj;
	belle_sip_refresher_listener_t listener;
	belle_sip_source_t* timer;
	belle_sip_client_transaction_t* transaction;
	int target_expires;
	int obtained_expires;
	belle_sip_refresher_state_t state;
	void* user_data;
	int retry_after;
	belle_sip_list_t* auth_events;
	int auth_failures;
	int on_io_error; /*flag to avoid multiple error notification*/
	int number_of_retry; /*counter to count number of unsuccesfull retry, used to know when to retry*/
	timer_purpose_t timer_purpose;
	unsigned char manual;
};
static int set_expires_from_trans(belle_sip_refresher_t* refresher);

static int timer_cb(void *user_data, unsigned int events) ;
static int belle_sip_refresher_refresh_internal(belle_sip_refresher_t* refresher,int expires,int auth_mandatory, belle_sip_list_t** auth_infos, belle_sip_uri_t *requri);


static void schedule_timer_at(belle_sip_refresher_t* refresher,int delay, timer_purpose_t purpose) {
	belle_sip_message("Refresher: scheduling next timer in %i ms",delay);
	refresher->timer_purpose=purpose;
	if (refresher->timer){
		belle_sip_main_loop_remove_source(belle_sip_stack_get_main_loop(refresher->transaction->base.provider->stack),refresher->timer);
		belle_sip_object_unref(refresher->timer);
	}
	refresher->timer=belle_sip_timeout_source_new(timer_cb,refresher,delay);
	belle_sip_object_set_name((belle_sip_object_t*)refresher->timer,"Refresher timeout");
	belle_sip_main_loop_add_source(belle_sip_stack_get_main_loop(refresher->transaction->base.provider->stack),refresher->timer);
}

static void retry_later(belle_sip_refresher_t* refresher) {
	refresher->number_of_retry++;
	schedule_timer_at(refresher,refresher->retry_after,RETRY);
}

static void retry_later_on_io_error(belle_sip_refresher_t* refresher) {
	/*if first retry, sent it in 500 ms*/
	if (refresher->number_of_retry < 1) {
		schedule_timer_at(refresher,DEFAULT_INITIAL_RETRY_AFTER_ON_IO_ERROR,RETRY);
		refresher->number_of_retry++;
	} else {
		retry_later(refresher);
	}
}

static void schedule_timer(belle_sip_refresher_t* refresher) {
	schedule_timer_at(refresher,refresher->obtained_expires*900,NORMAL_REFRESH);
}

static void process_dialog_terminated(belle_sip_listener_t *user_ctx, const belle_sip_dialog_terminated_event_t *event){
	/*nop*/
}
static void process_io_error(belle_sip_listener_t *user_ctx, const belle_sip_io_error_event_t *event){
	belle_sip_refresher_t* refresher=(belle_sip_refresher_t*)user_ctx;
	belle_sip_client_transaction_t*client_transaction;
	if (refresher->on_io_error==1) {
		return; /*refresher already on error*/
	}
	if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(belle_sip_io_error_event_get_source(event),belle_sip_client_transaction_t)) {
		client_transaction=BELLE_SIP_CLIENT_TRANSACTION(belle_sip_io_error_event_get_source(event));
		if (!refresher || (refresher && ((refresher->state==stopped
			&& belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(refresher->transaction)) != BELLE_SIP_TRANSACTION_TRYING
			&& belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(refresher->transaction)) != BELLE_SIP_TRANSACTION_INIT /*to cover dns or certificate error*/)
			|| client_transaction !=refresher->transaction )))
				return; /*not for me or no longuer involved*/

		if (refresher->target_expires==0
				&& belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(refresher->transaction)) != BELLE_SIP_TRANSACTION_TRYING
				&& belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(refresher->transaction)) != BELLE_SIP_TRANSACTION_INIT ) {
			return; /*not for me or no longuer involved because expire=0*/
		}
		if (refresher->state==started) retry_later_on_io_error(refresher);
		if (refresher->listener) refresher->listener(refresher,refresher->user_data,503, "io error");

		return;
	} else if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(belle_sip_io_error_event_get_source(event),belle_sip_provider_t)) {
		/*something went wrong on this provider, checking if my channel is still up*/
		if (refresher->state==started  /*refresher started or trying to refresh */
				&& belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(refresher->transaction)) == BELLE_SIP_TRANSACTION_TERMINATED /*else we are notified by transaction error*/
				&& refresher->transaction->base.channel /*transaction may not have any channel*/
				&&	(belle_sip_channel_get_state(refresher->transaction->base.channel) == BELLE_SIP_CHANNEL_DISCONNECTED
								||belle_sip_channel_get_state(refresher->transaction->base.channel) == BELLE_SIP_CHANNEL_ERROR)) {
			belle_sip_message("refresher [%p] has channel [%p] in state [%s], reporting error"
								,refresher
								,refresher->transaction->base.channel
								,belle_sip_channel_state_to_string(belle_sip_channel_get_state(refresher->transaction->base.channel)));
			if (refresher->state==started) retry_later_on_io_error(refresher);
			if (refresher->listener) refresher->listener(refresher,refresher->user_data,503, "io error");
			refresher->on_io_error=1;
		}
		return;
	}else {
		/*belle_sip_error("Refresher process_io_error not implemented yet for non transaction/provider source");*/
		/*nop, because already handle at transaction layer*/
	}
}

belle_sip_header_contact_t* get_first_contact_in_unknown_state(belle_sip_request_t *req){
	/*check if automatic contacts could be set by provider, if not resubmit the request immediately.*/
	belle_sip_header_contact_t *contact;
	const belle_sip_list_t *l;
	for (l=belle_sip_message_get_headers((belle_sip_message_t*)req,"Contact");l!=NULL;l=l->next){
		contact=(belle_sip_header_contact_t*)l->data;
		if (belle_sip_header_contact_is_unknown(contact)){
			return contact;
		}
	}
	return NULL;
}

static int is_contact_address_acurate(const belle_sip_refresher_t* refresher,belle_sip_request_t* request) {
	belle_sip_header_contact_t* contact;
	if ((contact = get_first_contact_in_unknown_state(request))){
		/*check if contact ip/port is consistant with  public channel ip/port*/
		int channel_public_port = refresher->transaction->base.channel->public_port;
		int contact_port = belle_sip_uri_get_listening_port(belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(contact)));
		const char* channel_public_ip = refresher->transaction->base.channel->public_ip;
		const char* contact_ip = belle_sip_uri_get_host(belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(contact)));

		if (channel_public_port == contact_port
				&& channel_public_ip && contact_ip
				&& strcmp(channel_public_ip,contact_ip) == 0) {
			/*nothing to do contact is accurate*/
			belle_sip_header_contact_set_unknown(contact,FALSE);
			return TRUE;
		} else {
			belle_sip_message("Refresher [%p]: contact address [%s:%i] does not match channel address[%s:%i]."	,refresher
					,contact_ip
					,contact_port
					,channel_public_ip
					,channel_public_port);
			return FALSE;
		}
	} else {
		belle_sip_message("Refresher [%p]:  has no contact for request [%p].", refresher,request);
		return TRUE;
	}
}

static void process_response_event(belle_sip_listener_t *user_ctx, const belle_sip_response_event_t *event){
	belle_sip_client_transaction_t* client_transaction = belle_sip_response_event_get_client_transaction(event);
	belle_sip_response_t* response = belle_sip_response_event_get_response(event);
	belle_sip_request_t* request=belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(client_transaction));
	int response_code = belle_sip_response_get_status_code(response);
	belle_sip_refresher_t* refresher=(belle_sip_refresher_t*)user_ctx;
	belle_sip_header_contact_t *contact;

	if (refresher && (client_transaction !=refresher->transaction))
		return; /*not for me*/

	/*handle authorization*/
	switch (response_code) {
	case 200:
		refresher->auth_failures=0;
		refresher->number_of_retry=0;
		/*great, success*/
		if (strcmp(belle_sip_request_get_method(request),"PUBLISH")==0) {
			/*search for etag*/
			belle_sip_header_t* etag=belle_sip_message_get_header(BELLE_SIP_MESSAGE(response),"SIP-ETag");
			if (etag) {
				belle_sip_header_t* sip_if_match = belle_sip_header_create("SIP-If-Match",belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(etag)));
				/*update request for next refresh*/
				belle_sip_message_remove_header(BELLE_SIP_MESSAGE(request),"SIP-If-Match");
				belle_sip_message_add_header(BELLE_SIP_MESSAGE(request),sip_if_match);
			} else {
				belle_sip_warning("Refresher [%p] receive 200ok to a publish without etag",refresher);
			}
		}
		/*update expire if needed*/
		set_expires_from_trans(refresher);
		if (refresher->target_expires<=0) {
			belle_sip_refresher_stop(refresher); /*doesn't not make sense to refresh if expire =0;*/
		}
		if (refresher->state==started) {
			if (is_contact_address_acurate(refresher,request)) {
				schedule_timer(refresher); /*re-arm timer*/
			} else {
				belle_sip_message("belle_sip_refresher_start(): refresher [%p] is resubmitting request because contact sent was not correct in original request.",refresher);
				belle_sip_refresher_refresh(refresher,refresher->target_expires);
				return;
			}
		}
		else belle_sip_message("Refresher [%p] not scheduling next refresh, because it was stopped",refresher);
		break;
	case 301:
	case 302:
		contact=belle_sip_message_get_header_by_type(response,belle_sip_header_contact_t);
		if (contact){
			if (belle_sip_refresher_refresh_internal(refresher,refresher->target_expires,TRUE,&refresher->auth_events,NULL)==0)
				return;
		}
		break;
	case 401:
	case 407:
		refresher->auth_failures++;
		if (refresher->auth_failures>3){
			/*avoid looping with 407 or 401 */
			belle_sip_warning("Authentication is failing constantly, giving up.");
			if (refresher->target_expires>0) retry_later(refresher);
			break;
		}
		if (refresher->auth_events) {
			refresher->auth_events=belle_sip_list_free_with_data(refresher->auth_events,(void (*)(void*))belle_sip_auth_event_destroy);
		}
		if (belle_sip_refresher_refresh_internal(refresher,refresher->target_expires,TRUE,&refresher->auth_events,NULL)==0)
			return; /*ok, keep 401 internal*/
		break; /*Else notify user of registration failure*/
	case 423:{
		belle_sip_header_extension_t *min_expires=BELLE_SIP_HEADER_EXTENSION(belle_sip_message_get_header((belle_sip_message_t*)response,"Min-Expires"));
		if (min_expires){
			const char *value=belle_sip_header_extension_get_value(min_expires);
			if (value){
				int new_expires=atoi(value);
				if (new_expires>0 && refresher->state==started){
					refresher->target_expires=new_expires;
					belle_sip_refresher_refresh(refresher,refresher->target_expires);
					return;
				}
			}
		}else belle_sip_warning("Receiving 423 but no min-expires header.");
		break;
	}
	case 408:
	case 480:
	case 503:
	case 504:
		if (refresher->target_expires>0) retry_later(refresher);
		break;
	default:
		break;
	}
	if (refresher->listener) refresher->listener(refresher,refresher->user_data,response_code, belle_sip_response_get_reason_phrase(response));

}

static void process_timeout(belle_sip_listener_t *user_ctx, const belle_sip_timeout_event_t *event) {
	belle_sip_refresher_t* refresher=(belle_sip_refresher_t*)user_ctx;
	belle_sip_client_transaction_t*client_transaction =belle_sip_timeout_event_get_client_transaction(event);

	if (refresher && (client_transaction !=refresher->transaction))
		return; /*not for me*/

	if (refresher->state==started) {
		/*retry in 2 seconds but not immediately to let the current transaction be cleaned*/
		schedule_timer_at(refresher,2000,RETRY);
	}
	if (refresher->listener) refresher->listener(refresher,refresher->user_data,408, "timeout");
}

static void process_transaction_terminated(belle_sip_listener_t *user_ctx, const belle_sip_transaction_terminated_event_t *event) {
	/*belle_sip_message("process_transaction_terminated Transaction terminated [%p]",event);*/
}

static void process_auth_requested(belle_sip_listener_t *l, belle_sip_auth_event_t *event){
}

static void process_request_event(belle_sip_listener_t *user_ctx, const belle_sip_request_event_t *event){
}

static void destroy(belle_sip_refresher_t *refresher){
	belle_sip_refresher_stop(refresher);
	belle_sip_provider_remove_internal_sip_listener(refresher->transaction->base.provider,BELLE_SIP_LISTENER(refresher));
	belle_sip_object_unref(refresher->transaction);
	refresher->transaction=NULL;
	if (refresher->auth_events) refresher->auth_events=belle_sip_list_free_with_data(refresher->auth_events,(void (*)(void*))belle_sip_auth_event_destroy);
}

BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(belle_sip_refresher_t,belle_sip_listener_t)
	process_dialog_terminated,
	process_io_error,
	process_request_event,
	process_response_event,
	process_timeout,
	process_transaction_terminated,
	process_auth_requested
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(belle_sip_refresher_t, belle_sip_listener_t);

BELLE_SIP_INSTANCIATE_VPTR(belle_sip_refresher_t, belle_sip_object_t,destroy, NULL, NULL,FALSE);

void belle_sip_refresher_set_listener(belle_sip_refresher_t* refresher, belle_sip_refresher_listener_t listener,void* user_pointer) {
	refresher->listener=listener;
	refresher->user_data=user_pointer;
}

int belle_sip_refresher_refresh(belle_sip_refresher_t* refresher,int expires) {
	return belle_sip_refresher_refresh_internal(refresher,expires,FALSE,NULL,NULL);
}

static int unfilled_auth_info(const void* info,const void* userptr) {
	belle_sip_auth_event_t* auth_info = (belle_sip_auth_event_t*)info;
	return auth_info->passwd || auth_info->ha1;
}

static int belle_sip_refresher_refresh_internal(belle_sip_refresher_t* refresher, int expires, int auth_mandatory, belle_sip_list_t** auth_infos, belle_sip_uri_t *requri) {
	belle_sip_request_t*old_request=belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(refresher->transaction));
	belle_sip_response_t*old_response=belle_sip_transaction_get_response(BELLE_SIP_TRANSACTION(refresher->transaction));
	belle_sip_dialog_t* dialog = belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(refresher->transaction));
	belle_sip_client_transaction_t* client_transaction;
	belle_sip_request_t* request;
	belle_sip_header_expires_t* expires_header;
	belle_sip_uri_t* preset_route=refresher->transaction->preset_route;
	belle_sip_provider_t* prov=refresher->transaction->base.provider;
	belle_sip_header_contact_t* contact;
	
	/*first remove timer if any*/
	if (expires >=0) {
		refresher->target_expires=expires;
	} else {
		/*-1 keep last value*/
	}

	if (!dialog) {
		const belle_sip_transaction_state_t state=belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(refresher->transaction));
		/*create new request*/
		if (belle_sip_transaction_state_is_transient(state)) {
			/*operation pending, cannot update authorization headers*/
			belle_sip_header_cseq_t* cseq;
			belle_sip_message("Refresher [%p] already have transaction [%p] in state [%s]"	,refresher
				,refresher->transaction
				,belle_sip_transaction_state_to_string(state));
			request=belle_sip_request_clone_with_body(belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(refresher->transaction)));
			cseq=belle_sip_message_get_header_by_type(request,belle_sip_header_cseq_t);
			belle_sip_header_cseq_set_seq_number(cseq,belle_sip_header_cseq_get_seq_number(cseq)+1);
		} else {
			request=belle_sip_client_transaction_create_authenticated_request(refresher->transaction,auth_infos);
		}
		if (requri){
			/*case where we are redirected*/
			belle_sip_request_set_uri(request,requri);
			/*remove auth headers, they are not valid for new destination*/
			belle_sip_message_remove_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_AUTHORIZATION);
			belle_sip_message_remove_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_PROXY_AUTHORIZATION);
		}
	} else if (dialog && belle_sip_dialog_get_state(dialog)==BELLE_SIP_DIALOG_CONFIRMED) {
		if (belle_sip_dialog_request_pending(dialog)){
			belle_sip_message("Cannot refresh now, there is a pending request in the dialog.");
			return -1;
		}
		request=belle_sip_dialog_create_request_from(dialog,old_request);
		if (strcmp(belle_sip_request_get_method(request),"SUBSCRIBE")==0) {
			/*put expire header*/
			if (!(expires_header = belle_sip_message_get_header_by_type(request,belle_sip_header_expires_t))) {
				expires_header = belle_sip_header_expires_new();
				belle_sip_message_add_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_HEADER(expires_header));
			}
		}
		belle_sip_provider_add_authorization(prov,request,old_response,auth_infos);
	} else {
		belle_sip_error("Unexpected dialog state [%s] for dialog [%p], cannot refresh [%s]"
				,belle_sip_dialog_state_to_string(belle_sip_dialog_get_state(dialog))
				,dialog
				,belle_sip_request_get_method(old_request));
		return -1;
	}

	if (auth_mandatory && auth_infos && belle_sip_list_find_custom(*auth_infos, unfilled_auth_info, NULL)) {
		belle_sip_message("Auth info not found for this refresh operation on [%p]",refresher);
		if (request) belle_sip_object_unref(request);
		return -1;
	}
	
	refresher->on_io_error=0; /*reset this flag*/

	/*update expires in any cases*/
	expires_header = belle_sip_message_get_header_by_type(request,belle_sip_header_expires_t);
	if (expires_header)
		belle_sip_header_expires_set_expires(expires_header,refresher->target_expires);
	contact=belle_sip_message_get_header_by_type(request,belle_sip_header_contact_t);
	if (contact && belle_sip_header_contact_get_expires(contact)>=0)
		belle_sip_header_contact_set_expires(contact,refresher->target_expires);

	/*update the Date header if it exists*/
	{
		belle_sip_header_date_t *date=belle_sip_message_get_header_by_type(request,belle_sip_header_date_t);
		if (date){
			time_t curtime=time(NULL);
			belle_sip_header_date_set_time(date,&curtime);
		}
	}
	
	client_transaction = belle_sip_provider_create_client_transaction(prov,request);
	client_transaction->base.is_internal=1;
	belle_sip_transaction_set_application_data(BELLE_SIP_TRANSACTION(client_transaction),refresher);
	/*update reference transaction for next refresh*/
	belle_sip_object_unref(refresher->transaction);
	refresher->transaction=client_transaction;
	belle_sip_object_ref(refresher->transaction);

	if (belle_sip_client_transaction_send_request_to(client_transaction,preset_route)) {
		belle_sip_error("Cannot send refresh method [%s] for refresher [%p]"
				,belle_sip_request_get_method(request)
				,refresher);
		return -1;
	}
	if (expires==0) belle_sip_refresher_stop(refresher);
	return 0;
}


static int timer_cb(void *user_data, unsigned int events) {
	belle_sip_refresher_t* refresher = (belle_sip_refresher_t*)user_data;
	
	refresher->auth_failures=0;/*reset the auth_failures to get a chance to authenticate again*/
	
	if (refresher->timer_purpose==NORMAL_REFRESH && refresher->manual) {
		belle_sip_message("Refresher [%p] is in manual mode, skipping refresh.",refresher);
		/*call listener with special code 0 to indicate request is about to expire*/
		if (refresher->listener) refresher->listener(refresher,refresher->user_data,0, "about to expire");
		return BELLE_SIP_STOP;
	}
	
	if (belle_sip_refresher_refresh(refresher,refresher->target_expires)==-1){
		retry_later(refresher);
	}
	return BELLE_SIP_STOP;
}

static belle_sip_header_contact_t* get_matching_contact(const belle_sip_transaction_t* transaction) {
	belle_sip_request_t*request=belle_sip_transaction_get_request(transaction);
	belle_sip_response_t*response=transaction->last_response;
	const belle_sip_list_t* contact_header_list;
	belle_sip_header_contact_t* unfixed_local_contact;
	belle_sip_header_contact_t* fixed_local_contact;
	char* tmp_string;
	char* tmp_string2;
	if (!response)
		return NULL;
	/*we assume, there is only one contact in request*/
	unfixed_local_contact= belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(request),belle_sip_header_contact_t);
	fixed_local_contact= BELLE_SIP_HEADER_CONTACT(belle_sip_object_clone(BELLE_SIP_OBJECT(unfixed_local_contact)));

	/*first fix contact using received/rport*/
	belle_sip_response_fix_contact(response,fixed_local_contact);
	contact_header_list = belle_sip_message_get_headers(BELLE_SIP_MESSAGE(response),BELLE_SIP_CONTACT);

	if (contact_header_list) {
		contact_header_list = belle_sip_list_find_custom((belle_sip_list_t*)contact_header_list
					,(belle_sip_compare_func)belle_sip_header_contact_not_equals
					, (const void*)fixed_local_contact);
		if (!contact_header_list) {
			/*reset header list*/
			contact_header_list = belle_sip_message_get_headers(BELLE_SIP_MESSAGE(response),BELLE_SIP_CONTACT);
			contact_header_list = belle_sip_list_find_custom((belle_sip_list_t*)contact_header_list
							,(belle_sip_compare_func)belle_sip_header_contact_not_equals
							,unfixed_local_contact);
		}
		if (!contact_header_list) {
			tmp_string=belle_sip_object_to_string(BELLE_SIP_OBJECT(fixed_local_contact));
			tmp_string2=belle_sip_object_to_string(BELLE_SIP_OBJECT(unfixed_local_contact));
			belle_sip_message("No matching contact neither for [%s] nor [%s]", tmp_string, tmp_string2);
			belle_sip_free(tmp_string);
			belle_sip_free(tmp_string2);
			return NULL;
		} else {
			return BELLE_SIP_HEADER_CONTACT(contact_header_list->data);
		}
	} else {
		return NULL;
	}

}
static int set_expires_from_trans(belle_sip_refresher_t* refresher) {
	belle_sip_transaction_t* transaction = BELLE_SIP_TRANSACTION(refresher->transaction);
	belle_sip_response_t*response=transaction->last_response;
	belle_sip_request_t*request=belle_sip_transaction_get_request(transaction);
	belle_sip_header_expires_t*  expires_header=belle_sip_message_get_header_by_type(request,belle_sip_header_expires_t);
	belle_sip_header_contact_t* contact_header;

	refresher->obtained_expires=-1;
	
	if (strcmp("REGISTER",belle_sip_request_get_method(request))==0
			|| expires_header /*if request has an expire header, refresher can always work*/) {

		if (expires_header)
			refresher->target_expires = belle_sip_header_expires_get_expires(expires_header);
		
		/*An "expires" parameter on the "Contact" header has no semantics for
		*   SUBSCRIBE and is explicitly not equivalent to an "Expires" header in
		*  a SUBSCRIBE request or response.
		*/
		if (strcmp("REGISTER",belle_sip_request_get_method(request))==0){
			if (!expires_header && (contact_header=belle_sip_message_get_header_by_type((belle_sip_message_t*)request,belle_sip_header_contact_t))){
				int ct_expires=belle_sip_header_contact_get_expires(BELLE_SIP_HEADER_CONTACT(contact_header));
				if (ct_expires!=-1) refresher->target_expires=ct_expires;
			}
			/*check in response also to get the obtained expires*/
			if ((contact_header=get_matching_contact(transaction))!=NULL){
				/*matching contact, check for its possible expires param*/
				refresher->obtained_expires=belle_sip_header_contact_get_expires(BELLE_SIP_HEADER_CONTACT(contact_header));
			}
		}
		if (refresher->obtained_expires==-1){
			/*no contact with expire or not relevant, looking for Expires header*/
			if (response && (expires_header=(belle_sip_header_expires_t*)belle_sip_message_get_header(BELLE_SIP_MESSAGE(response),BELLE_SIP_EXPIRES))) {
				refresher->obtained_expires = belle_sip_header_expires_get_expires(expires_header);
			}
		}
		if (refresher->obtained_expires==-1) {
			belle_sip_message("Neither Expires header nor corresponding Contact header found, checking from original request");
			refresher->obtained_expires=refresher->target_expires;
		}else if (refresher->target_expires>0 && refresher->obtained_expires==0){
			/*check this case because otherwise we are going to loop fast in sending refresh requests.*/
			belle_sip_warning("Server replied with 0 expires, what does this mean ?");
			/*suppose it's a server bug and assume our target_expires is understood.*/
			refresher->obtained_expires=refresher->target_expires;
		}
	} else if (strcmp("INVITE",belle_sip_request_get_method(request))==0) {
		belle_sip_error("Refresher does not support INVITE yet");
		return -1;
	} else {
		belle_sip_error("Refresher does not support [%s] yet",belle_sip_request_get_method(request));
		return -1;
	}
	return 0;
}


int belle_sip_refresher_start(belle_sip_refresher_t* refresher) {
	if(refresher->state==started) {
		belle_sip_warning("Refresher [%p] already started",refresher);
	} else {
		if (refresher->target_expires>0) {
			belle_sip_request_t* request = belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(refresher->transaction));
			refresher->state=started;
			if (is_contact_address_acurate(refresher,request)) {
				schedule_timer(refresher); /*re-arm timer*/
			} else {
				belle_sip_message("belle_sip_refresher_start(): refresher [%p] is resubmitting request because contact sent was not correct in original request.",refresher);
				belle_sip_refresher_refresh(refresher,refresher->target_expires);
				return 0;
			}
			belle_sip_message("Refresher [%p] started, next refresh in [%i] s",refresher,refresher->obtained_expires);
		}else{
			belle_sip_message("Refresher [%p] stopped, expires=%i",refresher,refresher->target_expires);
			refresher->state=stopped;
		}
	}
	return 0;
}

void belle_sip_refresher_stop(belle_sip_refresher_t* refresher) {
	belle_sip_message("Refresher [%p] stopped.",refresher);
	if (refresher->timer){
		belle_sip_main_loop_remove_source(belle_sip_stack_get_main_loop(refresher->transaction->base.provider->stack), refresher->timer);
		belle_sip_object_unref(refresher->timer);
		refresher->timer=NULL;
	}
	refresher->state=stopped;
}

belle_sip_refresher_t* belle_sip_refresher_new(belle_sip_client_transaction_t* transaction) {
	belle_sip_refresher_t* refresher;
	belle_sip_transaction_state_t state=belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(transaction));
	belle_sip_request_t* request = belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(transaction));
	int is_register=strcmp("REGISTER",belle_sip_request_get_method(request))==0;
	
	refresher = (belle_sip_refresher_t*)belle_sip_object_new(belle_sip_refresher_t);
	refresher->transaction=transaction;
	refresher->state=stopped;
	refresher->number_of_retry=0;
	belle_sip_object_ref(transaction);
	refresher->retry_after=DEFAULT_RETRY_AFTER;
	
	belle_sip_provider_add_internal_sip_listener(transaction->base.provider,BELLE_SIP_LISTENER(refresher), is_register);
	if (set_expires_from_trans(refresher)==-1){
		belle_sip_error("Unable to extract refresh value from transaction [%p]",transaction);
	}
	if (belle_sip_transaction_state_is_transient(state)) {
		belle_sip_message("Refresher [%p] takes ownership of transaction [%p]",refresher,transaction);
		transaction->base.is_internal=1;
		refresher->state=started;
	}else{
		belle_sip_refresher_start(refresher);
	}
	return refresher;
}

int belle_sip_refresher_get_expires(const belle_sip_refresher_t* refresher) {
	return refresher->target_expires;
}

int belle_sip_refresher_get_retry_after(const belle_sip_refresher_t* refresher){
	return refresher->retry_after;
}

void belle_sip_refresher_set_retry_after(belle_sip_refresher_t* refresher, int delay_ms) {
	refresher->retry_after=delay_ms;
}
const belle_sip_client_transaction_t* belle_sip_refresher_get_transaction(const belle_sip_refresher_t* refresher) {
	return refresher->transaction;
}
const belle_sip_list_t* belle_sip_refresher_get_auth_events(const belle_sip_refresher_t* refresher) {
	return refresher->auth_events;
}

void belle_sip_refresher_enable_manual_mode(belle_sip_refresher_t *refresher, int enabled){
	refresher->manual=enabled;
}

