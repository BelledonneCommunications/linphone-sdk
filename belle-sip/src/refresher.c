/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2012  Belledonne Communications SARL

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

#include "belle_sip_internal.h"
#include "belle-sip/refresher.h"

#define DEFAULT_RETRY_AFTER 60000
#define DEFAULT_INITIAL_RETRY_AFTER_ON_IO_ERROR 500

static void belle_sip_refresher_stop_internal(belle_sip_refresher_t* refresher,int cancel_pending_transaction) ;

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
	belle_sip_request_t* first_acknoleged_request; /*store first request sucessfully acknoleged, usefull to re-build a dialog iff needed*/
	belle_sip_dialog_t* dialog; /*Cannot rely on transaction to store dialog because of belle_sip_transaction_reset_dialog*/
	char* realm;
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
	unsigned int publish_pending;
};
static void set_or_update_dialog(belle_sip_refresher_t* refresher, belle_sip_dialog_t* dialog);
static int set_expires_from_trans(belle_sip_refresher_t* refresher);

static int timer_cb(void *user_data, unsigned int events) ;
static int belle_sip_refresher_refresh_internal(belle_sip_refresher_t* refresher,int expires,int auth_mandatory, belle_sip_list_t** auth_infos, belle_sip_uri_t *requri);

static void cancel_retry(belle_sip_refresher_t* refresher) {
	if (refresher->timer){
		belle_sip_main_loop_remove_source(belle_sip_stack_get_main_loop(refresher->transaction->base.provider->stack),refresher->timer);
		belle_sip_object_unref(refresher->timer);
		refresher->timer=NULL;
	}
}
static void schedule_timer_at(belle_sip_refresher_t* refresher,int delay, timer_purpose_t purpose) {
	belle_sip_message("Refresher[%p]: scheduling next timer in %i ms for purpose [%s]",refresher, delay,
		purpose == NORMAL_REFRESH ? "normal refresh" : "retry");
	refresher->timer_purpose=purpose;
	/*cancel timer if any*/
	cancel_retry(refresher);
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
	belle_sip_refresher_t* refresher=(belle_sip_refresher_t*)user_ctx;
	belle_sip_dialog_t *dialog =belle_sip_dialog_terminated_event_get_dialog(event);

	if (refresher && refresher->transaction && dialog != belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(refresher->transaction)))
		return; /*not for me*/

	if (belle_sip_dialog_expired(dialog) && refresher->state == started) {
		/* We notify the app that the dialog is expired due to failure to refresh the subscription on time.
		 * However the transaction to renew the dialog is either pending or already failed, and has scheduled a retry already,
		 * so there is no need to reschedule a retry here.*/
		belle_sip_warning("Refresher [%p] still started but expired, retrying",refresher);
		if (refresher->listener) refresher->listener(refresher,refresher->user_data,481, "dialod terminated", TRUE);
		
	}

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
		if (refresher->listener) refresher->listener(refresher,refresher->user_data,503, "io error", refresher->state == started);

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
			if (refresher->listener) refresher->listener(refresher,refresher->user_data,503, "io error", refresher->state == started);
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
			belle_sip_message("Refresher [%p]: contact address [%s:%i] does not match channel address[%s:%i] on channel [%p]"	,refresher
					,contact_ip
					,contact_port
					,channel_public_ip
					,channel_public_port
					,refresher->transaction->base.channel);
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
	int will_retry = TRUE; /*most error codes are retryable*/


	if (refresher && (client_transaction !=refresher->transaction))
		return; /*not for me*/

	set_or_update_dialog(refresher,belle_sip_response_event_get_dialog(event));
	/*success case:*/
	if (response_code>=200 && response_code<300){
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
			} else if (refresher->target_expires > 0){
				belle_sip_warning("Refresher [%p] received 200ok to a publish without etag",refresher);
			}
		}
		/*update expire if needed*/
		set_expires_from_trans(refresher);

		if (refresher->target_expires<=0) {
			belle_sip_refresher_stop(refresher); /*doesn't not make sense to refresh if expire =0;*/
		} else {
			/*remove all contact with expire = 0 from request if any, because no need to refresh them*/
			const belle_sip_list_t * contact_list= belle_sip_message_get_headers(BELLE_SIP_MESSAGE(request),BELLE_SIP_CONTACT);
			belle_sip_list_t *iterator, *head;
			if (contact_list) {
				for (iterator=head=belle_sip_list_copy(contact_list);iterator!=NULL;iterator=iterator->next) {
					belle_sip_header_contact_t *contact_for_expire = (belle_sip_header_contact_t *)(iterator->data);
					if (belle_sip_header_contact_get_expires(contact_for_expire) == 0) {
						belle_sip_message_remove_header_from_ptr(BELLE_SIP_MESSAGE(request),BELLE_SIP_HEADER(contact_for_expire));
					}
				}
				belle_sip_list_free(head);
			}
		}

		if (refresher->state==started) {
			if (!refresher->first_acknoleged_request)
				belle_sip_object_ref(refresher->first_acknoleged_request = request);
			if (is_contact_address_acurate(refresher,request)) {
				schedule_timer(refresher); /*re-arm timer*/
			} else {
				belle_sip_message("belle_sip_refresher_start(): refresher [%p] is resubmitting request because contact sent was not correct in original request.",refresher);
				belle_sip_refresher_refresh(refresher,refresher->target_expires);
				return;
			}
		}
		else belle_sip_message("Refresher [%p] not scheduling next refresh, because it was stopped",refresher);
	}else if (response_code >= 300) {/*special error cases*/
		switch (response_code) {
		case 301:
		case 302:
			contact=belle_sip_message_get_header_by_type(response,belle_sip_header_contact_t);
			if (contact){
				belle_sip_uri_t *uri=belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(contact));
				if (uri && belle_sip_refresher_refresh_internal(refresher,refresher->target_expires,TRUE,&refresher->auth_events,uri)==0)
					return;
			}
			break;
		case 401:
		case 407:
			refresher->auth_failures++;
			if (refresher->auth_failures>1){
				/*avoid looping with 407 or 401 */
				belle_sip_warning("Authentication is failing constantly, %s",(refresher->target_expires>0)? "will retry later":"giving up.");
				if (refresher->target_expires>0) retry_later(refresher);
				refresher->auth_failures=0; /*reset auth failure*/
				break;
			}
			if (refresher->auth_events) {
				refresher->auth_events=belle_sip_list_free_with_data(refresher->auth_events,(void (*)(void*))belle_sip_auth_event_destroy);
			}
			if (belle_sip_refresher_refresh_internal(refresher,refresher->target_expires,TRUE,&refresher->auth_events,NULL)==0)
				return; /*ok, keep 401 internal*/
			break; /*Else notify user of registration failure*/
		case 403:
			/*In case of 403, we will retry later, just in case*/
			if (refresher->target_expires>0) retry_later(refresher);
			else will_retry = FALSE;
			break;
		case 412:
			if (strcmp(belle_sip_request_get_method(request),"PUBLISH")==0) {
				belle_sip_message_remove_header(BELLE_SIP_MESSAGE(request),"SIP-If-Match");
				if (refresher->target_expires>0) {
					retry_later_on_io_error(refresher);
					return; /*do not notify this kind of error*/
				}else{
					will_retry = FALSE;
				}
			} else {
				if (refresher->target_expires>0) retry_later(refresher);
				else will_retry = FALSE;
			}
			break;
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
			will_retry = FALSE;
			break;
		}
		case 491: {
			if (refresher->target_expires>0) {
				int delay = belle_sip_random() % 10000; /*schedule a retry between 0 and 10 seconds*/
				schedule_timer_at(refresher, delay, RETRY);
				return; /*do not notify this kind of error*/
			}
		}
		/*intentionally no break*/
		case 505:
		case 501:
			/*irrecoverable errors, probably no need to retry later*/
			will_retry = FALSE;
			break;
		case 481:
		case 503:
			if (refresher->target_expires>0) {
				if (refresher->dialog) retry_later_on_io_error(refresher);
				else retry_later(refresher);
			}else will_retry = FALSE;
			break;
		default:
			/*for all other errors <600, retry later*/
			if (response_code < 600 && refresher->target_expires>0) retry_later(refresher);
			else will_retry = FALSE;
			break;
		}
	}
	if (refresher->listener) refresher->listener(refresher,refresher->user_data,response_code, belle_sip_response_get_reason_phrase(response), will_retry);

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
	if (refresher->listener) refresher->listener(refresher,refresher->user_data,408, "timeout", refresher->state==started);
}

