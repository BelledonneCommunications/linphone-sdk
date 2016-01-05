/*
    carddav_http_query.c belle-sip - SIP (RFC3261) library.
    Copyright (C) 2015 Belledonne Communications SARL

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

#include <stdio.h>
#include <string.h>

#include "belle-sip/belle-sip.h"
#include "belle-sip/object.h"
#include "belle-sip/types.h"
#include "belle-sip/sipstack.h"
#include "belle-sip/http-listener.h"
#include "belle-sip/http-provider.h"
#include "belle-sip/http-message.h"

typedef struct _CardDavRequest {
	const char *url;
	const char *method;
	const char *body;
	const char *depth;
	const char *digest_auth_username;
	const char *digest_auth_password;
	int request_in_progress;
} CardDavRequest;

static void process_response_from_post_xml_rpc_request(void *data, const belle_http_response_event_t *event) {
	CardDavRequest *request = (CardDavRequest *)data;
	
	if (event->response) {
		int code = belle_http_response_get_status_code(event->response);
		belle_sip_message("HTTP code: %i", code);
		if (code == 207 || code == 200) {
			/*const char *body = belle_sip_message_get_body((belle_sip_message_t *)event->response);
			belle_sip_message("%s", body);*/
			request->request_in_progress = 0;
		}
	}
}

static void process_io_error_from_post_xml_rpc_request(void *data, const belle_sip_io_error_event_t *event) {
	CardDavRequest *request = (CardDavRequest *)data;
	belle_sip_error("I/O Error during request sending");
	request->request_in_progress = 0;
}

static void process_auth_requested_from_post_xml_rpc_request(void *data, belle_sip_auth_event_t *event) {
	CardDavRequest *request = (CardDavRequest *)data;
	
	if (request->digest_auth_username && request->digest_auth_password) {
		belle_sip_auth_event_set_username(event, request->digest_auth_username);
		belle_sip_auth_event_set_passwd(event, request->digest_auth_password);
	} else {
		belle_sip_error("Authentication error during request sending");
		request->request_in_progress = 0;
	}
}

static void prepare_query(CardDavRequest *request) {
	belle_http_request_listener_callbacks_t cbs = { 0 };
	belle_http_request_listener_t *l = NULL;
	belle_generic_uri_t *uri = NULL;
	belle_http_request_t *req = NULL;
	belle_sip_memory_body_handler_t *bh = NULL;
	belle_sip_stack_t *stack = NULL;
	belle_http_provider_t *http_provider = NULL;

	belle_sip_set_log_level(BELLE_SIP_LOG_MESSAGE);
	uri = belle_generic_uri_parse(request->url);
	if (!uri) {
		belle_sip_error("Could not send request, URL %s is invalid", request->url);
		return;
	}
	req = belle_http_request_create(request->method, uri, belle_sip_header_content_type_create("application", "xml; charset=utf-8"), belle_sip_header_create("Depth", request->depth), NULL);
	if (!req) {
		belle_sip_object_unref(uri);
		belle_sip_error("Could not create request");
		return;
	}
	
	bh = belle_sip_memory_body_handler_new_copy_from_buffer(request->body, strlen(request->body), NULL, NULL);
	belle_sip_message_set_body_handler(BELLE_SIP_MESSAGE(req), BELLE_SIP_BODY_HANDLER(bh));
	
	cbs.process_response = process_response_from_post_xml_rpc_request;
	cbs.process_io_error = process_io_error_from_post_xml_rpc_request;
	cbs.process_auth_requested = process_auth_requested_from_post_xml_rpc_request;
	l = belle_http_request_listener_create_from_callbacks(&cbs, request);
	
	stack = belle_sip_stack_new(NULL);
	http_provider = belle_sip_stack_create_http_provider(stack, "0.0.0.0");
	
	request->request_in_progress = 1;
	belle_http_provider_send_request(http_provider, req, l);
	while (request->request_in_progress) {
		belle_sip_stack_sleep(stack, 0);
	}
}

int main(int argc, char *argv[]) {
	CardDavRequest *request = (CardDavRequest *)malloc(sizeof(CardDavRequest));
	if (argc >= 5) {
		request->url = argv[1];
		request->method = argv[2];
		request->body = argv[3];
		request->depth = argv[4];
		if (argc >= 7) {
			request->digest_auth_username = argv[5];
			request->digest_auth_password = argv[6];
		}
	} else {
		belle_sip_error("Usage: carddav_http_query <url> <method> <body> <depth> [username password]");
		return 0;
	}
	
/* Examples:
	"http://192.168.0.230/sabredav/addressbookserver.php/addressbooks/sylvain/default";
	request->method = "PROPFIND";
	request->body = "<d:propfind xmlns:d=\"DAV:\" xmlns:cs=\"http://calendarserver.org/ns/\"><d:prop><d:displayname /><cs:getctag /></d:prop></d:propfind>";
	request->depth = "0";

	request->url = "http://192.168.0.230/sabredav/addressbookserver.php/addressbooks/sylvain/default";
	request->method = "REPORT";
	request->body = "<card:addressbook-query xmlns:d=\"DAV:\" xmlns:card=\"urn:ietf:params:xml:ns:carddav\"><d:prop><d:getetag /><card:address-data content-type='text-vcard' version='4.0'/></d:prop></card:addressbook-query>";
	request->depth = "1";
	
	request->url = "http://192.168.0.230/sabredav/addressbookserver.php/addressbooks/sylvain/default";
	request->method = "REPORT";
	request->body = "<card:addressbook-multiget xmlns:d=\"DAV:\" xmlns:card=\"urn:ietf:params:xml:ns:carddav\"><d:prop><d:getetag /><card:address-data content-type='text-vcard' version='4.0'/></d:prop><d:href>/sabredav/addressbookserver.php/addressbooks/sylvain/default/me.vcf</d:href></card:addressbook-multiget>"
	request->depth = "1";
*/
	
	prepare_query(request);
}