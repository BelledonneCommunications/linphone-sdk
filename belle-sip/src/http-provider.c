/*
 * Copyright (c) 2012-2019 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "bctoolbox/defs.h"

#include "belle_sip_internal.h"
#include "channel_bank.hh"

typedef struct belle_http_channel_context belle_http_channel_context_t;

#define BELLE_HTTP_CHANNEL_CONTEXT(obj) BELLE_SIP_CAST(obj, belle_http_channel_context_t)

static void provider_remove_channel(belle_http_provider_t *obj, belle_sip_channel_t *chan);

struct belle_http_channel_context {
	belle_sip_object_t base;
	belle_http_provider_t *provider;
	belle_sip_list_t *pending_requests;
};

struct belle_http_provider {
	belle_sip_object_t base;
	belle_sip_stack_t *stack;
	char *bind_ip;
	int ai_family;
	belle_sip_channel_bank_t *tcp_channels;
	belle_sip_channel_bank_t *tls_channels;
	belle_tls_crypto_config_t *crypto_config;
	int simulated_recv_return;
	uint8_t transports; /**< a mask of enabled transports, availables: BELLE_SIP_HTTP_TRANSPORT_TCP and
	                       BELLE_SIP_HTTP_TRANSPORT_TLS */
};

#define BELLE_HTTP_REQUEST_INVOKE_LISTENER(obj, method, arg)                                                           \
	obj->listener ? BELLE_SIP_INVOKE_LISTENER_ARG(obj->listener, belle_http_request_listener_t, method, arg) : 0

static void uri_copy_parts(belle_generic_uri_t *uri, const belle_generic_uri_t *original_uri) {
	const char *username = belle_generic_uri_get_user(original_uri);
	const char *passwd = belle_generic_uri_get_user_password(original_uri);
	if (username) belle_generic_uri_set_user(uri, username);
	if (passwd) belle_generic_uri_set_user_password(uri, passwd);
	/* are there other uri parameters that shall be copied ? */
}

static int http_channel_context_handle_redirect(belle_http_channel_context_t *ctx, belle_http_request_t *req) {
	const char *method = belle_http_request_get_method(req);
	belle_http_response_t *resp = belle_http_request_get_response(req);
	belle_sip_header_t *location;
	belle_generic_uri_t *new_uri;
	const char *location_value;

	if (req->redirect_count > 70) {
		belle_sip_error("Too many redirection of this request, giveup.");
		return -1;
	}

	if (!(strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0)) {
		/* if not GET or HEAD, the redirect shall not be done automatically without user interaction */
		return -1;
	}
	location = belle_sip_message_get_header(BELLE_SIP_MESSAGE(resp), "Location");
	if (!location || (location_value = belle_sip_header_get_unparsed_value(location)) == NULL) {
		belle_sip_error("HTTP Redirect without location header.");
		return -1;
	}
	new_uri = belle_generic_uri_parse(location_value);
	if (new_uri == NULL) {
		belle_sip_error("Cannot parse location header's uri [%s]", location_value);
		return -1;
	}
	req->redirect_count++;
	if (req->orig_uri) uri_copy_parts(new_uri, req->orig_uri);
	belle_http_request_set_uri(req, new_uri);
	/* reset the original URI, as it is being used for routing*/
	SET_OBJECT_PROPERTY(req, orig_uri, NULL);
	/*remove auth headers, they are not valid for new destination*/
	belle_sip_message_remove_header(BELLE_SIP_MESSAGE(req), BELLE_HTTP_AUTHORIZATION);

	belle_sip_message("belle_http_request_t[%p]: handling HTTP redirection to [%s]", req, location_value);
	belle_http_provider_send_request(ctx->provider, req, NULL);
	return 0;
}

