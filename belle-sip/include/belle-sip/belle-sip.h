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
#ifndef BELLE_SIP_H
#define BELLE_SIP_H

#include <stdlib.h>

#include "belle-sip/defs.h"
#include "belle-sip/object.h"

/**
 * This enum declares all object types used in belle-sip (see belle_sip_object_t)
**/
BELLE_SIP_DECLARE_TYPES_BEGIN(belle_sip,1)
	BELLE_SIP_TYPE_ID(belle_sip_stack_t),
	BELLE_SIP_TYPE_ID(belle_sip_hop_t),
	BELLE_SIP_TYPE_ID(belle_sip_object_pool_t),
	BELLE_SIP_TYPE_ID(belle_sip_listening_point_t),
	BELLE_SIP_TYPE_ID(belle_sip_datagram_listening_point_t),
	BELLE_SIP_TYPE_ID(belle_sip_udp_listening_point_t),
	BELLE_SIP_TYPE_ID(belle_sip_stream_listening_point_t),
	BELLE_SIP_TYPE_ID(belle_sip_tls_listening_point_t),
	BELLE_SIP_TYPE_ID(belle_sip_tunnel_listening_point_t),
	BELLE_SIP_TYPE_ID(belle_sip_channel_t),
	BELLE_SIP_TYPE_ID(belle_sip_udp_channel_t),
	BELLE_SIP_TYPE_ID(belle_sip_stream_channel_t),
	BELLE_SIP_TYPE_ID(belle_sip_tls_channel_t),
	BELLE_SIP_TYPE_ID(belle_sip_tunnel_channel_t),
	BELLE_SIP_TYPE_ID(belle_sip_provider_t),
	BELLE_SIP_TYPE_ID(belle_sip_main_loop_t),
	BELLE_SIP_TYPE_ID(belle_sip_source_t),
	BELLE_SIP_TYPE_ID(belle_sip_resolver_context_t),
	BELLE_SIP_TYPE_ID(belle_sip_transaction_t),
	BELLE_SIP_TYPE_ID(belle_sip_server_transaction_t),
	BELLE_SIP_TYPE_ID(belle_sip_client_transaction_t),
	BELLE_SIP_TYPE_ID(belle_sip_ict_t),
	BELLE_SIP_TYPE_ID(belle_sip_nict_t),
	BELLE_SIP_TYPE_ID(belle_sip_ist_t),
	BELLE_SIP_TYPE_ID(belle_sip_nist_t),
	BELLE_SIP_TYPE_ID(belle_sip_dialog_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_address_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_contact_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_from_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_to_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_via_t),
	BELLE_SIP_TYPE_ID(belle_sip_uri_t),
	BELLE_SIP_TYPE_ID(belle_sip_message_t),
	BELLE_SIP_TYPE_ID(belle_sip_request_t),
	BELLE_SIP_TYPE_ID(belle_sip_response_t),
	BELLE_SIP_TYPE_ID(belle_sip_object_t),
	BELLE_SIP_TYPE_ID(belle_sip_parameters_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_call_id_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_cseq_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_content_type_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_route_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_record_route_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_user_agent_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_content_length_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_extension_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_authorization_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_proxy_authorization_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_www_authenticate_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_proxy_authenticate_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_max_forwards_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_expires_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_allow_t),
	BELLE_SIP_TYPE_ID(belle_sdp_attribute_t),
	BELLE_SIP_TYPE_ID(belle_sdp_bandwidth_t),
	BELLE_SIP_TYPE_ID(belle_sdp_connection_t),
	BELLE_SIP_TYPE_ID(belle_sdp_email_t),
	BELLE_SIP_TYPE_ID(belle_sdp_info_t),
	BELLE_SIP_TYPE_ID(belle_sdp_key_t),
	BELLE_SIP_TYPE_ID(belle_sdp_media_t),
	BELLE_SIP_TYPE_ID(belle_sdp_media_description_t),
	BELLE_SIP_TYPE_ID(belle_sdp_origin_t),
	BELLE_SIP_TYPE_ID(belle_sdp_phone_t),
	BELLE_SIP_TYPE_ID(belle_sdp_repeate_time_t),
	BELLE_SIP_TYPE_ID(belle_sdp_session_description_t),
	BELLE_SIP_TYPE_ID(belle_sdp_session_name_t),
	BELLE_SIP_TYPE_ID(belle_sdp_time_t),
	BELLE_SIP_TYPE_ID(belle_sdp_time_description_t),
	BELLE_SIP_TYPE_ID(belle_sdp_uri_t),
	BELLE_SIP_TYPE_ID(belle_sdp_version_t),
	BELLE_SIP_TYPE_ID(belle_sdp_base_description_t),
	BELLE_SIP_TYPE_ID(belle_sdp_mime_parameter_t),
	BELLE_SIP_TYPE_ID(belle_sip_callbacks_t),
	BELLE_SIP_TYPE_ID(belle_sip_refresher_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_subscription_state_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_service_route_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_refer_to_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_referred_by_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_replaces_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_date_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_p_preferred_identity_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_privacy_t),
	BELLE_SIP_TYPE_ID(belle_sip_certificates_chain_t),
	BELLE_SIP_TYPE_ID(belle_sip_signing_key_t)
