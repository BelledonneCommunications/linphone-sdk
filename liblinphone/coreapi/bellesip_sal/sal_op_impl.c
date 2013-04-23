/*
linphone
Copyright (C) 2012  Belledonne Communications, Grenoble, France

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
#include "sal_impl.h"


/*create an operation */
SalOp * sal_op_new(Sal *sal){
	SalOp *op=ms_new0(SalOp,1);
	__sal_op_init(op,sal);
	op->type=SalOpUnknown;
	sal_op_ref(op);
	return op;
}
void sal_op_release(SalOp *op){
	op->state=SalOpStateTerminated;
	sal_op_set_user_pointer(op,NULL);/*mandatory because releasing op doesn not mean freeing op. Make sure back pointer will not be used later*/
	if (op->refresher) belle_sip_refresher_stop(op->refresher);
	sal_op_unref(op);
}
void sal_op_release_impl(SalOp *op){
	ms_message("Destroying op [%p] of type [%s]",op,sal_op_type_to_string(op->type));
	if (op->pending_auth_transaction) belle_sip_object_unref(op->pending_auth_transaction);
	if (op->auth_info) sal_auth_info_delete(op->auth_info);
	if (op->sdp_answer) belle_sip_object_unref(op->sdp_answer);
	if (op->refresher) {
		belle_sip_object_unref(op->refresher);
		op->refresher=NULL;
	}
	if(op->replaces) belle_sip_object_unref(op->replaces);
	if(op->referred_by) belle_sip_object_unref(op->referred_by);

	if (op->pending_client_trans) belle_sip_object_unref(op->pending_client_trans);
	if (op->pending_server_trans) belle_sip_object_unref(op->pending_server_trans);
	__sal_op_free(op);
	return ;
}

void sal_op_authenticate(SalOp *op, const SalAuthInfo *info){
	if (op->type == SalOpRegister) {
		/*Registration authenticate is just about registering again*/
		sal_register_refresh(op,-1);
	}else {
		/*for sure auth info will be accesible from the provider*/
		sal_process_authentication(op);
	}
	return ;
}

void sal_op_cancel_authentication(SalOp *h){
	ms_fatal("sal_op_cancel_authentication not implemented yet");
	return ;
}

int sal_op_get_auth_requested(SalOp *op, const char **realm, const char **username){
	*realm=op->auth_info?op->auth_info->realm:NULL;
	*username=op->auth_info?op->auth_info->username:NULL;
	return 0;
}
belle_sip_header_contact_t* sal_op_create_contact(SalOp *op,belle_sip_header_from_t* from_header) {
	belle_sip_uri_t* req_uri = (belle_sip_uri_t*)belle_sip_object_clone((belle_sip_object_t*)belle_sip_header_address_get_uri((belle_sip_header_address_t*)from_header));
	belle_sip_header_contact_t* contact_header;
	if (sal_op_get_contact_address(op)) {
		contact_header = belle_sip_header_contact_create(BELLE_SIP_HEADER_ADDRESS(sal_op_get_contact_address(op)));
	} else {
		contact_header= belle_sip_header_contact_new();
		belle_sip_header_address_set_uri((belle_sip_header_address_t*)contact_header,belle_sip_uri_new());
		belle_sip_uri_set_user(belle_sip_header_address_get_uri((belle_sip_header_address_t*)contact_header),belle_sip_uri_get_user(req_uri));
	}
	belle_sip_object_unref(req_uri);
	return contact_header;
}

belle_sip_request_t* sal_op_build_request(SalOp *op,const char* method) {
	belle_sip_header_from_t* from_header;
	belle_sip_header_to_t* to_header;
	belle_sip_provider_t* prov=op->base.root->prov;
	belle_sip_request_t *req;
	belle_sip_uri_t* req_uri;
	char token[10];

	from_header = belle_sip_header_from_create(BELLE_SIP_HEADER_ADDRESS(sal_op_get_from_address(op))
												,belle_sip_random_token(token,sizeof(token)));
	to_header = belle_sip_header_to_create(BELLE_SIP_HEADER_ADDRESS(sal_op_get_to_address(op)),NULL);
	req_uri = (belle_sip_uri_t*)belle_sip_object_clone((belle_sip_object_t*)belle_sip_header_address_get_uri((belle_sip_header_address_t*)to_header));

	req=belle_sip_request_create(
							req_uri,
							method,
		                    belle_sip_provider_create_call_id(prov),
		                    belle_sip_header_cseq_create(20,method),
		                    from_header,
		                    to_header,
		                    belle_sip_header_via_new(),
		                    70);

	return req;
}