static int http_channel_context_handle_authentication(belle_http_channel_context_t *ctx, belle_http_request_t *req) {
	const char *realm = NULL;
	belle_sip_auth_event_t *ev = NULL;
	belle_http_response_t *resp = belle_http_request_get_response(req);
	const char *username = NULL;
	const char *passwd = NULL;
	const char *access_token = NULL;
	const char *ha1 = NULL;
	const char *algorithm = NULL;
	belle_sip_auth_mode_t auth_mode = BELLE_SIP_AUTH_MODE_HTTP_DIGEST;
	char computed_ha1[65];
	belle_sip_header_www_authenticate_t *authenticate;
	belle_sip_list_t *authenticate_lst;
	belle_sip_list_t *it;

	int ret = 0;

	if (req->auth_attempt_count > 1) {
		req->auth_attempt_count = 0;
		return -1;
	}
	if (resp == NULL) {
		belle_sip_error("Missing response for  req [%p], cannot authenticate", req);
		return -1;
	}

	/* check we have at least one authenticate header and discard any proxy authentification */
	if (!(authenticate = belle_sip_message_get_header_by_type(resp, belle_sip_header_www_authenticate_t))) {
		if (belle_sip_message_get_header_by_type(resp, belle_sip_header_proxy_authenticate_t)) {
			belle_sip_error("Proxy authentication not supported yet, cannot authenticate for resp [%p]", resp);
		}
		belle_sip_error("Missing auth header in response  [%p], cannot authenticate", resp);
		return -1;
	}

	/*find if username, passwd or acces token were already supplied in original request uri*/
	if (req->orig_uri) {
		username = belle_generic_uri_get_user(req->orig_uri);
		passwd = belle_generic_uri_get_user_password(req->orig_uri);
		access_token = belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(req->orig_uri), "access_token");
	}

	// add from_uri to process_auth_requested event data
	belle_sip_uri_t *from_uri = NULL;
	belle_sip_header_t *header = belle_sip_message_get_header(BELLE_SIP_MESSAGE(req), "From");

	/* loop on all authenticate headers.  03/24 fixme probably not working for more than one auth header, still to be
	 * tested*/
	authenticate_lst =
	    belle_sip_list_copy(belle_sip_message_get_headers(BELLE_SIP_MESSAGE(resp), BELLE_SIP_WWW_AUTHENTICATE));
	for (it = authenticate_lst; it != NULL; it = it->next) {
		authenticate = BELLE_SIP_HEADER_WWW_AUTHENTICATE(it->data);
		do {
			const char *requested_algorithm = belle_sip_header_www_authenticate_get_algorithm(authenticate);

			realm = belle_sip_header_www_authenticate_get_realm(authenticate);

			belle_sip_message("Processing authenticate header");

			if (strcasecmp("Digest", belle_sip_header_www_authenticate_get_scheme(authenticate)) == 0) {
				if (requested_algorithm == NULL) { // default algorithm is MD5
					requested_algorithm = "MD5";
				}
				auth_mode = BELLE_SIP_AUTH_MODE_HTTP_DIGEST;
				if (belle_sip_stack_check_digest_compatibility(ctx->provider->stack, authenticate) == -1) continue;

			} else if (strcasecmp("Basic", belle_sip_header_www_authenticate_get_scheme(authenticate)) == 0) {
				auth_mode = BELLE_SIP_AUTH_MODE_HTTP_BASIC;
				/* ok, Basic is supported*/
			} else if (strcasecmp("Bearer", belle_sip_header_www_authenticate_get_scheme(authenticate)) == 0) {
				auth_mode = BELLE_SIP_AUTH_MODE_HTTP_BEARER;
				/* ok, Bearer is supported*/
			} else {
				belle_sip_error("Unsupported auth scheme [%s] in response  [%p], cannot authenticate",
				                belle_sip_header_www_authenticate_get_scheme(authenticate), resp);
				belle_sip_list_free(authenticate_lst);
				return -1;
			}

			if (header) {
				belle_sip_header_address_t *from_address =
				    belle_sip_header_address_parse(belle_sip_header_get_unparsed_value(header));
				from_uri = belle_sip_header_address_get_uri(from_address);
			} else if (username && !passwd) {
				from_uri = belle_sip_uri_create(username, realm);
			}

			if ((!username || !passwd) && !access_token) {
				ev = belle_sip_auth_event_create((belle_sip_object_t *)ctx->provider, realm, from_uri);
				belle_sip_auth_event_set_algorithm(ev, requested_algorithm);
				ev->mode = auth_mode;
				if (auth_mode == BELLE_SIP_AUTH_MODE_HTTP_BEARER) {
					belle_sip_auth_event_set_authz_server(
					    ev, belle_sip_header_www_authenticate_get_authz_server(authenticate));
				}
				belle_sip_message("Invoking process_auth_requested");
				BELLE_HTTP_REQUEST_INVOKE_LISTENER(req, process_auth_requested, ev);
				username = ev->userid ? ev->userid : ev->username;
				passwd = ev->passwd;
				ha1 = ev->ha1;
				access_token = ev->bearer_token ? belle_sip_bearer_token_get_token(ev->bearer_token) : NULL;
				algorithm = ev->algorithm;
			}
			if (auth_mode == BELLE_SIP_AUTH_MODE_HTTP_DIGEST) {
				if (!ha1) {
					if (username && passwd) {
						belle_sip_auth_helper_compute_ha1_for_algorithm(username, realm, passwd, computed_ha1,
						                                                belle_sip_auth_define_size(requested_algorithm),
						                                                requested_algorithm);
						ha1 = computed_ha1;
						algorithm = requested_algorithm;
					}
				}
				if (ha1) break;
			}
			authenticate = BELLE_SIP_HEADER_WWW_AUTHENTICATE(belle_sip_header_get_next(BELLE_SIP_HEADER(authenticate)));
		} while (authenticate);
	}
	belle_sip_list_free(authenticate_lst);

	if (auth_mode == BELLE_SIP_AUTH_MODE_HTTP_DIGEST && ha1) {
		belle_http_header_authorization_t *authorization;
		req->auth_attempt_count++;

		authorization = belle_http_auth_helper_create_authorization(authenticate);
		/*select first qop !mode qop = auth not supported yet for http, but lime serveur auth require nc!*/
		belle_sip_header_authorization_set_qop(BELLE_SIP_HEADER_AUTHORIZATION(authorization),
		                                       belle_sip_header_www_authenticate_get_qop_first(authenticate));
		belle_sip_header_authorization_set_nonce_count(BELLE_SIP_HEADER_AUTHORIZATION(authorization),
		                                               1); /*we don't store nonce count for now*/
		belle_sip_header_authorization_set_username(BELLE_SIP_HEADER_AUTHORIZATION(authorization), username);
		belle_http_header_authorization_set_uri(authorization, belle_http_request_get_uri(req));
		belle_sip_header_authorization_set_algorithm(BELLE_SIP_HEADER_AUTHORIZATION(authorization), algorithm);
		if (belle_sip_auth_helper_fill_authorization(BELLE_SIP_HEADER_AUTHORIZATION(authorization),
		                                             belle_http_request_get_method(req), ha1)) {
			belle_sip_error("Cannot fill auth header for request [%p]", req);
			if (authorization) belle_sip_object_unref(authorization);
			ret = -1;
		} else {
			belle_sip_message_remove_header(BELLE_SIP_MESSAGE(req), BELLE_HTTP_AUTHORIZATION);
			belle_sip_message_add_header(BELLE_SIP_MESSAGE(req), BELLE_SIP_HEADER(authorization));
			belle_http_provider_send_request(ctx->provider, req, NULL);
		}

	} else if (auth_mode == BELLE_SIP_AUTH_MODE_HTTP_BASIC && username && passwd) {
		belle_http_header_authorization_t *authorization;
		req->auth_attempt_count++;

		authorization = belle_http_header_authorization_new();
		belle_sip_header_authorization_set_scheme(BELLE_SIP_HEADER_AUTHORIZATION(authorization), "Basic");
		char *username_passwd = belle_sip_strdup_printf("%s:%s", username, passwd);
		size_t username_passwd_length = strlen(username_passwd);
		size_t encoded_username_paswd_length = username_passwd_length * 2;
		unsigned char *encoded_username_paswd = belle_sip_malloc(2 * username_passwd_length);
		bctbx_base64_encode(encoded_username_paswd, &encoded_username_paswd_length,
		                    (const unsigned char *)username_passwd, username_passwd_length);
		belle_sip_parameters_set(BELLE_SIP_PARAMETERS(authorization), (const char *)encoded_username_paswd);
		belle_sip_free(username_passwd);
		belle_sip_free(encoded_username_paswd);

		belle_sip_message_remove_header(BELLE_SIP_MESSAGE(req), BELLE_HTTP_AUTHORIZATION);
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req), BELLE_SIP_HEADER(authorization));
		belle_http_provider_send_request(ctx->provider, req, NULL);
	} else if (auth_mode == BELLE_SIP_AUTH_MODE_HTTP_BEARER && access_token) {
		belle_http_header_authorization_t *authorization;
		req->auth_attempt_count++;

		authorization = belle_http_header_authorization_new();
		belle_sip_header_authorization_set_scheme(BELLE_SIP_HEADER_AUTHORIZATION(authorization), "Bearer");

		belle_sip_parameters_set(BELLE_SIP_PARAMETERS(authorization), access_token);
		belle_sip_message_remove_header(BELLE_SIP_MESSAGE(req), BELLE_HTTP_AUTHORIZATION);
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req), BELLE_SIP_HEADER(authorization));
		belle_http_provider_send_request(ctx->provider, req, NULL);
	} else {
		belle_sip_error("No auth info found for request [%p], cannot authenticate", req);
		ret = -1;
	}
	if (ev) belle_sip_auth_event_destroy(ev);
	return ret;
}