static void process_transaction_terminated(belle_sip_listener_t *user_ctx, const belle_sip_transaction_terminated_event_t *event) {
	belle_sip_refresher_t* refresher=(belle_sip_refresher_t*)user_ctx;
	belle_sip_client_transaction_t*client_transaction = belle_sip_transaction_terminated_event_get_client_transaction(event);
	if (refresher && (client_transaction !=refresher->transaction))
		return; /*not for me*/

	if (refresher->publish_pending && refresher->state==started) {
		refresher->publish_pending = FALSE;
		belle_sip_message("Publish pending on refresher [%p], doing it",refresher);
		belle_sip_refresher_refresh(refresher,refresher->target_expires);
	} else {
		refresher->publish_pending = FALSE;
	}

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
	if (refresher->realm) belle_sip_free(refresher->realm);
	if (refresher->auth_events) refresher->auth_events=belle_sip_list_free_with_data(refresher->auth_events,(void (*)(void*))belle_sip_auth_event_destroy);
	if (refresher->first_acknoleged_request) belle_sip_object_unref(refresher->first_acknoleged_request);
	if (refresher->dialog) belle_sip_object_unref(refresher->dialog);
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
	/*first cancel any current retry*/
	cancel_retry(refresher);
	refresher->auth_failures=0;/*reset the auth_failures to get a chance to authenticate again*/
	return belle_sip_refresher_refresh_internal(refresher,expires,FALSE,NULL,NULL);
}

