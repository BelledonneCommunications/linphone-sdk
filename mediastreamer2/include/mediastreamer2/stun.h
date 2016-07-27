/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2012-2016  Belledonne Communications

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef MS_STUN_H
#define MS_STUN_H


#include <ortp/rtpsession.h>
#include <mediastreamer2/mscommon.h>


#define MS_STUN_MAX_MESSAGE_SIZE 2048
#define MS_STUN_MAGIC_COOKIE 0x2112A442

#define MS_STUN_ADDR_FAMILY_IPV4 0x01
#define MS_STUN_ADDR_FAMILY_IPV6 0x02

#define MS_STUN_TYPE_REQUEST          0x0000
#define MS_STUN_TYPE_INDICATION       0x0010
#define MS_STUN_TYPE_SUCCESS_RESPONSE 0x0100
#define MS_STUN_TYPE_ERROR_RESPONSE   0x0110

#define MS_STUN_METHOD_BINDING           0x001
#define MS_STUN_METHOD_SHARED_SECRET     0x002 /* Deprecated, now reserved */

#define MS_TURN_METHOD_ALLOCATE          0x003
#define MS_TURN_METHOD_REFRESH           0x004
#define MS_TURN_METHOD_SEND              0x006
#define MS_TURN_METHOD_DATA              0x007
#define MS_TURN_METHOD_CREATE_PERMISSION 0x008
#define MS_TURN_METHOD_CHANNEL_BIND      0x009


#define MS_STUN_ATTR_MAPPED_ADDRESS           0x0001
#define MS_STUN_ATTR_RESPONSE_ADDRESS         0x0002 /* Deprecated, now reserved */
#define MS_STUN_ATTR_CHANGE_REQUEST           0x0003 /* Deprecated, now reserved */
#define MS_STUN_ATTR_SOURCE_ADDRESS           0x0004 /* Deprecated, now reserved */
#define MS_STUN_ATTR_CHANGED_ADDRESS          0x0005 /* Deprecated, now reserved */
#define MS_STUN_ATTR_USERNAME                 0x0006
#define MS_STUN_ATTR_PASSWORD                 0x0007 /* Deprecated, now reserved */
#define MS_STUN_ATTR_MESSAGE_INTEGRITY        0x0008
#define MS_STUN_ATTR_ERROR_CODE               0x0009
#define MS_STUN_ATTR_UNKNOWN_ATTRIBUTES       0x000A
#define MS_STUN_ATTR_REFLECTED_FROM           0x000B
#define MS_STUN_ATTR_REALM                    0x0014
#define MS_STUN_ATTR_NONCE                    0x0015
#define MS_STUN_ATTR_XOR_MAPPED_ADDRESS       0x0020

#define MS_STUN_ATTR_SOFTWARE                 0x8022
#define MS_STUN_ATTR_ALTERNATE_SERVER         0x8023
#define MS_STUN_ATTR_FINGERPRINT              0x8028

#define MS_TURN_ATTR_CHANNEL_NUMBER           0x000C
#define MS_TURN_ATTR_LIFETIME                 0x000D
#define MS_TURN_ATTR_BANDWIDTH                0x0010 /* Deprecated, now reserved */
#define MS_TURN_ATTR_XOR_PEER_ADDRESS         0x0012
#define MS_TURN_ATTR_DATA                     0x0013
#define MS_TURN_ATTR_XOR_RELAYED_ADDRESS      0x0016
#define MS_TURN_ATTR_REQUESTED_ADDRESS_FAMILY 0x0017
#define MS_TURN_ATTR_EVEN_PORT                0x0018
#define MS_TURN_ATTR_REQUESTED_TRANSPORT      0x0019
#define MS_TURN_ATTR_DONT_FRAGMENT            0x001A
#define MS_TURN_ATTR_TIMER_VAL                0x0021 /* Deprecated, now reserved */
#define MS_TURN_ATTR_RESERVATION_TOKEN        0x0022

#define MS_ICE_ATTR_PRIORITY                  0x0024
#define MS_ICE_ATTR_USE_CANDIDATE             0x0025
#define MS_ICE_ATTR_ICE_CONTROLLED            0x8029
#define MS_ICE_ATTR_ICE_CONTROLLING           0x802A