static void http_channel_context_handle_response_headers(belle_http_channel_context_t *ctx,
                                                         belle_sip_channel_t *chan,
                                                         belle_http_response_t *response) {
	belle_http_request_t *req = ctx->pending_requests ? (belle_http_request_t *)ctx->pending_requests->data : NULL;
	belle_http_response_event_t ev = {0};
	int code;

	if (req == NULL) {
		belle_sip_error("Receiving http response headers not matching any request.");
		return;
	}
	if (belle_http_request_is_cancelled(req)) {
		belle_sip_warning("Receiving http response headers for a cancelled request.");
		return;
	}
	code = belle_http_response_get_status_code(response);
	if (code != 401 && code != 407 && code != 301 && code != 302 && code != 307) {
		/*else notify the app about the response headers received*/
		ev.source = (belle_sip_object_t *)ctx->provider;
		ev.request = req;
		ev.response = response;
		BELLE_HTTP_REQUEST_INVOKE_LISTENER(req, process_response_headers, &ev);
	}
}

static void release_background_task(belle_http_request_t *req) {
	if (req->background_task_id) {
		belle_sip_message("HTTP request finished: ending bg task id=[%x]", req->background_task_id);
		belle_sip_end_background_task(req->background_task_id);
		req->background_task_id = 0;
	}
}