static int unfilled_auth_info(const void* info,const void* userptr) {
	belle_sip_auth_event_t* auth_info = (belle_sip_auth_event_t*)info;
	return auth_info->passwd || auth_info->ha1;
}

static int belle_sip_refresher_refresh_internal(belle_sip_refresher_t* refresher, int expires, int auth_mandatory, belle_sip_list_t** auth_infos, belle_sip_uri_t *requri) {
	belle_sip_request_t*old_request=belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(refresher->transaction));
	belle_sip_response_t*old_response=belle_sip_transaction_get_response(BELLE_SIP_TRANSACTION(refresher->transaction));
	belle_sip_dialog_t* dialog = refresher->dialog;
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
			belle_sip_message("Refresher [%p] already has transaction [%p] in state [%s]"	,refresher
				,refresher->transaction
				,belle_sip_transaction_state_to_string(state));

			if (strcmp(belle_sip_request_get_method(old_request),"PUBLISH")==0) {
				belle_sip_message("Refresher [%p] new publish is delayed to end of ongoing transaction"	,refresher);
				refresher->publish_pending = TRUE;
				return 0;
			} else if (strcmp(belle_sip_request_get_method(old_request),"SUBSCRIBE")==0)  {
				belle_sip_message("Cannot refresh now, there is a pending request for refresher [%p].",refresher);
				return -1;
			}else {
				request=belle_sip_request_clone_with_body(belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(refresher->transaction)));
				cseq=belle_sip_message_get_header_by_type(request,belle_sip_header_cseq_t);
				belle_sip_header_cseq_set_seq_number(cseq,belle_sip_header_cseq_get_seq_number(cseq)+1);
			}
		} else {
			request=belle_sip_client_transaction_create_authenticated_request(refresher->transaction,auth_infos,refresher->realm);
		}
		if (requri){
			/*case where we are redirected*/
			belle_sip_request_set_uri(request,requri);
			/*remove auth headers, they are not valid for new destination*/
			belle_sip_message_remove_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_AUTHORIZATION);
			belle_sip_message_remove_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_PROXY_AUTHORIZATION);
		}
	} else  {
		switch (belle_sip_dialog_get_state(dialog)) {
			case BELLE_SIP_DIALOG_CONFIRMED: {
				if (belle_sip_dialog_request_pending(dialog)){
					belle_sip_message("Cannot refresh now, there is a pending request in the dialog.");
					return -1;
				}
				request=belle_sip_dialog_create_request_from(dialog,old_request);
				if (strcmp(belle_sip_request_get_method(request),"SUBSCRIBE")==0) {
					belle_sip_header_content_type_t *content_type;
					/*put expire header*/
					if (!(expires_header = belle_sip_message_get_header_by_type(request,belle_sip_header_expires_t))) {
						expires_header = belle_sip_header_expires_new();
						belle_sip_message_add_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_HEADER(expires_header));
					}
					if ((content_type = belle_sip_message_get_header_by_type(request, belle_sip_header_content_type_t))
						&& strcasecmp("application", belle_sip_header_content_type_get_type(content_type)) == 0
						&& strcasecmp("resource-lists+xml", belle_sip_header_content_type_get_subtype(content_type)) == 0) {
						/*rfc5367
						 3.2.  Subsequent SUBSCRIBE Requests
						 ...
						 At this point, there are no semantics associated with resource-list
						 bodies in subsequent SUBSCRIBE requests (although future extensions
						 can define them).  Therefore, UACs SHOULD NOT include resource-list
						 bodies in subsequent SUBSCRIBE requests to a resource list server.
						 */
						belle_sip_message("Removing body, content type and content length for refresher [%p]",refresher);
						belle_sip_message_set_body(BELLE_SIP_MESSAGE(request), NULL, 0);
						belle_sip_message_remove_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_CONTENT_TYPE);
						belle_sip_message_remove_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_CONTENT_LENGTH);

					}
				}
				belle_sip_provider_add_authorization(prov,request,old_response,NULL,auth_infos,refresher->realm);
				break;
			}
			case BELLE_SIP_DIALOG_TERMINATED: {
				if (refresher->first_acknoleged_request) {
					belle_sip_message("Dialog [%p] is in state terminated, recreating a new one for refresher [%p]",dialog,refresher);
					request = refresher->first_acknoleged_request;
					belle_sip_header_cseq_set_seq_number(belle_sip_message_get_header_by_type(request,belle_sip_header_cseq_t)
														 ,20);
					belle_sip_parameters_remove_parameter(BELLE_SIP_PARAMETERS(belle_sip_message_get_header_by_type(request,belle_sip_header_to_t)),"tag");

					belle_sip_message_set_header((belle_sip_message_t*)request, (belle_sip_header_t*)belle_sip_provider_create_call_id(prov));
					break;
				} /*else nop, error case*/

			}
			default: {
				belle_sip_error("Unexpected dialog state [%s] for dialog [%p], cannot refresh [%s]"
								,belle_sip_dialog_state_to_string(belle_sip_dialog_get_state(dialog))
								,dialog
								,belle_sip_request_get_method(old_request));
				return -1;
			}
		}
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
	
	if (request ==  refresher->first_acknoleged_request) { /*request is now ref by transaction so no need to keepo it*/
		belle_sip_object_unref(refresher->first_acknoleged_request);
		refresher->first_acknoleged_request = NULL;
	}

	switch (belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(refresher->transaction))) {
	case BELLE_SIP_TRANSACTION_INIT:
	case BELLE_SIP_TRANSACTION_CALLING:
	case BELLE_SIP_TRANSACTION_TRYING:
		/*very early state, we can assume nobody will answer, stop retransmiting*/
		belle_sip_transaction_terminate(BELLE_SIP_TRANSACTION(refresher->transaction));
		break;
	default: /*we preserve the transaction "as is"*/
		break;
	}
	/*update reference transaction for next refresh*/
	belle_sip_object_unref(refresher->transaction);
	refresher->transaction=client_transaction;
	belle_sip_object_ref(refresher->transaction);

	if (belle_sip_client_transaction_send_request_to(client_transaction,requri?requri:preset_route)) { /*send imediatly to requri in case of redirect*/
		belle_sip_error("Cannot send refresh method [%s] for refresher [%p]"
				,belle_sip_request_get_method(request)
				,refresher);
		return -1;
	}
	if (expires==0) belle_sip_refresher_stop_internal(refresher,0); /*unregister transaction must be preserved*/
	return 0;
}


