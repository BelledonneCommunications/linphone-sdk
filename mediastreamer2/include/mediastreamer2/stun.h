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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef MS_STUN_H
#define MS_STUN_H


#include "mediastreamer2/mscommon.h"


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


#define MS_STUN_ATTR_MAPPED_ADDRESS      0x0001
#define MS_STUN_ATTR_RESPONSE_ADDRESS    0x0002 /* Deprecated, now reserved */
#define MS_STUN_ATTR_CHANGE_REQUEST      0x0003 /* Deprecated, now reserved */
#define MS_STUN_ATTR_SOURCE_ADDRESS      0x0004 /* Deprecated, now reserved */
#define MS_STUN_ATTR_CHANGED_ADDRESS     0x0005 /* Deprecated, now reserved */
#define MS_STUN_ATTR_USERNAME            0x0006
#define MS_STUN_ATTR_PASSWORD            0x0007 /* Deprecated, now reserved */
#define MS_STUN_ATTR_MESSAGE_INTEGRITY   0x0008
#define MS_STUN_ATTR_ERROR_CODE          0x0009
#define MS_STUN_ATTR_UNKNOWN_ATTRIBUTES  0x000A
#define MS_STUN_ATTR_REFLECTED_FROM      0x000B
#define MS_STUN_ATTR_REALM               0x0014
#define MS_STUN_ATTR_NONCE               0x0015
#define MS_STUN_ATTR_XOR_MAPPED_ADDRESS  0x0020

#define MS_STUN_ATTR_SOFTWARE            0x8022
#define MS_STUN_ATTR_ALTERNATE_SERVER    0x8023
#define MS_STUN_ATTR_FINGERPRINT         0x8028

#define MS_TURN_ATTR_CHANNEL_NUMBER      0x000C
#define MS_TURN_ATTR_LIFETIME            0x000D
#define MS_TURN_ATTR_BANDWIDTH           0x0010 /* Deprecated, now reserved */
#define MS_TURN_ATTR_XOR_PEER_ADDRESS    0x0012
#define MS_TURN_ATTR_DATA                0x0013
#define MS_TURN_ATTR_XOR_RELAYED_ADDRESS 0x0016
#define MS_TURN_ATTR_EVEN_PORT           0x0018
#define MS_TURN_ATTR_REQUESTED_TRANSPORT 0x0019
#define MS_TURN_ATTR_DONT_FRAGMENT       0x001A
#define MS_TURN_ATTR_TIMER_VAL           0x0021 /* Deprecated, now reserved */
#define MS_TURN_ATTR_RESERVATION_TOKEN   0x0022

#define MS_ICE_ATTR_PRIORITY             0x0024
#define MS_ICE_ATTR_USE_CANDIDATE        0x0025
#define MS_ICE_ATTR_ICE_CONTROLLED       0x8029
#define MS_ICE_ATTR_ICE_CONTROLLING      0x802A


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
	MSStunAddress4 ipv4;
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
	char *username;
	char *password;
	char *realm;
	char *message_integrity;
	char *software;
	MSStunErrorCode error_code;
	MSStunAddress mapped_address;
	MSStunAddress xor_mapped_address;
	uint32_t change_request;
	uint32_t fingerprint;
	uint32_t priority;
	uint64_t ice_controlling;
	uint64_t ice_controlled;
	bool_t include_username_attribute;
	bool_t has_error_code;
	bool_t has_message_integrity;
	bool_t has_dummy_message_integrity;
	bool_t has_fingerprint;
	bool_t has_mapped_address;
	bool_t has_xor_mapped_address;
	bool_t has_priority;
	bool_t has_use_candidate;
	bool_t has_ice_controlling;
	bool_t has_ice_controlled;
} MSStunMessage;


#ifdef __cplusplus
extern "C"
{
#endif

MS2_PUBLIC MSStunAddress4 ms_stun_hostname_to_stun_addr(const char *hostname, uint16_t default_port);
MS2_PUBLIC char * ms_stun_calculate_integrity_short_term(const char *buf, size_t bufsize, const char *key);
MS2_PUBLIC char * ms_stun_calculate_integrity_long_term(const char *buf, size_t bufsize, const char *realm, const char *username, const char *password);
MS2_PUBLIC uint32_t ms_stun_calculate_fingerprint(const char *buf, size_t bufsize);

MS2_PUBLIC MSStunMessage * ms_stun_message_create(uint16_t type, uint16_t method);
MS2_PUBLIC MSStunMessage * ms_stun_message_create_from_buffer_parsing(const char *buf, size_t bufsize);
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
MS2_PUBLIC uint16_t ms_stun_message_get_length(const MSStunMessage *msg);
MS2_PUBLIC UInt96 ms_stun_message_get_tr_id(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_tr_id(MSStunMessage *msg, UInt96 tr_id);
MS2_PUBLIC void ms_stun_message_set_random_tr_id(MSStunMessage *msg);
MS2_PUBLIC const char * ms_stun_message_get_username(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_username(MSStunMessage *msg, const char *username);
MS2_PUBLIC void ms_stun_message_include_username_attribute(MSStunMessage *msg, bool_t include);
MS2_PUBLIC const char * ms_stun_message_get_password(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_password(MSStunMessage *msg, const char *password);
MS2_PUBLIC const char * ms_stun_message_get_realm(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_realm(MSStunMessage *msg, const char *realm);
MS2_PUBLIC const char * ms_stun_message_get_software(const MSStunMessage *msg);
MS2_PUBLIC void ms_stun_message_set_software(MSStunMessage *msg, const char *software);
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

#ifdef __cplusplus
}
#endif

#endif /* MS_STUN_H */
