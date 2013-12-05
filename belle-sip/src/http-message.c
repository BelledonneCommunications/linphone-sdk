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

#include "belle_sip_internal.h"


struct belle_http_request{
	belle_sip_message_t base;
	belle_http_url_t *req_url;
	belle_http_request_listener_t *listener;
};

static void belle_http_request_listener_destroyed(belle_http_request_t *req){
	req->listener=NULL;
}

static void belle_http_request_destroy(belle_http_request_t *req){
	if (req->req_url) belle_sip_object_unref(req->req_url);
	belle_http_request_set_listener(req,NULL);
}

static void belle_http_request_clone(belle_http_request_t *obj, const belle_http_request_t *orig){
	if (orig->req_url) obj->req_url=(belle_http_url_t*)belle_sip_object_clone((belle_sip_object_t*)orig->req_url);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_http_request_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_http_request_t,belle_sip_message_t,belle_http_request_destroy,belle_http_request_clone,NULL,TRUE);

belle_http_request_t *belle_http_request_new(){
	belle_http_request_t *obj=belle_sip_object_new(belle_http_request_t);
	belle_sip_message_init((belle_sip_message_t*)obj);
	return obj;
}

belle_http_request_t *belle_http_request_create(belle_http_url_t *url){
	belle_http_request_t *obj=belle_http_request_new();
	obj->req_url=(belle_http_url_t*)belle_sip_object_ref(url);
	return obj;
}

void belle_http_request_set_listener(belle_http_request_t *req, belle_http_request_listener_t *l){
	if (req->listener){
		 belle_sip_object_weak_unref(req->listener,(belle_sip_object_destroy_notify_t)belle_http_request_listener_destroyed,req);
		 req->listener=NULL;
	}
	if (l)
		belle_sip_object_weak_ref(l,(belle_sip_object_destroy_notify_t)belle_http_request_listener_destroyed,req);
}

belle_http_url_t *belle_http_request_get_url(belle_http_request_t *req){
	return req->req_url;
}