static int timer_cb(void *user_data, unsigned int events) {
	belle_sip_refresher_t* refresher = (belle_sip_refresher_t*)user_data;

	if (refresher->timer_purpose==NORMAL_REFRESH && refresher->manual) {
		belle_sip_message("Refresher [%p] is in manual mode, skipping refresh.",refresher);
		/*call listener with special code 0 to indicate request is about to expire*/
		if (refresher->listener) refresher->listener(refresher,refresher->user_data,0, "about to expire", FALSE);
		return BELLE_SIP_STOP;
	}

	if (belle_sip_refresher_refresh(refresher,refresher->target_expires)==-1){
		retry_later(refresher);
	}
	return BELLE_SIP_STOP;
}

belle_sip_header_contact_t* belle_sip_refresher_get_contact(const belle_sip_refresher_t* refresher) {
	belle_sip_transaction_t* transaction = BELLE_SIP_TRANSACTION(refresher->transaction);
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
			belle_sip_object_unref(fixed_local_contact);
			belle_sip_free(tmp_string);
			belle_sip_free(tmp_string2);
			return NULL;
		} else {
			belle_sip_object_unref(fixed_local_contact);
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
			if ((contact_header=belle_sip_refresher_get_contact(refresher))!=NULL){
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
			const char* reason = response ? belle_sip_response_get_reason_phrase(response) : NULL;
			/*check this case because otherwise we are going to loop fast in sending refresh requests.*/
			/*"Test account created" is a special reason given by testers when we create temporary account.
			Since this is a bit of hack, we can ignore logging in that case*/
			if (reason && strcmp(reason, "Test account created") != 0) {
				belle_sip_warning("Server replied with 0 expires, what does that mean?");
			}
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
	belle_sip_refresher_stop_internal(refresher, 1);
}

static void belle_sip_refresher_stop_internal(belle_sip_refresher_t* refresher,int cancel_pending_transaction) {
	belle_sip_message("Refresher [%p] stopped.",refresher);
	if (refresher->timer){
		belle_sip_main_loop_remove_source(belle_sip_stack_get_main_loop(refresher->transaction->base.provider->stack), refresher->timer);
		belle_sip_object_unref(refresher->timer);
		refresher->timer=NULL;
	}
	if (cancel_pending_transaction && refresher->transaction && belle_sip_transaction_state_is_transient(belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(refresher->transaction)))) {
		belle_sip_transaction_terminate(BELLE_SIP_TRANSACTION(refresher->transaction)); /*refresher cancelled, no need to continue to retransmit*/
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

	if (belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(transaction))) {
		set_or_update_dialog(refresher, belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(transaction)));
	}
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