#define MS_STUN_ERROR_CODE_TRY_ALTERNATE                  300
#define MS_STUN_ERROR_CODE_BAD_REQUEST                    400
#define MS_STUN_ERROR_CODE_UNAUTHORIZED                   401
#define MS_STUN_ERROR_CODE_UNKNOWN_ATTRIBUTE              420
#define MS_STUN_ERROR_CODE_STALE_NONCE                    438
#define MS_STUN_ERROR_CODE_SERVER_ERROR                   500

#define MS_TURN_ERROR_CODE_FORBIDDEN                      403
#define MS_TURN_ERROR_CODE_ALLOCATION_MISMATCH            437
#define MS_TURN_ERROR_CODE_WRONG_CREDENTIALS              441
#define MS_TURN_ERROR_CODE_UNSUPPORTED_TRANSPORT_PROTOCOL 442
#define MS_TURN_ERROR_CODE_ALLOCATION_QUOTA_REACHED       486
#define MS_TURN_ERROR_CODE_INSUFFICIENT_CAPACITY          508

#define MS_ICE_ERROR_CODE_ROLE_CONFLICT                   487


typedef struct {
	uint16_t port;
	uint32_t addr;
} MSStunAddress4;

typedef struct {
	uint16_t port;
	UInt128 addr;
} MSStunAddress6;

typedef struct {
	union {
		MSStunAddress4 v4;
		MSStunAddress6 v6;
	} ip;
	uint8_t family;
} MSStunAddress;

typedef struct {
	char *reason;
	uint16_t number;
} MSStunErrorCode;

typedef struct {
	uint16_t type;
	uint16_t method;
	uint16_t length;
	UInt96 tr_id;
	uint8_t *data;
	char *username;
	char *password;
	char *ha1;
	char *realm;
	char *message_integrity;
	char *software;
	char *nonce;
	MSStunErrorCode error_code;
	MSStunAddress mapped_address;
	MSStunAddress xor_mapped_address;
	MSStunAddress xor_peer_address;
	MSStunAddress xor_relayed_address;
	uint32_t change_request;
	uint32_t fingerprint;
	uint32_t priority;
	uint64_t ice_controlling;
	uint64_t ice_controlled;
	uint32_t lifetime;
	uint16_t channel_number;
	uint16_t data_length;
	uint8_t requested_transport;
	uint8_t requested_address_family;
	bool_t include_username_attribute;
	bool_t has_error_code;
	bool_t has_message_integrity;
	bool_t has_dummy_message_integrity;
	bool_t has_fingerprint;
	bool_t has_mapped_address;
	bool_t has_xor_mapped_address;
	bool_t has_xor_peer_address;
	bool_t has_xor_relayed_address;
	bool_t has_priority;
	bool_t has_use_candidate;
	bool_t has_ice_controlling;
	bool_t has_ice_controlled;
	bool_t has_lifetime;
	bool_t has_channel_number;
	bool_t has_requested_transport;
	bool_t has_requested_address_family;
} MSStunMessage;

typedef enum {
	MS_TURN_CONTEXT_STATE_IDLE,
	MS_TURN_CONTEXT_STATE_CREATING_ALLOCATION,
	MS_TURN_CONTEXT_STATE_ALLOCATION_CREATED,
	MS_TURN_CONTEXT_STATE_CREATING_PERMISSIONS,
	MS_TURN_CONTEXT_STATE_PERMISSIONS_CREATED,
	MS_TURN_CONTEXT_STATE_BINDING_CHANNEL,
	MS_TURN_CONTEXT_STATE_CHANNEL_BOUND
} MSTurnContextState;

typedef enum {
	MS_TURN_CONTEXT_TYPE_RTP,
	MS_TURN_CONTEXT_TYPE_RTCP
} MSTurnContextType;