BELLE_SIP_DECLARE_TYPES_END


enum belle_sip_interface_ids{
	belle_sip_interface_id_first=1,
	BELLE_SIP_INTERFACE_ID(belle_sip_channel_listener_t),
	BELLE_SIP_INTERFACE_ID(belle_sip_listener_t)
};

BELLE_SIP_BEGIN_DECLS

BELLESIP_EXPORT void *belle_sip_malloc(size_t size);
BELLESIP_EXPORT void *belle_sip_malloc0(size_t size);
BELLESIP_EXPORT void *belle_sip_realloc(void *ptr, size_t size);
BELLESIP_EXPORT void belle_sip_free(void *ptr);
BELLESIP_EXPORT char * belle_sip_strdup(const char *s);

BELLE_SIP_END_DECLS

/*these types are declared here because they are widely used in many headers included after*/

typedef struct belle_sip_listening_point belle_sip_listening_point_t;
typedef struct belle_sip_tls_listening_point belle_sip_tls_listening_point_t;
typedef struct belle_sip_stack belle_sip_stack_t;
typedef struct belle_sip_provider belle_sip_provider_t;
typedef struct belle_sip_dialog belle_sip_dialog_t;
typedef struct belle_sip_transaction belle_sip_transaction_t;
typedef struct belle_sip_server_transaction belle_sip_server_transaction_t;
typedef struct belle_sip_client_transaction belle_sip_client_transaction_t;
typedef struct _belle_sip_message belle_sip_message_t;
typedef struct _belle_sip_request belle_sip_request_t;
typedef struct _belle_sip_response belle_sip_response_t;
typedef struct belle_sip_hop belle_sip_hop_t;

#include "belle-sip/utils.h"
#include "belle-sip/list.h"
#include "belle-sip/listener.h"
#include "belle-sip/mainloop.h"
#include "belle-sip/uri.h"
#include "belle-sip/headers.h"
#include "belle-sip/parameters.h"
#include "belle-sip/message.h"
#include "belle-sip/refresher.h"
#include "belle-sip/transaction.h"
#include "belle-sip/dialog.h"
#include "belle-sip/sipstack.h"
#include "belle-sip/resolver.h"
#include "belle-sip/listeningpoint.h"
#include "belle-sip/provider.h"
#include "belle-sip/auth-helper.h"



#undef TRUE
#define TRUE 1


#undef FALSE
#define FALSE 0


#define BELLE_SIP_POINTER_TO_INT(p)	((int)(long)(p))
#define BELLE_SIP_INT_TO_POINTER(i)	((void*)(long)(i))

#endif

