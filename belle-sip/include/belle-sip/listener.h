/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

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


#ifndef belle_sip_listener_h
#define belle_sip_listener_h

BELLE_SIP_BEGIN_DECLS

typedef struct belle_sip_dialog_terminated_event belle_sip_dialog_terminated_event_t;
typedef struct belle_sip_io_error_event belle_sip_io_error_event_t;
typedef struct belle_sip_request_event belle_sip_request_event_t;
typedef struct belle_sip_response_event belle_sip_response_event_t;
typedef struct belle_sip_timeout_event belle_sip_timeout_event_t;
typedef struct belle_sip_transaction_terminated_event belle_sip_transaction_terminated_event_t;
typedef struct belle_sip_auth_event belle_sip_auth_event_t;
typedef struct belle_sip_certificates_chain belle_sip_certificates_chain_t;
typedef struct belle_sip_signing_key belle_sip_signing_key_t;


BELLE_SIP_DECLARE_INTERFACE_BEGIN(belle_sip_listener_t)
	void (*process_dialog_terminated)(belle_sip_listener_t *user_ctx, const belle_sip_dialog_terminated_event_t *event);
	void (*process_io_error)(belle_sip_listener_t *user_ctx, const belle_sip_io_error_event_t *event);
	void (*process_request_event)(belle_sip_listener_t *user_ctx, const belle_sip_request_event_t *event);
	void (*process_response_event)(belle_sip_listener_t *user_ctx, const belle_sip_response_event_t *event);
	void (*process_timeout)(belle_sip_listener_t *user_ctx, const belle_sip_timeout_event_t *event);
	void (*process_transaction_terminated)(belle_sip_listener_t *user_ctx, const belle_sip_transaction_terminated_event_t *event);
	void (*process_auth_requested)(belle_sip_listener_t *user_ctx, belle_sip_auth_event_t *event);
BELLE_SIP_DECLARE_INTERFACE_END

#define BELLE_SIP_LISTENER(obj) BELLE_SIP_INTERFACE_CAST(obj,belle_sip_listener_t)

/*Response event*/
BELLESIP_EXPORT belle_sip_response_t* belle_sip_response_event_get_response(const belle_sip_response_event_t* event);
BELLESIP_EXPORT belle_sip_client_transaction_t *belle_sip_response_event_get_client_transaction(const belle_sip_response_event_t* event);
BELLESIP_EXPORT belle_sip_dialog_t *belle_sip_response_event_get_dialog(const belle_sip_response_event_t* event);

/*Request event*/
BELLESIP_EXPORT belle_sip_request_t* belle_sip_request_event_get_request(const belle_sip_request_event_t* event);
BELLESIP_EXPORT belle_sip_server_transaction_t *belle_sip_request_event_get_server_transaction(const belle_sip_request_event_t* event);
BELLESIP_EXPORT belle_sip_dialog_t *belle_sip_request_event_get_dialog(const belle_sip_request_event_t* event);

/*Dialog terminated event*/
BELLESIP_EXPORT belle_sip_dialog_t* belle_sip_dialog_terminated_event_get_dialog(const belle_sip_dialog_terminated_event_t *event);
BELLESIP_EXPORT int belle_sip_dialog_terminated_event_is_expired(const belle_sip_dialog_terminated_event_t *event);

/**
 * Timeout Event
 */
BELLESIP_EXPORT belle_sip_client_transaction_t *belle_sip_timeout_event_get_client_transaction(const belle_sip_timeout_event_t* event);
belle_sip_server_transaction_t *belle_sip_timeout_event_get_server_transaction(const belle_sip_timeout_event_t* event);

/**
 * Transaction Termonated Event
 */
BELLESIP_EXPORT belle_sip_client_transaction_t *belle_sip_transaction_terminated_event_get_client_transaction(const belle_sip_transaction_terminated_event_t* event);
BELLESIP_EXPORT belle_sip_server_transaction_t *belle_sip_transaction_terminated_event_get_server_transaction(const belle_sip_transaction_terminated_event_t* event);


/**
 * auth event mode
 * */
typedef enum belle_sip_auth_mode {
	BELLE_SIP_AUTH_MODE_HTTP_DIGEST, /** Digest authentication has been requested by the server*/
	BELLE_SIP_AUTH_MODE_TLS /** Client certificate has been requested by the server*/
} belle_sip_auth_mode_t;

BELLESIP_EXPORT void belle_sip_auth_event_destroy(belle_sip_auth_event_t* event);
BELLESIP_EXPORT const char* belle_sip_auth_event_get_username(const belle_sip_auth_event_t* event);
BELLESIP_EXPORT void belle_sip_auth_event_set_username(belle_sip_auth_event_t* event, const char* value);

BELLESIP_EXPORT const char* belle_sip_auth_event_get_userid(const belle_sip_auth_event_t* event);
BELLESIP_EXPORT void belle_sip_auth_event_set_userid(belle_sip_auth_event_t* event, const char* value);