static void http_channel_context_handle_response(belle_http_channel_context_t *ctx,
                                                 belle_sip_channel_t *chan,
                                                 belle_http_response_t *response) {
	belle_http_request_t *req = NULL;
	belle_http_response_event_t ev = {0};
	int code;
	belle_sip_header_t *connection;
	/*pop the request matching this response*/
	ctx->pending_requests = belle_sip_list_pop_front(ctx->pending_requests, (void **)&req);
	if (req == NULL) {
		belle_sip_error("Receiving http response not matching any request.");
		return;
	}
	if (belle_http_request_is_cancelled(req)) {
		belle_sip_warning("Receiving http response for a cancelled request.");
		return;
	}
	connection = belle_sip_message_get_header((belle_sip_message_t *)response, "Connection");
	if (connection && strstr(belle_sip_header_get_unparsed_value(connection), "close") != NULL)
		chan->about_to_be_closed = TRUE;

	belle_http_request_set_response(req, response);
	code = belle_http_response_get_status_code(response);
	if ((code == 401 || code == 407) && http_channel_context_handle_authentication(ctx, req) == 0) {
		/*nothing to do, the request has been resubmitted with authentication*/
	} else if ((code == 301 || code == 302 || code == 307) && http_channel_context_handle_redirect(ctx, req) == 0) {
		/* nothing to do, the request is automatically re-submitted to the new location */
	} else {
		/*else notify the app about the response received*/
		ev.source = (belle_sip_object_t *)ctx->provider;
		ev.request = req;
		ev.response = response;
		BELLE_HTTP_REQUEST_INVOKE_LISTENER(req, process_response, &ev);
		release_background_task(req);
	}
	belle_sip_object_unref(req);
}

static void
http_channel_notify_io_error(belle_http_channel_context_t *ctx, belle_sip_channel_t *chan, belle_http_request_t *req) {
	belle_sip_io_error_event_t ev = {0};
	/*TODO: would be nice to put the message in the event*/
	ev.source = (belle_sip_object_t *)ctx->provider;
	ev.host = chan->peer_cname;
	ev.port = chan->peer_port;
	ev.transport = belle_sip_channel_get_transport_name(chan);
	BELLE_HTTP_REQUEST_INVOKE_LISTENER(req, process_io_error, &ev);
	release_background_task(req);
}

static void http_channel_context_handle_io_error(belle_http_channel_context_t *ctx, belle_sip_channel_t *chan) {
	belle_http_request_t *req = NULL;
	belle_sip_list_t *elem;

	/*if the error happens before attempting to send the message, the pending_requests is empty*/
	if (ctx->pending_requests == NULL) elem = chan->outgoing_messages;
	else elem = ctx->pending_requests;
	/*pop the requests for which this error is reported*/
	for (; elem != NULL; elem = elem->next) {
		req = (belle_http_request_t *)elem->data;
		http_channel_notify_io_error(ctx, chan, req);
	}
}