typedef struct {
	uint32_t nb_send_indication;
	uint32_t nb_data_indication;
	uint32_t nb_received_channel_msg;
	uint32_t nb_sent_channel_msg;
	uint16_t nb_successful_allocate;
	uint16_t nb_successful_refresh;
	uint16_t nb_successful_create_permission;
	uint16_t nb_successful_channel_bind;
} MSTurnContextStatistics;

typedef struct {
	RtpSession *rtp_session;
	RtpTransport *endpoint;
	MSList *allowed_peer_addresses;
	char *realm;
	char *nonce;
	char *username;
	char *password;
	char *ha1;
	uint32_t lifetime;
	uint16_t channel_number;
	MSTurnContextState state;
	MSTurnContextType type;
	MSStunAddress relay_addr;
	struct sockaddr *turn_server_addr;
	socklen_t turn_server_addrlen;
	bool_t force_rtp_sending_via_relay;
	MSTurnContextStatistics stats;
} MSTurnContext;


#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*MSStunAuthRequestedCb)(void *userdata, const char *realm, const char *nonce, const char **username, const char **password, const char **ha1);


MS2_PUBLIC bool_t ms_compare_stun_addresses(const MSStunAddress *a1, const MSStunAddress *a2);
MS2_PUBLIC int ms_stun_family_to_af(int stun_family);
MS2_PUBLIC void ms_stun_address_to_sockaddr(const MSStunAddress *stun_addr, struct sockaddr *addr, socklen_t *addrlen);
MS2_PUBLIC void ms_sockaddr_to_stun_address(const struct sockaddr *addr, MSStunAddress *stun_addr);
MS2_PUBLIC MSStunAddress ms_ip_address_to_stun_address(int ai_family, int socktype, const char *hostname, int port);
MS2_PUBLIC void ms_stun_address_to_ip_address(const MSStunAddress *stun_address, char *ip, size_t ip_size, int *port);
MS2_PUBLIC void ms_stun_address_to_printable_ip_address(const MSStunAddress *stun_address, char *printable_ip, size_t printable_ip_size);
MS2_PUBLIC char * ms_stun_calculate_integrity_short_term(const char *buf, size_t bufsize, const char *key);
MS2_PUBLIC char * ms_stun_calculate_integrity_long_term(const char *buf, size_t bufsize, const char *realm, const char *username, const char *password);
MS2_PUBLIC char * ms_stun_calculate_integrity_long_term_from_ha1(const char *buf, size_t bufsize, const char *ha1_text);
MS2_PUBLIC uint32_t ms_stun_calculate_fingerprint(const char *buf, size_t bufsize);