BELLESIP_EXPORT const char* belle_sip_auth_event_get_realm(const belle_sip_auth_event_t* event);
BELLESIP_EXPORT void belle_sip_auth_event_set_realm(belle_sip_auth_event_t* event, const char* value);

BELLESIP_EXPORT const char* belle_sip_auth_event_get_domain(const belle_sip_auth_event_t* event);
BELLESIP_EXPORT void belle_sip_auth_event_set_domain(belle_sip_auth_event_t* event, const char* value);

BELLESIP_EXPORT const char* belle_sip_auth_event_get_passwd(const belle_sip_auth_event_t* event);
BELLESIP_EXPORT void belle_sip_auth_event_set_passwd(belle_sip_auth_event_t* event, const char* value);

BELLESIP_EXPORT const char* belle_sip_auth_event_get_ha1(const belle_sip_auth_event_t* event);
BELLESIP_EXPORT void belle_sip_auth_event_set_ha1(belle_sip_auth_event_t* event, const char* value);

/**
 * get the authentication mode requested by the server, can be either TLS client certificates of http digest
 * @param event
 * @return  belle_sip_auth_mode_t
 * */
BELLESIP_EXPORT belle_sip_auth_mode_t belle_sip_auth_event_get_mode(const belle_sip_auth_event_t* event);


/**
 * In case of TLS auth, get value of the distinguished name sent by the server
 * @param event
 * @return DN has sent by the server
 *
 */
BELLESIP_EXPORT const char* belle_sip_auth_event_get_distinguished_name(const belle_sip_auth_event_t* event);
/**
 * get client certificate
 * @return  belle_sip_certificate_t*
 * */
BELLESIP_EXPORT belle_sip_certificates_chain_t* belle_sip_auth_event_get_client_certificates_chain(const belle_sip_auth_event_t* event);
/**
 * set client certificate to be sent in answer to the certificate request issued by the server for the DN belle_sip_auth_event_get_distinguished_name() name
 * @return  belle_sip_certificate_t*
 * */
BELLESIP_EXPORT void belle_sip_auth_event_set_client_certificates_chain(belle_sip_auth_event_t* event, belle_sip_certificates_chain_t* value);
/**
 * get client certificate private key
 * @return  belle_sip_signing_key_t*
 * */
BELLESIP_EXPORT belle_sip_signing_key_t* belle_sip_auth_event_get_signing_key(const belle_sip_auth_event_t* event);
/**
 * set the private key attached to the client certificate.
 * @param event belle_sip_auth_event_t
 * @param value belle_sip_signing_key_t signing key
 * */
BELLESIP_EXPORT void belle_sip_auth_event_set_signing_key(belle_sip_auth_event_t* event, belle_sip_signing_key_t* value);


/*Io error event*/
/*
 * Give access to the remote host
 * @param event object
 * @return host value the socket is pointing to
 * */
const char* belle_sip_io_error_event_get_host(const belle_sip_io_error_event_t* event);
/*
 * Give access to the used transport
 * @param event object
 * @return host value the socket is pointing to
 * */
const char* belle_sip_io_error_event_get_transport(const belle_sip_io_error_event_t* event);
/*
 * Give access to the remote port
 * @param event object
 * @return port value the socket is pointing to
 * */
unsigned int belle_sip_io_error_event_port(const belle_sip_io_error_event_t* event);

/*
 * Get access to the object involved in this error, can be either belle_sip_dialog_t or belle_sip_transaction_t or belle_sip_provider_t
 * @param event
 * @return belle_sip_object_t source, use belle_sip_object_is_instance_of to check returns type
 * */

BELLESIP_EXPORT belle_sip_object_t* belle_sip_io_error_event_get_source(const belle_sip_io_error_event_t* event);


struct belle_sip_listener_callbacks{
	void (*process_dialog_terminated)(void *user_ctx, const belle_sip_dialog_terminated_event_t *event);
	void (*process_io_error)(void *user_ctx, const belle_sip_io_error_event_t *event);
	void (*process_request_event)(void *user_ctx, const belle_sip_request_event_t *event);
	void (*process_response_event)(void *user_ctx, const belle_sip_response_event_t *event);
	void (*process_timeout)(void *user_ctx, const belle_sip_timeout_event_t *event);
	void (*process_transaction_terminated)(void *user_ctx, const belle_sip_transaction_terminated_event_t *event);
	void (*process_auth_requested)(void *user_ctx, belle_sip_auth_event_t *auth_event);
	void (*listener_destroyed)(void *user_ctx);
};

typedef struct belle_sip_listener_callbacks belle_sip_listener_callbacks_t;

/**
 * Creates an object implementing the belle_sip_listener_t interface.
 * This object passes the events to the callbacks, providing also the user context.
**/
BELLESIP_EXPORT belle_sip_listener_t *belle_sip_listener_create_from_callbacks(const belle_sip_listener_callbacks_t *callbacks, void *user_ctx);

BELLE_SIP_END_DECLS

#endif