static void http_channel_context_handle_disconnection(belle_http_channel_context_t *ctx, belle_sip_channel_t *chan) {
	belle_sip_list_t *elem;
	belle_http_request_t *req = NULL;
	belle_sip_list_t *to_be_resubmitted = NULL;

	for (elem = chan->outgoing_messages; elem != NULL; elem = elem->next) {
		to_be_resubmitted = bctbx_list_append(to_be_resubmitted, elem->data);
	}
	for (elem = ctx->pending_requests; elem != NULL; elem = elem->next) {
		if (bctbx_list_find(to_be_resubmitted, elem->data) == NULL) {
			to_be_resubmitted = bctbx_list_append(to_be_resubmitted, elem->data);
		}
	}
	for (elem = to_be_resubmitted; elem != NULL; elem = elem->next) {
		req = (belle_http_request_t *)elem->data;
		if (req->resubmitted == 0) {
			req->resubmitted = 1;
			belle_sip_message("Resubmitting http request.");
			belle_http_provider_send_request(ctx->provider, req, NULL /*keep the listener as it is already*/);
		} else {
			belle_sip_warning("Http request has already been resubmitted after a server disconnection. Treating this "
			                  "as an error now.");
			http_channel_notify_io_error(ctx, chan, req);
		}
	}
	bctbx_list_free(to_be_resubmitted);
}

/* we are called here by the channel when receiving a message for which a body is expected.
 * We can notify the application so that it can setup an appropriate body handler.
 */
static void
channel_on_message_headers(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_message_t *msg) {
	belle_http_channel_context_t *ctx = BELLE_HTTP_CHANNEL_CONTEXT(obj);

	if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(msg, belle_http_response_t)) {
		http_channel_context_handle_response_headers(ctx, chan, (belle_http_response_t *)msg);
	} /*ignore requests*/
}

static void channel_on_message(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_message_t *msg) {
	belle_http_channel_context_t *ctx = BELLE_HTTP_CHANNEL_CONTEXT(obj);

	if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(msg, belle_http_response_t)) {
		http_channel_context_handle_response(ctx, chan, (belle_http_response_t *)msg);
	} /*ignore requests*/
}

static int channel_on_auth_requested(belle_sip_channel_listener_t *obj,
                                     belle_sip_channel_t *chan,
                                     const char *distinguished_name) {
	belle_http_channel_context_t *ctx = BELLE_HTTP_CHANNEL_CONTEXT(obj);
	if (BELLE_SIP_IS_INSTANCE_OF(chan, belle_sip_tls_channel_t)) {
		belle_sip_auth_event_t *auth_event =
		    belle_sip_auth_event_create((belle_sip_object_t *)ctx->provider, NULL, NULL);
		belle_sip_tls_channel_t *tls_chan = BELLE_SIP_TLS_CHANNEL(chan);
		belle_http_request_t *req = (belle_http_request_t *)chan->outgoing_messages->data;
		auth_event->mode = BELLE_SIP_AUTH_MODE_TLS;
		belle_sip_auth_event_set_distinguished_name(auth_event, distinguished_name);

		BELLE_HTTP_REQUEST_INVOKE_LISTENER(req, process_auth_requested, auth_event);
		belle_sip_tls_channel_set_client_certificates_chain(tls_chan, auth_event->cert);
		belle_sip_tls_channel_set_client_certificate_key(tls_chan, auth_event->key);
		belle_sip_auth_event_destroy(auth_event);
	}
	return 0;
}

static void channel_on_sending(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_message_t *msg) {
	belle_http_channel_context_t *ctx = BELLE_HTTP_CHANNEL_CONTEXT(obj);
	ctx->pending_requests = belle_sip_list_append(ctx->pending_requests, belle_sip_object_ref(msg));
}