MS2_PUBLIC MSStunMessage * ms_stun_message_create(uint16_t type, uint16_t method);
MS2_PUBLIC MSStunMessage * ms_stun_message_create_from_buffer_parsing(const uint8_t *buf, ssize_t bufsize);
MS2_PUBLIC MSStunMessage * ms_stun_binding_request_create(void);
MS2_PUBLIC MSStunMessage * ms_stun_binding_success_response_create(void);
MS2_PUBLIC MSStunMessage * ms_stun_binding_error_response_create(void);
MS2_PUBLIC MSStunMessage * ms_stun_binding_indication_create(void);
MS2_PUBLIC bool_t ms_stun_message_is_request(const MSStunMessage *msg);
MS2_PUBLIC bool_t ms_stun_message_is_success_response(const MSStunMessage *msg);
MS2_PUBLIC bool_t ms_stun_message_is_error_response(const MSStunMessage *msg);
MS2_PUBLIC bool_t ms_stun_message_is_indication(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_destroy(MSStunMessage *msg);
MS2_PUBLIC size_t ms_stun_message_encode(const MSStunMessage *msg, char **buf);
MS2_PUBLIC uint16_t ms_stun_message_get_method(const MSStunMessage *msg);
MS2_PUBLIC uint16_t ms_stun_message_get_length(const MSStunMessage *msg);
MS2_PUBLIC UInt96 ms_stun_message_get_tr_id(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_tr_id(MSStunMessage *msg, UInt96 tr_id);
MS2_PUBLIC void ms_stun_message_set_random_tr_id(MSStunMessage *msg);
MS2_PUBLIC const char * ms_stun_message_get_username(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_username(MSStunMessage *msg, const char *username);
MS2_PUBLIC void ms_stun_message_include_username_attribute(MSStunMessage *msg, bool_t include);
MS2_PUBLIC const char * ms_stun_message_get_password(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_password(MSStunMessage *msg, const char *password);
MS2_PUBLIC void ms_stun_message_set_ha1(MSStunMessage *msg, const char *ha1_text);
MS2_PUBLIC const char * ms_stun_message_get_realm(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_realm(MSStunMessage *msg, const char *realm);
MS2_PUBLIC const char * ms_stun_message_get_software(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_software(MSStunMessage *msg, const char *software);
MS2_PUBLIC const char * ms_stun_message_get_nonce(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_nonce(MSStunMessage *msg, const char *nonce);
MS2_PUBLIC bool_t ms_stun_message_has_error_code(const MSStunMessage *msg);
MS2_PUBLIC uint16_t ms_stun_message_get_error_code(const MSStunMessage *msg, char **reason);
MS2_PUBLIC void ms_stun_message_set_error_code(MSStunMessage *msg, uint16_t number, const char *reason);
MS2_PUBLIC bool_t ms_stun_message_message_integrity_enabled(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_enable_message_integrity(MSStunMessage *msg, bool_t enable);
MS2_PUBLIC const char * ms_stun_message_get_message_integrity(const MSStunMessage *msg);
MS2_PUBLIC bool_t ms_stun_message_fingerprint_enabled(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_enable_fingerprint(MSStunMessage *msg, bool_t enable);
MS2_PUBLIC const MSStunAddress * ms_stun_message_get_mapped_address(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_mapped_address(MSStunMessage *msg, MSStunAddress mapped_address);
MS2_PUBLIC const MSStunAddress * ms_stun_message_get_xor_mapped_address(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_xor_mapped_address(MSStunMessage *msg, MSStunAddress xor_mapped_address);
MS2_PUBLIC const MSStunAddress * ms_stun_message_get_xor_peer_address(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_xor_peer_address(MSStunMessage *msg, MSStunAddress xor_peer_address);
MS2_PUBLIC const MSStunAddress * ms_stun_message_get_xor_relayed_address(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_xor_relayed_address(MSStunMessage *msg, MSStunAddress xor_relayed_address);
MS2_PUBLIC void ms_stun_message_enable_change_ip(MSStunMessage *msg, bool_t enable);
MS2_PUBLIC void ms_stun_message_enable_change_port(MSStunMessage *msg, bool_t enable);

MS2_PUBLIC bool_t ms_stun_message_has_priority(const MSStunMessage *msg);
MS2_PUBLIC uint32_t ms_stun_message_get_priority(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_priority(MSStunMessage *msg, uint32_t priority);
MS2_PUBLIC bool_t ms_stun_message_use_candidate_enabled(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_enable_use_candidate(MSStunMessage *msg, bool_t enable);
MS2_PUBLIC bool_t ms_stun_message_has_ice_controlling(const MSStunMessage *msg);
MS2_PUBLIC uint64_t ms_stun_message_get_ice_controlling(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_ice_controlling(MSStunMessage *msg, uint64_t value);
MS2_PUBLIC bool_t ms_stun_message_has_ice_controlled(const MSStunMessage *msg);
MS2_PUBLIC uint64_t ms_stun_message_get_ice_controlled(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_ice_controlled(MSStunMessage *msg, uint64_t value);

MS2_PUBLIC bool_t ms_stun_message_dummy_message_integrity_enabled(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_enable_dummy_message_integrity(MSStunMessage *msg, bool_t enable);

MS2_PUBLIC MSStunMessage * ms_turn_allocate_request_create(void);
MS2_PUBLIC MSStunMessage * ms_turn_refresh_request_create(uint32_t lifetime);
MS2_PUBLIC MSStunMessage * ms_turn_create_permission_request_create(MSStunAddress peer_address);
MS2_PUBLIC MSStunMessage * ms_turn_send_indication_create(MSStunAddress peer_address);
MS2_PUBLIC MSStunMessage * ms_turn_channel_bind_request_create(MSStunAddress peer_address, uint16_t channel_number);
MS2_PUBLIC bool_t ms_stun_message_has_requested_transport(const MSStunMessage *msg);
MS2_PUBLIC uint8_t ms_stun_message_get_requested_transport(const MSStunMessage *msg);
MS2_PUBLIC bool_t ms_stun_message_has_requested_address_family(const MSStunMessage *msg);
MS2_PUBLIC uint8_t ms_stun_message_get_requested_address_family(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_requested_address_family(MSStunMessage *msg, uint8_t family);
MS2_PUBLIC bool_t ms_stun_message_has_lifetime(const MSStunMessage *msg);
MS2_PUBLIC uint32_t ms_stun_message_get_lifetime(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_lifetime(MSStunMessage *msg, uint32_t lifetime);
MS2_PUBLIC bool_t ms_stun_message_has_channel_number(const MSStunMessage *msg);
MS2_PUBLIC uint16_t ms_stun_message_get_channel_number(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_channel_number(MSStunMessage *msg, uint16_t channel_number);
MS2_PUBLIC uint8_t * ms_stun_message_get_data(const MSStunMessage *msg);
MS2_PUBLIC uint16_t ms_stun_message_get_data_length(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_data(MSStunMessage *msg, uint8_t *data, uint16_t length);

MS2_PUBLIC MSTurnContext * ms_turn_context_new(MSTurnContextType type, RtpSession *rtp_session);
MS2_PUBLIC void ms_turn_context_destroy(MSTurnContext *context);
MS2_PUBLIC void ms_turn_context_set_server_addr(MSTurnContext *context, struct sockaddr *addr, socklen_t addrlen);
MS2_PUBLIC MSTurnContextState ms_turn_context_get_state(const MSTurnContext *context);
MS2_PUBLIC void ms_turn_context_set_state(MSTurnContext *context, MSTurnContextState state);
MS2_PUBLIC const char * ms_turn_context_get_realm(const MSTurnContext *context);
MS2_PUBLIC void ms_turn_context_set_realm(MSTurnContext *context, const char *realm);
MS2_PUBLIC const char * ms_turn_context_get_nonce(const MSTurnContext *context);
MS2_PUBLIC void ms_turn_context_set_nonce(MSTurnContext *context, const char *nonce);
MS2_PUBLIC const char * ms_turn_context_get_username(const MSTurnContext *context);
MS2_PUBLIC void ms_turn_context_set_username(MSTurnContext *context, const char *username);
MS2_PUBLIC const char * ms_turn_context_get_password(const MSTurnContext *context);
MS2_PUBLIC void ms_turn_context_set_password(MSTurnContext *context, const char *password);
MS2_PUBLIC const char * ms_turn_context_get_ha1(const MSTurnContext *context);
MS2_PUBLIC void ms_turn_context_set_ha1(MSTurnContext *context, const char *ha1);
MS2_PUBLIC uint32_t ms_turn_context_get_lifetime(const MSTurnContext *context);
MS2_PUBLIC void ms_turn_context_set_lifetime(MSTurnContext *context, uint32_t lifetime);
MS2_PUBLIC uint16_t ms_turn_context_get_channel_number(const MSTurnContext *context);
MS2_PUBLIC void ms_turn_context_set_channel_number(MSTurnContext *context, uint16_t channel_number);
MS2_PUBLIC void ms_turn_context_set_allocated_relay_addr(MSTurnContext *context, MSStunAddress relay_addr);
MS2_PUBLIC void ms_turn_context_set_force_rtp_sending_via_relay(MSTurnContext *context, bool_t force);
MS2_PUBLIC bool_t ms_turn_context_peer_address_allowed(const MSTurnContext *context, const MSStunAddress *peer_address);
MS2_PUBLIC void ms_turn_context_allow_peer_address(MSTurnContext *context, const MSStunAddress *peer_address);
MS2_PUBLIC RtpTransport * ms_turn_context_create_endpoint(MSTurnContext *context);

#ifdef __cplusplus
}
#endif

#endif /* MS_STUN_H */