const char* belle_sip_refresher_get_realm(const belle_sip_refresher_t* refresher){
	return refresher->realm;
}

void belle_sip_refresher_set_realm(belle_sip_refresher_t* refresher, const char* realm) {
	if (refresher->realm){
		belle_sip_free(refresher->realm);
		refresher->realm = NULL;
	}
	if (realm!=NULL){
		refresher->realm=belle_sip_strdup(realm);
	}
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

const char * belle_sip_refresher_get_local_address(belle_sip_refresher_t* refresher, int *port){
	belle_sip_channel_t *chan = refresher->transaction->base.channel;
	if (chan) return belle_sip_channel_get_local_address(chan, port);
	return NULL;
}

const char * belle_sip_refresher_get_public_address(belle_sip_refresher_t* refresher, int *port){
	belle_sip_channel_t *chan = refresher->transaction->base.channel;
	if (chan) return belle_sip_channel_get_public_address(chan, port);
	return NULL;
}

static void set_or_update_dialog(belle_sip_refresher_t* refresher, belle_sip_dialog_t* dialog) {
	if (refresher->dialog!=dialog){
		belle_sip_message("refresher [%p] : set_or_update_dialog() current=[%p] new=[%p]",refresher,refresher->dialog,dialog);
		if (refresher->dialog){
			belle_sip_object_unref(refresher->dialog);
		}
		if (dialog) {
			belle_sip_object_ref(dialog);
			/*make sure dialog is internal now*/
			dialog->is_internal = TRUE;

		}
		refresher->dialog=dialog;
	}
}