static void
channel_state_changed(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_channel_state_t state) {
	belle_http_channel_context_t *ctx = BELLE_HTTP_CHANNEL_CONTEXT(obj);
	switch (state) {
		case BELLE_SIP_CHANNEL_INIT:
		case BELLE_SIP_CHANNEL_RES_IN_PROGRESS:
		case BELLE_SIP_CHANNEL_RES_DONE:
		case BELLE_SIP_CHANNEL_CONNECTING:
		case BELLE_SIP_CHANNEL_READY:
		case BELLE_SIP_CHANNEL_RETRY:
			break;
		case BELLE_SIP_CHANNEL_ERROR:
			http_channel_context_handle_io_error(ctx, chan);
			if (!chan->force_close) {
				provider_remove_channel(ctx->provider, chan);
			}
			break;
		case BELLE_SIP_CHANNEL_DISCONNECTED:
			if (!chan->force_close) {
				/* This may happen while there is a pending requests, typically when the client decides to send a new
				 * request while the server decides to close the connection because of inactivity timeout. Handle this
				 * case by re-submitting the pending request(s). */
				http_channel_context_handle_disconnection(ctx, chan);
				provider_remove_channel(ctx->provider, chan);
			} else {
				/*In case of force close, manage DISCONNECTED as an io error in order to notify potential pending
				 * requests*/
				http_channel_context_handle_io_error(ctx, chan);
			}
			break;
	}
}

static void belle_http_channel_context_uninit(belle_http_channel_context_t *obj) {
	belle_sip_list_free_with_data(obj->pending_requests, belle_sip_object_unref);
}

static void on_channel_destroyed(belle_http_channel_context_t *obj, belle_sip_channel_t *chan_being_destroyed) {
	belle_sip_channel_remove_listener(chan_being_destroyed, BELLE_SIP_CHANNEL_LISTENER(obj));
	belle_sip_object_unref(obj);
}

/*
 * The http channel context stores pending requests so that they can be matched with response received.
 * It is associated with the channel when the channel is created, and automatically destroyed when the channel is
 *destroyed.
 **/
belle_http_channel_context_t *belle_http_channel_context_new(belle_sip_channel_t *chan, belle_http_provider_t *prov) {
	belle_http_channel_context_t *obj = belle_sip_object_new(belle_http_channel_context_t);
	obj->provider = prov;
	belle_sip_channel_add_listener(chan, (belle_sip_channel_listener_t *)obj);
	belle_sip_object_weak_ref(chan, (belle_sip_object_destroy_notify_t)on_channel_destroyed, obj);
	return obj;
}

int belle_http_channel_is_busy(belle_sip_channel_t *obj) {
	belle_sip_list_t *it;
	if (obj->outgoing_messages != NULL) {
		return 1;
	}
	/*fixme, a litle bit intrusive*/
	for (it = obj->full_listeners; it != NULL; it = it->next) {
		if (BELLE_SIP_IS_INSTANCE_OF(it->data, belle_http_channel_context_t)) {
			belle_http_channel_context_t *obj = it->data;
			return obj->pending_requests != NULL;
		}
	}
	return 0;
}

BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(belle_http_channel_context_t, belle_sip_channel_listener_t)
channel_state_changed, channel_on_message_headers, channel_on_message, channel_on_sending,
    channel_on_auth_requested BELLE_SIP_IMPLEMENT_INTERFACE_END

        BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(belle_http_channel_context_t, belle_sip_channel_listener_t);
BELLE_SIP_INSTANCIATE_VPTR(
    belle_http_channel_context_t, belle_sip_object_t, belle_http_channel_context_uninit, NULL, NULL, FALSE);

static void http_provider_uninit(belle_http_provider_t *obj) {
	belle_sip_message("http provider destroyed.");
	belle_sip_free(obj->bind_ip);
	belle_sip_object_unref(obj->tcp_channels);
	belle_sip_object_unref(obj->tls_channels);
	belle_sip_object_unref(obj->crypto_config);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_http_provider_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_http_provider_t, belle_sip_object_t, http_provider_uninit, NULL, NULL, FALSE);

belle_http_provider_t *belle_http_provider_new(belle_sip_stack_t *s, const char *bind_ip, const uint8_t transports) {
	belle_http_provider_t *p = belle_sip_object_new(belle_http_provider_t);
	p->stack = s;
	p->bind_ip = belle_sip_strdup(bind_ip);
	p->ai_family = strchr(p->bind_ip, ':') ? AF_INET6 : AF_INET;
	p->crypto_config = belle_tls_crypto_config_new();
	p->transports = transports;
	p->simulated_recv_return = 1;
	p->tcp_channels = belle_sip_channel_bank_new();
	p->tls_channels = belle_sip_channel_bank_new();
	return p;
}