/*ping: main purpose is to obtain its own contact address behind firewalls*/
int sal_ping(SalOp *op, const char *from, const char *to){
	sal_op_set_from(op,from);
	sal_op_set_to(op,to);
	return sal_op_send_request(op,sal_op_build_request(op,"OPTION"));
}

void sal_op_set_remote_ua(SalOp*op,belle_sip_message_t* message) {
	belle_sip_header_user_agent_t* user_agent=belle_sip_message_get_header_by_type(message,belle_sip_header_user_agent_t);
	char user_agent_string[256];
	if(user_agent && belle_sip_header_user_agent_get_products_as_string(user_agent,user_agent_string,sizeof(user_agent_string))>0) {
		op->base.remote_ua=ms_strdup(user_agent_string);
	}
}


int sal_op_send_request_with_expires(SalOp* op, belle_sip_request_t* request,int expires) {
	belle_sip_header_expires_t* expires_header=(belle_sip_header_expires_t*)belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_EXPIRES);

	if (!expires_header && expires>=0) {
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_HEADER(expires_header=belle_sip_header_expires_new()));
	}
	if (expires_header) belle_sip_header_expires_set_expires(expires_header,expires);
	return sal_op_send_request(op,request);
}

void sal_op_resend_request(SalOp* op, belle_sip_request_t* request) {
	belle_sip_header_cseq_t* cseq=(belle_sip_header_cseq_t*)belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_CSEQ);
	belle_sip_header_cseq_set_seq_number(cseq,belle_sip_header_cseq_get_seq_number(cseq)+1);
	sal_op_send_request(op,request);
}


static int _sal_op_send_request_with_contact(SalOp* op, belle_sip_request_t* request,bool_t add_contact) {
	belle_sip_client_transaction_t* client_transaction;
	belle_sip_provider_t* prov=op->base.root->prov;
	belle_sip_uri_t* outbound_proxy=NULL;
	belle_sip_header_contact_t* contact;
	
	if (!op->dialog || belle_sip_dialog_get_state(op->dialog) == BELLE_SIP_DIALOG_NULL) {
		/*don't put route header if  dialog is in confirmed state*/
		const MSList *elem=sal_op_get_route_addresses(op);
		belle_sip_uri_t *next_hop_uri;
		const char *transport;
		if (elem) {
			outbound_proxy=belle_sip_header_address_get_uri((belle_sip_header_address_t*)elem->data);
			next_hop_uri=outbound_proxy;
		}else{
			next_hop_uri=belle_sip_request_get_uri(request);
		}
		transport=belle_sip_uri_get_transport_param(next_hop_uri);
		if (transport==NULL){
			/*compatibility mode: by default it should be udp as not explicitely set and if no udp listening point is available, then use
			 * the first available transport*/
			if (belle_sip_provider_get_listening_point(prov,"UDP")==0){
				if (belle_sip_provider_get_listening_point(prov,"TCP")!=NULL){
					transport="tcp";
				}else if (belle_sip_provider_get_listening_point(prov,"TLS")!=NULL){
					transport="tls";
				}
			}
			if (transport){
				belle_sip_message("Transport is not specified, using %s because UDP is not available.",transport);
				belle_sip_uri_set_transport_param(next_hop_uri,transport);
			}
			belle_sip_uri_fix(next_hop_uri);
		}
	}

	client_transaction = belle_sip_provider_create_client_transaction(prov,request);
	belle_sip_transaction_set_application_data(BELLE_SIP_TRANSACTION(client_transaction),sal_op_ref(op));
	if (op->pending_client_trans) belle_sip_object_unref(op->pending_client_trans);
	op->pending_client_trans=client_transaction; /*update pending inv for being able to cancel*/
	belle_sip_object_ref(op->pending_client_trans);

	if (belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(request),belle_sip_header_user_agent_t)==NULL)
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_HEADER(op->base.root->user_agent));

	if (add_contact) {
		contact = sal_op_create_contact(op,belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(request),belle_sip_header_from_t));
		belle_sip_message_remove_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_CONTACT);
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_HEADER(contact));
	}
	if (!belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_AUTHORIZATION)
		&& !belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_PROXY_AUTHORIZATION)) {
		/*hmm just in case we already have authentication param in cache*/
		belle_sip_provider_add_authorization(op->base.root->prov,request,NULL,NULL);
	}
	return belle_sip_client_transaction_send_request_to(client_transaction,outbound_proxy/*might be null*/);

}

