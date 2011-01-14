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


#ifndef belle_sip_listener_h
#define belle_sip_listener_h


typedef struct belle_sip_dialog_terminated_event belle_sip_dialog_terminated_event_t;

typedef struct belle_sip_io_error_event belle_sip_io_error_event_t;

typedef struct belle_sip_request_event belle_sip_request_event_t;

typedef struct belle_sip_response_event belle_sip_response_event_t;

typedef struct belle_sip_timeout_event belle_sip_timeout_event_t;

typedef struct belle_sip_transaction_terminated_event belle_sip_transaction_terminated_event_t;

struct belle_sip_dialog_terminated_event{
	belle_sip_provider_t *source;
	belle_sip_dialog_t *dialog;
};

struct belle_sip_io_error_event{
	belle_sip_provider_t *source;
	char *transport;
	char *host;
	int port;
};

struct belle_sip_request_event{
	belle_sip_provider_t *source;
	belle_sip_server_transaction_t *server_transaction;
	belle_sip_dialog_t *dialog;
	belle_sip_request_t *request;
};

struct belle_sip_response_event{
	belle_sip_provider_t *source;
	belle_sip_client_transaction_t *client_transaction;
	belle_sip_dialog_t *dialog;
	belle_sip_request_t *response;
};

struct belle_sip_timeout_event{
	belle_sip_provider_t *source;
	belle_sip_transaction_t *transaction;
	int is_server_transaction;
};

struct belle_sip_transaction_terminated_event{
	belle_sip_provider_t *source;
	belle_sip_transaction_t *transaction;
	int is_server_transaction;
};

struct belle_sip_listener{
	void (*process_dialog_terminated)(void *user_ctx, const belle_sip_dialog_terminated_event_t *event);
	void (*process_io_error)(void *user_ctx, const belle_sip_io_error_event_t *event);
	void (*process_request_event)(void *user_ctx, const belle_sip_request_event_t *event);
	void (*process_response_event)(void *user_ctx, const belle_sip_response_event_t *event);
	void (*process_timeout)(void *user_ctx, const belle_sip_timeout_event_t *event);
	void (*process_transaction_terminated)(void *user_ctx, const belle_sip_transaction_terminated_event_t *event);
};


#endif