static void split_request_url(belle_http_request_t *req) {
	belle_generic_uri_t *uri = belle_http_request_get_uri(req);
	belle_generic_uri_t *new_uri;
	char *host_value;
	const char *path;

	if (belle_generic_uri_get_host(uri) == NULL && req->orig_uri != NULL) return; /*already processed request uri*/
	path = belle_generic_uri_get_path(uri);
	if (path == NULL) path = "/";
	new_uri = belle_generic_uri_new();

	belle_generic_uri_set_path(new_uri, path);
	belle_generic_uri_set_query(new_uri, belle_generic_uri_get_query(uri));
	if (belle_generic_uri_get_port(uri) > 0)
		host_value = belle_sip_strdup_printf("%s:%i", belle_generic_uri_get_host(uri), belle_generic_uri_get_port(uri));
	else host_value = belle_sip_strdup(belle_generic_uri_get_host(uri));

	belle_sip_message_set_header(BELLE_SIP_MESSAGE(req), belle_sip_header_create("Host", host_value));
	belle_sip_free(host_value);
	SET_OBJECT_PROPERTY(req, orig_uri, uri);
	belle_http_request_set_uri(req, new_uri);
}

static void fix_request(belle_http_request_t *req) {
	size_t size = belle_sip_message_get_body_size((belle_sip_message_t *)req);
	belle_sip_header_content_length_t *ctlen =
	    belle_sip_message_get_header_by_type(req, belle_sip_header_content_length_t);
	if (size > 0 && !ctlen) {
		belle_sip_message_add_header((belle_sip_message_t *)req,
		                             (belle_sip_header_t *)belle_sip_header_content_length_create(size));
	}
}

static belle_sip_channel_bank_t *belle_http_provider_get_channels(belle_http_provider_t *obj,
                                                                  const char *transport_name) {
	if (strcasecmp(transport_name, "tcp") == 0) return obj->tcp_channels;
	else if (strcasecmp(transport_name, "tls") == 0) return obj->tls_channels;
	else {
		belle_sip_error("belle_http_provider_send_request(): unsupported transport %s", transport_name);
		return NULL;
	}
}

static void provider_remove_channel(belle_http_provider_t *obj, belle_sip_channel_t *chan) {
	belle_sip_channel_bank_t *channels =
	    belle_http_provider_get_channels(obj, belle_sip_channel_get_transport_name(chan));
	belle_sip_channel_bank_remove_channel(channels, chan);
	belle_sip_message("channel [%p] removed from http provider.", chan);
}

static void belle_http_end_background_task(void *data) {
	belle_http_request_t *req = BELLE_HTTP_REQUEST(data);
	belle_sip_warning("Ending unfinished HTTP transfer background task id=[%x]", req->background_task_id);
	if (req->background_task_id) {
		belle_sip_end_background_task(req->background_task_id);
		req->background_task_id = 0;
	}
}