int sal_op_send_request(SalOp* op, belle_sip_request_t* request)  {
	bool_t need_contact=FALSE;
	/*
  	  Header field          where   proxy ACK BYE CAN INV OPT REG
      ___________________________________________________________
      Contact                 R            o   -   -   m   o   o
	 */
	if (strcmp(belle_sip_request_get_method(request),"INVITE")==0
			||strcmp(belle_sip_request_get_method(request),"REGISTER")==0
			||strcmp(belle_sip_request_get_method(request),"SUBSCRIBE")==0
			||strcmp(belle_sip_request_get_method(request),"OPTION")==0)
		need_contact=TRUE;

	return _sal_op_send_request_with_contact(op, request,need_contact);
}


void sal_compute_sal_errors_from_code(int code ,SalError* sal_err,SalReason* sal_reason) {
		switch(code) {
		case 400:
			*sal_err=SalErrorUnknown;
			break;
		case 404:
			*sal_err=SalErrorFailure;
			*sal_reason=SalReasonNotFound;
			break;
		case 415:
			*sal_err=SalErrorFailure;
			*sal_reason=SalReasonMedia;
			break;
		case 422:
			ms_error ("422 not implemented yet");;
			break;
		case 480:
			*sal_err=SalErrorFailure;
			*sal_reason=SalReasonTemporarilyUnavailable;
			break;
		case 486:
			*sal_err=SalErrorFailure;
			*sal_reason=SalReasonBusy;
			break;
		case 487:
			break;
		case 600:
			*sal_err=SalErrorFailure;
			*sal_reason=SalReasonDoNotDisturb;
			break;
		case 603:
			*sal_err=SalErrorFailure;
			*sal_reason=SalReasonDeclined;
			break;
		default:
			if (code>0){
				*sal_err=SalErrorFailure;
				*sal_reason=SalReasonUnknown;
			}else *sal_err=SalErrorNoResponse;
			/* no break */
		}
}
/*return TRUE if error code*/
bool_t sal_compute_sal_errors(belle_sip_response_t* response,SalError* sal_err,SalReason* sal_reason,char* reason, size_t reason_size) {
	int code = belle_sip_response_get_status_code(response);
	belle_sip_header_t* reason_header = belle_sip_message_get_header(BELLE_SIP_MESSAGE(response),"Reason");
	*sal_err=SalErrorUnknown;
	*sal_reason = SalReasonUnknown;

	if (reason_header){
		snprintf(reason
				,reason_size
				,"%s %s"
				,belle_sip_response_get_reason_phrase(response)
				,belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(reason_header)));
	} else {
		strncpy(reason,belle_sip_response_get_reason_phrase(response),reason_size);
	}
	if (code>400) {
		sal_compute_sal_errors_from_code(code,sal_err,sal_reason);
		return TRUE;
	} else {
		return FALSE;
	}
}
void set_or_update_dialog(SalOp* op, belle_sip_dialog_t* dialog) {
	/*check if dialog has changed*/
	if (dialog && dialog != op->dialog) {
		ms_message("Dialog set from [%p] to [%p] for op [%p]",op->dialog,dialog,op);
		/*fixme, shouldn't we cancel previous dialog*/
		if (op->dialog) {
			belle_sip_dialog_set_application_data(op->dialog,NULL);
			belle_sip_object_unref(op->dialog);
			sal_op_unref(op);
		}
		op->dialog=dialog;
		belle_sip_dialog_set_application_data(op->dialog,op);
		sal_op_ref(op);
		belle_sip_object_ref(op->dialog);
	}
}
/*return reffed op*/
SalOp* sal_op_ref(SalOp* op) {
	op->ref++;
	return op;
}
/*return null, destroy op if ref count =0*/
void* sal_op_unref(SalOp* op) {
	if (--op->ref <=0) {
		sal_op_release_impl(op);
	}
	return NULL;
}
int sal_op_send_and_create_refresher(SalOp* op,belle_sip_request_t* req, int expires,belle_sip_refresher_listener_t listener ) {
	if (sal_op_send_request_with_expires(op,req,expires)) {
		return -1;
	} else {
		if (op->refresher) {
			belle_sip_refresher_stop(op->refresher);
			belle_sip_object_unref(op->refresher);
		}
		if ((op->refresher = belle_sip_client_transaction_create_refresher(op->pending_client_trans))) {
			belle_sip_refresher_enable_nat_helper(op->refresher,op->base.root->nat_helper_enabled);
			belle_sip_refresher_set_listener(op->refresher,listener,op);
			return 0;
		} else {
			return -1;
		}
	}
}

const char* sal_op_state_to_string(const SalOpSate_t value) {
	switch(value) {
	case SalOpStateEarly: return"SalOpStateEarly";
	case SalOpStateActive: return "SalOpStateActive";
	case SalOpStateTerminating: return "SalOpStateTerminating";
	case SalOpStateTerminated: return "SalOpStateTerminated";
	default:
		return "Unknon";
	}
}