int belle_http_provider_send_request(belle_http_provider_t *obj,
                                     belle_http_request_t *req,
                                     belle_http_request_listener_t *listener) {
	belle_sip_channel_t *chan;
	belle_sip_channel_bank_t *channels;
	belle_sip_hop_t *hop = belle_sip_hop_new_from_generic_uri(req->orig_uri ? req->orig_uri : req->req_uri);

	if (hop->host == NULL) {
		belle_sip_error("belle_http_provider_send_request(): no host defined in request uri.");
		belle_sip_object_unref(hop);
		return -1;
	}

	channels = belle_http_provider_get_channels(obj, hop->transport);
	if (!channels) {
		belle_sip_object_unref(hop);
		return -1;
	}

	if (listener) belle_http_request_set_listener(req, listener);

	chan = belle_sip_channel_bank_find(channels, obj->ai_family, hop);

	if (chan) {
		// we cannot use the same channel for multiple requests yet since only the first
		// one will be processed. Instead of queuing/serializing requests on a single channel,
		// we currently create one channel per request if needed.
		if (belle_http_channel_is_busy(chan)) {
			belle_sip_message("%s: found an available channel but was busy, creating a new one", __FUNCTION__);
			chan = NULL;
		}
	}
	if (!chan) {
		if (strcasecmp(hop->transport, "tcp") == 0) {
			if ((obj->transports & BELLE_SIP_HTTP_TRANSPORT_TCP) == 0) {
				char *uri_as_string = belle_generic_uri_to_string(req->orig_uri ? req->orig_uri : req->req_uri);
				belle_sip_error(
				    "%s: cannot process request to [%s] as this provider is not configured to process http requests",
				    __FUNCTION__, uri_as_string);
				belle_sip_free(uri_as_string);
				belle_sip_object_unref(hop);
				return -1;
			}
			chan = belle_sip_stream_channel_new_client(obj->stack, obj->bind_ip, 0, hop->cname, hop->host, hop->port,
			                                           FALSE);
		} else if (strcasecmp(hop->transport, "tls") == 0) {
			if ((obj->transports & BELLE_SIP_HTTP_TRANSPORT_TLS) == 0) {
				char *uri_as_string = belle_generic_uri_to_string(req->orig_uri ? req->orig_uri : req->req_uri);
				belle_sip_error(
				    "%s: cannot process request to [%s] as this provider is not configured to process https requests",
				    __FUNCTION__, uri_as_string);
				belle_sip_free(uri_as_string);
				belle_sip_object_unref(hop);
				return -1;
			}
			chan = belle_sip_channel_new_tls(obj->stack, obj->crypto_config, obj->bind_ip, 0, hop->cname, hop->host,
			                                 hop->port, FALSE);
		}

		if (!chan) {
			belle_sip_error("%s: cannot create channel for [%s:%s:%i]", __FUNCTION__, hop->transport, hop->cname,
			                hop->port);
			belle_sip_object_unref(hop);
			return -1;
		}
		if (obj->simulated_recv_return != 1) {
			belle_sip_channel_set_simulated_recv_return(chan, obj->simulated_recv_return);
		}
		belle_http_channel_context_new(chan, obj);
		belle_sip_channel_bank_add_channel(channels, chan);
		belle_sip_object_unref(chan);
	}
	belle_sip_object_unref(hop);
	split_request_url(req);
	fix_request(req);

	belle_http_request_set_channel(req, chan);
	if (req->background_task_id != 0) {
		req->background_task_id =
		    belle_sip_begin_background_task("belle-sip http", belle_http_end_background_task, req);
	}

	belle_sip_channel_queue_message(chan, BELLE_SIP_MESSAGE(req));
	return 0;
}

static void reenqueue_request(belle_http_request_t *req, belle_http_provider_t *prov) {
	belle_http_provider_send_request(prov, req, req->listener);
}

void belle_http_provider_cancel_request(belle_http_provider_t *obj, belle_http_request_t *req) {
	belle_sip_list_t *outgoing_messages;

	belle_http_request_cancel(req);
	if (req->channel) {
		// Keep the list of the outgoing messages of the channel...
		outgoing_messages =
		    belle_sip_list_copy_with_data(req->channel->outgoing_messages, (void *(*)(void *))belle_sip_object_ref);
		if (outgoing_messages && outgoing_messages->data == req) {
			/*our request didn't go out; so drop it.*/
			outgoing_messages = belle_sip_list_remove(outgoing_messages, req);
			belle_sip_object_unref(req);
		}
		/*protect the channel from being destroyed before removing it (removing it will unref it)*/
		belle_sip_object_ref(req->channel);
		provider_remove_channel(obj, req->channel);
		// ... close the channel...
		belle_sip_channel_force_close(req->channel);
		belle_sip_object_unref(req->channel);
		// ... and reenqueue the previously queued outgoing messages into a new channel
		belle_sip_list_for_each2(outgoing_messages, (void (*)(void *, void *))reenqueue_request, obj);
		belle_sip_list_free_with_data(outgoing_messages, belle_sip_object_unref);
	}
}

int belle_http_provider_set_tls_verify_policy(belle_http_provider_t *obj, belle_tls_verify_policy_t *verify_ctx) {
	SET_OBJECT_PROPERTY(obj, crypto_config, verify_ctx);
	return 0;
}

int belle_http_provider_set_tls_crypto_config(belle_http_provider_t *obj, belle_tls_crypto_config_t *crypto_config) {
	SET_OBJECT_PROPERTY(obj, crypto_config, crypto_config);
	return 0;
}

static void apply_simulated_return(belle_sip_channel_t *chan, void *userdata) {
	belle_http_provider_t *obj = (belle_http_provider_t *)userdata;
	belle_sip_channel_set_simulated_recv_return(chan, obj->simulated_recv_return);
}

void belle_http_provider_set_recv_error(belle_http_provider_t *obj, int recv_error) {
	obj->simulated_recv_return = recv_error;
	belle_sip_channel_bank_for_each(obj->tcp_channels, apply_simulated_return, obj);
	belle_sip_channel_bank_for_each(obj->tls_channels, apply_simulated_return, obj);
}
