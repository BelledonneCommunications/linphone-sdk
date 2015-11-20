/*
	belle-sdp - SIP (RFC4566) library.
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

#ifndef BELLE_SDP_H_
#define BELLE_SDP_H_

#include "belle-sip/defs.h"
#include "belle-sip/list.h"

#define BELLE_SDP_CAST(obj,t) BELLE_SIP_CAST(obj,t)

BELLE_SIP_BEGIN_DECLS

/***************************************************************************************
 * Attribute
 *
 **************************************************************************************/
typedef struct _belle_sdp_attribute belle_sdp_attribute_t;
BELLESIP_EXPORT belle_sdp_attribute_t* belle_sdp_attribute_new(void);
BELLESIP_EXPORT belle_sdp_attribute_t* belle_sdp_attribute_parse (const char* attribute);
BELLESIP_EXPORT belle_sdp_attribute_t* belle_sdp_attribute_create (const char* name,const char* value);
BELLESIP_EXPORT const char* belle_sdp_attribute_get_name(const belle_sdp_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_attribute_set_name(belle_sdp_attribute_t* attribute, const char* name);
BELLESIP_EXPORT const char* belle_sdp_attribute_get_value(belle_sdp_attribute_t* attribute);
BELLESIP_EXPORT unsigned int belle_sdp_attribute_has_value(belle_sdp_attribute_t* attribute);
#define BELLE_SDP_ATTRIBUTE(t) BELLE_SDP_CAST(t,belle_sdp_attribute_t)
#define belle_sdp_attribute_init(obj)		/*nothing*/
/***************************************************************************************
 * RAW Attribute
 *
 **************************************************************************************/
typedef struct _belle_sdp_raw_attribute belle_sdp_raw_attribute_t;
BELLESIP_EXPORT belle_sdp_raw_attribute_t* belle_sdp_raw_attribute_new(void);
BELLESIP_EXPORT belle_sdp_raw_attribute_t* belle_sdp_raw_attribute_parse(const char* attribute);
BELLESIP_EXPORT belle_sdp_raw_attribute_t* belle_sdp_raw_attribute_create(const char* name, const char* value);
BELLESIP_EXPORT void belle_sdp_raw_attribute_set_value(belle_sdp_raw_attribute_t* attribute, const char* value);
#define BELLE_SDP_RAW_ATTRIBUTE(t) BELLE_SDP_CAST(t,belle_sdp_raw_attribute_t)
/***************************************************************************************
 * RTCP-FB Attribute
 *
 **************************************************************************************/
typedef enum _belle_sdp_rtcp_fb_val_type {
	BELLE_SDP_RTCP_FB_ACK,
	BELLE_SDP_RTCP_FB_NACK,
	BELLE_SDP_RTCP_FB_TRR_INT,
	BELLE_SDP_RTCP_FB_CCM
} belle_sdp_rtcp_fb_val_type_t;
typedef enum _belle_sdp_rtcp_fb_val_param {
	BELLE_SDP_RTCP_FB_NONE,
	BELLE_SDP_RTCP_FB_PLI,
	BELLE_SDP_RTCP_FB_SLI,
	BELLE_SDP_RTCP_FB_RPSI,
	BELLE_SDP_RTCP_FB_APP,
	BELLE_SDP_RTCP_FB_FIR,
	BELLE_SDP_RTCP_FB_TMMBR
} belle_sdp_rtcp_fb_val_param_t;
typedef struct _belle_sdp_rtcp_fb_attribute belle_sdp_rtcp_fb_attribute_t;
BELLESIP_EXPORT belle_sdp_rtcp_fb_attribute_t* belle_sdp_rtcp_fb_attribute_new(void);
BELLESIP_EXPORT belle_sdp_rtcp_fb_attribute_t* belle_sdp_rtcp_fb_attribute_parse(const char* attribute);
BELLESIP_EXPORT belle_sdp_rtcp_fb_attribute_t* belle_sdp_rtcp_fb_attribute_create(void);
BELLESIP_EXPORT int8_t belle_sdp_rtcp_fb_attribute_get_id(const belle_sdp_rtcp_fb_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_rtcp_fb_attribute_set_id(belle_sdp_rtcp_fb_attribute_t* attribute, int8_t id);
BELLESIP_EXPORT belle_sdp_rtcp_fb_val_type_t belle_sdp_rtcp_fb_attribute_get_type(const belle_sdp_rtcp_fb_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_rtcp_fb_attribute_set_type(belle_sdp_rtcp_fb_attribute_t* attribute, belle_sdp_rtcp_fb_val_type_t type);
BELLESIP_EXPORT belle_sdp_rtcp_fb_val_param_t belle_sdp_rtcp_fb_attribute_get_param(const belle_sdp_rtcp_fb_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_rtcp_fb_attribute_set_param(belle_sdp_rtcp_fb_attribute_t* attribute, belle_sdp_rtcp_fb_val_param_t param);
BELLESIP_EXPORT uint16_t belle_sdp_rtcp_fb_attribute_get_trr_int(const belle_sdp_rtcp_fb_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_rtcp_fb_attribute_set_trr_int(belle_sdp_rtcp_fb_attribute_t* attribute, uint16_t milliseconds);
BELLESIP_EXPORT uint32_t belle_sdp_rtcp_fb_attribute_get_smaxpr(const belle_sdp_rtcp_fb_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_rtcp_fb_attribute_set_smaxpr(belle_sdp_rtcp_fb_attribute_t* attribute, uint32_t smaxpr);
#define BELLE_SDP_RTCP_FB_ATTRIBUTE(t) BELLE_SDP_CAST(t,belle_sdp_rtcp_fb_attribute_t)
/***************************************************************************************
 * RTCP-XR Attribute
 *
 **************************************************************************************/
typedef struct _belle_sdp_rtcp_xr_attribute belle_sdp_rtcp_xr_attribute_t;
BELLESIP_EXPORT belle_sdp_rtcp_xr_attribute_t* belle_sdp_rtcp_xr_attribute_new(void);
BELLESIP_EXPORT belle_sdp_rtcp_xr_attribute_t* belle_sdp_rtcp_xr_attribute_parse(const char* attribute);
BELLESIP_EXPORT belle_sdp_rtcp_xr_attribute_t* belle_sdp_rtcp_xr_attribute_create(void);
BELLESIP_EXPORT const char* belle_sdp_rtcp_xr_attribute_get_rcvr_rtt_mode(const belle_sdp_rtcp_xr_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_rtcp_xr_attribute_set_rcvr_rtt_mode(belle_sdp_rtcp_xr_attribute_t* attribute, const char *mode);
BELLESIP_EXPORT int belle_sdp_rtcp_xr_attribute_get_rcvr_rtt_max_size(const belle_sdp_rtcp_xr_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_rtcp_xr_attribute_set_rcvr_rtt_max_size(belle_sdp_rtcp_xr_attribute_t* attribute, int max_size);
BELLESIP_EXPORT unsigned int belle_sdp_rtcp_xr_attribute_has_stat_summary(const belle_sdp_rtcp_xr_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_rtcp_xr_attribute_set_stat_summary(belle_sdp_rtcp_xr_attribute_t* attribute, unsigned int enable);
BELLESIP_EXPORT const belle_sip_list_t* belle_sdp_rtcp_xr_attribute_get_stat_summary_flags(const belle_sdp_rtcp_xr_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_rtcp_xr_attribute_add_stat_summary_flag(belle_sdp_rtcp_xr_attribute_t* attribute, const char* flag);
BELLESIP_EXPORT unsigned int belle_sdp_rtcp_xr_attribute_has_voip_metrics(const belle_sdp_rtcp_xr_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_rtcp_xr_attribute_set_voip_metrics(belle_sdp_rtcp_xr_attribute_t* attribute, unsigned int enable);
#define BELLE_SDP_RTCP_XR_ATTRIBUTE(t) BELLE_SDP_CAST(t,belle_sdp_rtcp_xr_attribute_t)
/***************************************************************************************
 * Bandwidth
 *
 **************************************************************************************/
typedef struct _belle_sdp_bandwidth belle_sdp_bandwidth_t;
BELLESIP_EXPORT belle_sdp_bandwidth_t* belle_sdp_bandwidth_new(void);
BELLESIP_EXPORT belle_sdp_bandwidth_t* belle_sdp_bandwidth_parse (const char* bandwidth);
BELLESIP_EXPORT int belle_sdp_bandwidth_get_value(const belle_sdp_bandwidth_t* attribute);
BELLESIP_EXPORT const char* belle_sdp_bandwidth_get_type(const belle_sdp_bandwidth_t* attribute);
BELLESIP_EXPORT void belle_sdp_bandwidth_set_value(belle_sdp_bandwidth_t* attribute, int value);
BELLESIP_EXPORT void belle_sdp_bandwidth_set_type(belle_sdp_bandwidth_t* attribute, const char* type);
#define BELLE_SDP_BANDWIDTH(t) BELLE_SDP_CAST(t,belle_sdp_bandwidth_t)
/***************************************************************************************
 * Connection
 *
 **************************************************************************************/
typedef struct _belle_sdp_connection belle_sdp_connection_t;
BELLESIP_EXPORT belle_sdp_connection_t* belle_sdp_connection_new(void);
BELLESIP_EXPORT belle_sdp_connection_t* belle_sdp_connection_create(const char* net_type, const char* addr_type, const char* addr);
BELLESIP_EXPORT belle_sdp_connection_t* belle_sdp_connection_parse (const char* connection);
BELLESIP_EXPORT const char* belle_sdp_connection_get_address(const belle_sdp_connection_t* connection);
BELLESIP_EXPORT const char* belle_sdp_connection_get_address_type(const belle_sdp_connection_t* connection);
BELLESIP_EXPORT const char* belle_sdp_connection_get_network_type(const belle_sdp_connection_t* connection);
BELLESIP_EXPORT int belle_sdp_connection_get_ttl(const belle_sdp_connection_t* connection);
BELLESIP_EXPORT int belle_sdp_connection_get_range(const belle_sdp_connection_t* connection);
BELLESIP_EXPORT void belle_sdp_connection_set_address(belle_sdp_connection_t* connection, const char* addr);
BELLESIP_EXPORT void belle_sdp_connection_set_address_type(belle_sdp_connection_t* connection, const char* type);
BELLESIP_EXPORT void belle_sdp_connection_set_network_type(belle_sdp_connection_t* connection, const char* type);
BELLESIP_EXPORT void belle_sdp_connection_set_ttl(belle_sdp_connection_t* connection,int ttl);
BELLESIP_EXPORT void belle_sdp_connection_set_range(belle_sdp_connection_t* connection,int range);
#define BELLE_SDP_CONNECTION(t) BELLE_SDP_CAST(t,belle_sdp_connection_t)
/***************************************************************************************
 * Email
 *
 **************************************************************************************/
typedef struct _belle_sdp_email belle_sdp_email_t;
BELLESIP_EXPORT belle_sdp_email_t* belle_sdp_email_new(void);
BELLESIP_EXPORT belle_sdp_email_t* belle_sdp_email_parse (const char* email);
BELLESIP_EXPORT const char* belle_sdp_email_get_value(const belle_sdp_email_t* email);
BELLESIP_EXPORT void belle_sdp_email_set_value(belle_sdp_email_t* email, const char* value);
#define BELLE_SDP_EMAIL(t) BELLE_SDP_CAST(t,belle_sdp_email_t)
/***************************************************************************************
 * Info
 *
 **************************************************************************************/
typedef struct _belle_sdp_info belle_sdp_info_t;
BELLESIP_EXPORT belle_sdp_info_t* belle_sdp_info_new(void);
BELLESIP_EXPORT belle_sdp_info_t* belle_sdp_info_parse (const char* info);
BELLESIP_EXPORT const char* belle_sdp_info_get_value(const belle_sdp_info_t* info);
BELLESIP_EXPORT void belle_sdp_info_set_value(belle_sdp_info_t* info, const char* value);
#define BELLE_SDP_INFO(t) BELLE_SDP_CAST(t,belle_sdp_info_t)
/***************************************************************************************
 * Key
 *
 **************************************************************************************/
//typedef struct _belle_sdp_key belle_sdp_key_t;
//belle_sdp_key_t* belle_sdp_key_new(void);
//belle_sdp_key_t* belle_sdp_key_parse (const char* key);
//const char* belle_sdp_key_get_key(const belle_sdp_key_t* key);
//const char* belle_sdp_key_get_method(const belle_sdp_key_t* key);
//unsigned int belle_sdp_key_as_key(const belle_sdp_key_t* key);
//void belle_sdp_key_set_key(belle_sdp_key_t* key, const char* keyvalue);
//void belle_sdp_key_set_method(belle_sdp_key_t* key, const char* method);
//#define BELLE_SDP_KEY(t) BELLE_SDP_CAST(t,belle_sdp_key_t);
/***************************************************************************************
 * Media
 *
 **************************************************************************************/
typedef struct _belle_sdp_media belle_sdp_media_t;
BELLESIP_EXPORT belle_sdp_media_t* belle_sdp_media_new(void);
BELLESIP_EXPORT belle_sdp_media_t* belle_sdp_media_parse (const char* media);
BELLESIP_EXPORT belle_sdp_media_t* belle_sdp_media_create(const char* media_type
                         ,int media_port
                         ,int port_count
                         ,const char* protocol
                         ,belle_sip_list_t* static_media_formats);
BELLESIP_EXPORT belle_sip_list_t*	belle_sdp_media_get_media_formats(const belle_sdp_media_t* media);
BELLESIP_EXPORT const char*	belle_sdp_media_get_raw_fmt(const belle_sdp_media_t* media);
BELLESIP_EXPORT int	belle_sdp_media_get_media_port(const belle_sdp_media_t* media);
BELLESIP_EXPORT const char* belle_sdp_media_get_media_type(const belle_sdp_media_t* media);
BELLESIP_EXPORT int	belle_sdp_media_get_port_count(const belle_sdp_media_t* media);
BELLESIP_EXPORT const char* belle_sdp_media_get_protocol(const belle_sdp_media_t* media);
BELLESIP_EXPORT void belle_sdp_media_set_media_formats(belle_sdp_media_t* media, belle_sip_list_t* mediaFormats);
BELLESIP_EXPORT void belle_sdp_media_set_raw_fmt(belle_sdp_media_t* media, const char* fmt);
BELLESIP_EXPORT void belle_sdp_media_set_media_port(belle_sdp_media_t* media, int port);
BELLESIP_EXPORT void belle_sdp_media_set_media_type(belle_sdp_media_t* media, const char* mediaType);
BELLESIP_EXPORT void belle_sdp_media_set_port_count(belle_sdp_media_t* media, int port_count);
BELLESIP_EXPORT void belle_sdp_media_set_protocol(belle_sdp_media_t* media, const char* protocole);
#define BELLE_SDP_MEDIA(t) BELLE_SDP_CAST(t,belle_sdp_media_t)

/***************************************************************************************
 * mime_parameter
 *
 **************************************************************************************/
typedef struct _belle_sdp_mime_parameter belle_sdp_mime_parameter_t;
BELLESIP_EXPORT belle_sdp_mime_parameter_t* belle_sdp_mime_parameter_new(void);
BELLESIP_EXPORT belle_sdp_mime_parameter_t* belle_sdp_mime_parameter_create(const char* type, int media_format, int rate,int channel_count);
BELLESIP_EXPORT int belle_sdp_mime_parameter_get_rate(const belle_sdp_mime_parameter_t* mime_parameter);
BELLESIP_EXPORT void belle_sdp_mime_parameter_set_rate(belle_sdp_mime_parameter_t* mime_parameter,int rate);
BELLESIP_EXPORT int belle_sdp_mime_parameter_get_channel_count(const belle_sdp_mime_parameter_t* mime_parameter);
BELLESIP_EXPORT void belle_sdp_mime_parameter_set_channel_count(belle_sdp_mime_parameter_t* mime_parameter,int count);
BELLESIP_EXPORT int belle_sdp_mime_parameter_get_ptime(const belle_sdp_mime_parameter_t* mime_parameter);
BELLESIP_EXPORT void belle_sdp_mime_parameter_set_ptime(belle_sdp_mime_parameter_t* mime_parameter,int ptime);
BELLESIP_EXPORT int belle_sdp_mime_parameter_get_max_ptime(const belle_sdp_mime_parameter_t* mime_parameter);
BELLESIP_EXPORT void belle_sdp_mime_parameter_set_max_ptime(belle_sdp_mime_parameter_t* mime_parameter,int max_ptime);
BELLESIP_EXPORT const char* belle_sdp_mime_parameter_get_type(const belle_sdp_mime_parameter_t* mime_parameter);
BELLESIP_EXPORT void belle_sdp_mime_parameter_set_type(belle_sdp_mime_parameter_t* mime_parameter,const char* type);
BELLESIP_EXPORT int belle_sdp_mime_parameter_get_media_format(const belle_sdp_mime_parameter_t* mime_parameter);
BELLESIP_EXPORT void belle_sdp_mime_parameter_set_media_format(belle_sdp_mime_parameter_t* mime_parameter,int format);
BELLESIP_EXPORT const char* belle_sdp_mime_parameter_get_parameters(const belle_sdp_mime_parameter_t* mime_parameter);
BELLESIP_EXPORT void belle_sdp_mime_parameter_set_parameters(belle_sdp_mime_parameter_t* mime_parameter,const char* parameters);
#define BELLE_SDP_MIME_PARAMETER(t) BELLE_SDP_CAST(t,belle_sdp_mime_parameter_t)

/***************************************************************************************
 * Media Description
 *
 **************************************************************************************/
typedef struct _belle_sdp_media_description belle_sdp_media_description_t;
BELLESIP_EXPORT belle_sdp_media_description_t* belle_sdp_media_description_new(void);
BELLESIP_EXPORT belle_sdp_media_description_t* belle_sdp_media_description_parse (const char* media_description);
BELLESIP_EXPORT belle_sdp_media_description_t* belle_sdp_media_description_create(const char* media_type
                         	 	 	 	 	 	 	 	 	 	 ,int media_port
                         	 	 	 	 	 	 	 	 	 	 ,int port_count
                         	 	 	 	 	 	 	 	 	 	 ,const char* protocol
                         	 	 	 	 	 	 	 	 	 	 ,belle_sip_list_t* static_media_formats);
BELLESIP_EXPORT void belle_sdp_media_description_add_dynamic_payloads(belle_sdp_media_description_t* media_description, belle_sip_list_t* payloadNames, belle_sip_list_t* payloadValues);
BELLESIP_EXPORT const char*	belle_sdp_media_description_get_attribute_value(const belle_sdp_media_description_t* media_description, const char* name);
BELLESIP_EXPORT belle_sdp_attribute_t*	belle_sdp_media_description_get_attribute(const belle_sdp_media_description_t* media_description, const char* name);
BELLESIP_EXPORT belle_sip_list_t* belle_sdp_media_description_get_attributes(const belle_sdp_media_description_t* media_description);
BELLESIP_EXPORT int	belle_sdp_media_description_get_bandwidth(const belle_sdp_media_description_t* media_description, const char* name);
BELLESIP_EXPORT belle_sip_list_t* belle_sdp_media_description_get_bandwidths(const belle_sdp_media_description_t* media_description);
BELLESIP_EXPORT belle_sdp_connection_t*	belle_sdp_media_description_get_connection(const belle_sdp_media_description_t* media_description);
BELLESIP_EXPORT belle_sdp_info_t* belle_sdp_media_description_get_info(const belle_sdp_media_description_t* media_description);
/*belle_sdp_key_t*  belle_sdp_media_description_get_key(const belle_sdp_media_description_t* media_description);*/
BELLESIP_EXPORT belle_sdp_media_t* belle_sdp_media_description_get_media(const belle_sdp_media_description_t* media_description);
BELLESIP_EXPORT belle_sip_list_t* belle_sdp_media_description_build_mime_parameters(const belle_sdp_media_description_t* media_description);
/*belle_sip_list_t* belle_sdp_media_description_get_mime_types(const belle_sdp_media_description_t* media_description);*/
BELLESIP_EXPORT void belle_sdp_media_description_remove_attribute(belle_sdp_media_description_t* media_description,const char* attribute);
BELLESIP_EXPORT void belle_sdp_media_description_remove_bandwidth(belle_sdp_media_description_t* media_description,const char* bandwidth);
BELLESIP_EXPORT void belle_sdp_media_description_set_attribute_value(belle_sdp_media_description_t* media_description, const char* name, const char* value);
BELLESIP_EXPORT void belle_sdp_media_description_add_attribute(belle_sdp_media_description_t* media_description, const belle_sdp_attribute_t* attr);
BELLESIP_EXPORT void belle_sdp_media_description_set_attributes(belle_sdp_media_description_t* media_description, belle_sip_list_t* Attributes);
BELLESIP_EXPORT void belle_sdp_media_description_set_bandwidth(belle_sdp_media_description_t* media_description, const char* name, int value);
BELLESIP_EXPORT void belle_sdp_media_description_add_bandwidth(belle_sdp_media_description_t* media_description, const belle_sdp_bandwidth_t* bandwidth);
BELLESIP_EXPORT void belle_sdp_media_description_set_bandwidths(belle_sdp_media_description_t* media_description, belle_sip_list_t* bandwidths);
BELLESIP_EXPORT void belle_sdp_media_description_set_connection(belle_sdp_media_description_t* media_description, belle_sdp_connection_t* conn);
BELLESIP_EXPORT void belle_sdp_media_description_set_info(belle_sdp_media_description_t* media_description,belle_sdp_info_t* i);
/*void belle_sdp_media_description_set_key(belle_sdp_media_description_t* media_description,belle_sdp_key_t* key);*/
BELLESIP_EXPORT void belle_sdp_media_description_set_media(belle_sdp_media_description_t* media_description, belle_sdp_media_t* media);
BELLESIP_EXPORT void belle_sdp_media_description_append_values_from_mime_parameter(belle_sdp_media_description_t* media_description, const belle_sdp_mime_parameter_t* mime_parameter);
#define BELLE_SDP_MEDIA_DESCRIPTION(t) BELLE_SDP_CAST(t,belle_sdp_media_description_t)

/***************************************************************************************
 * Origin
 *
 **************************************************************************************/
typedef struct _belle_sdp_origin belle_sdp_origin_t;
BELLESIP_EXPORT belle_sdp_origin_t* belle_sdp_origin_new(void);
BELLESIP_EXPORT belle_sdp_origin_t* belle_sdp_origin_parse (const char* origin);
BELLESIP_EXPORT belle_sdp_origin_t* belle_sdp_origin_create(const char* user_name
											, unsigned int session_id
											, unsigned int session_version
											, const char* network_type
											, const char* addr_type
											, const char* address);
BELLESIP_EXPORT const char* belle_sdp_origin_get_address(const belle_sdp_origin_t* origin);
BELLESIP_EXPORT const char* belle_sdp_origin_get_address_type(const belle_sdp_origin_t* origin);
BELLESIP_EXPORT const char* belle_sdp_origin_get_network_type(const belle_sdp_origin_t* origin);
BELLESIP_EXPORT unsigned int belle_sdp_origin_get_session_id(const belle_sdp_origin_t* origin);
BELLESIP_EXPORT unsigned int belle_sdp_origin_get_session_version(const belle_sdp_origin_t* origin);
BELLESIP_EXPORT const char* belle_sdp_origin_get_username(const belle_sdp_origin_t* origin);
BELLESIP_EXPORT void belle_sdp_origin_set_address(belle_sdp_origin_t* origin, const char* address);
BELLESIP_EXPORT void belle_sdp_origin_set_address_type(belle_sdp_origin_t* origin, const char* address);
BELLESIP_EXPORT void belle_sdp_origin_set_network_type(belle_sdp_origin_t* origin, const char* network_type);
BELLESIP_EXPORT void belle_sdp_origin_set_session_id(belle_sdp_origin_t* origin, unsigned int session_id);
BELLESIP_EXPORT void belle_sdp_origin_set_session_version(belle_sdp_origin_t* origin, unsigned int version);
BELLESIP_EXPORT void belle_sdp_origin_set_username(belle_sdp_origin_t* origin, const char* username);
#define BELLE_SDP_ORIGIN(t) BELLE_SDP_CAST(t,belle_sdp_origin_t)
/***************************************************************************************
 * Phone
 *
 **************************************************************************************/
typedef struct _belle_sdp_phone belle_sdp_phone_t;
BELLESIP_EXPORT belle_sdp_phone_t* belle_sdp_phone_new(void);
BELLESIP_EXPORT belle_sdp_phone_t* belle_sdp_phone_parse (const char* phone);
BELLESIP_EXPORT const char* belle_sdp_phone_get_value(const belle_sdp_phone_t* phone);
BELLESIP_EXPORT void belle_sdp_phone_set_value(belle_sdp_phone_t* phone, const char* value);
#define BELLE_SDP_PHONE(t) BELLE_SDP_CAST(t,belle_sdp_phone_t)
/***************************************************************************************
 * Repeat time
 *
 **************************************************************************************/
typedef struct _belle_sdp_repeate_time belle_sdp_repeate_time_t;
BELLESIP_EXPORT belle_sdp_repeate_time_t* belle_sdp_repeate_time_new(void);
BELLESIP_EXPORT belle_sdp_repeate_time_t* belle_sdp_repeate_time_parse (const char* repeate_time);
BELLESIP_EXPORT const char* belle_sdp_repeate_time_get_value(const belle_sdp_repeate_time_t* repeate_time);
BELLESIP_EXPORT void belle_sdp_repeate_time_set_value(belle_sdp_repeate_time_t* repeate_time, const char* value);
#define BELLE_SDP_REPEATE_TIME(t) BELLE_SDP_CAST(t,belle_sdp_repeate_time_t)
/***************************************************************************************
 * Session Name
 *
 **************************************************************************************/
typedef struct _belle_sdp_session_name belle_sdp_session_name_t;
BELLESIP_EXPORT belle_sdp_session_name_t* belle_sdp_session_name_new(void);
BELLESIP_EXPORT belle_sdp_session_name_t* belle_sdp_session_name_create (const char* name);
BELLESIP_EXPORT const char* belle_sdp_session_name_get_value(const belle_sdp_session_name_t* session_name);
BELLESIP_EXPORT void belle_sdp_session_name_set_value(belle_sdp_session_name_t* session_name, const char* value);
#define BELLE_SDP_SESSION_NAME(t) BELLE_SDP_CAST(t,belle_sdp_session_name_t)
/***************************************************************************************
 * Time
 *
 **************************************************************************************/
typedef struct _belle_sdp_time belle_sdp_time_t;
BELLESIP_EXPORT belle_sdp_time_t* belle_sdp_time_new(void);
BELLESIP_EXPORT belle_sdp_time_t* belle_sdp_time_parse (const char* time);

BELLESIP_EXPORT int belle_sdp_time_get_start(const belle_sdp_time_t* time);
BELLESIP_EXPORT int belle_sdp_time_get_stop(const belle_sdp_time_t* time);
BELLESIP_EXPORT void belle_sdp_time_set_start(belle_sdp_time_t* time, int value);
BELLESIP_EXPORT void belle_sdp_time_set_stop(belle_sdp_time_t* time, int value);
#define BELLE_SDP_TIME(t) BELLE_SDP_CAST(t,belle_sdp_time_t)
/***************************************************************************************
 * Time description
 *
 **************************************************************************************/
typedef struct _belle_sdp_time_description belle_sdp_time_description_t;
BELLESIP_EXPORT belle_sdp_time_description_t* belle_sdp_time_description_new(void);
BELLESIP_EXPORT belle_sdp_time_description_t* belle_sdp_time_description_parse (const char* time_description);
BELLESIP_EXPORT belle_sdp_time_description_t* belle_sdp_time_description_create (int start,int stop);

BELLESIP_EXPORT belle_sip_list_t* belle_sdp_time_description_get_repeate_times(const belle_sdp_time_description_t* time_description);
BELLESIP_EXPORT belle_sdp_time_t* belle_sdp_time_description_get_time(const belle_sdp_time_description_t* time_description);
BELLESIP_EXPORT void belle_sdp_time_description_set_repeate_times(belle_sdp_time_description_t* time_description, belle_sip_list_t* times);
BELLESIP_EXPORT void belle_sdp_time_description_set_time(belle_sdp_time_description_t* time_description, belle_sdp_time_t* times);
#define BELLE_SDP_TIME_DESCRIPTION(t) BELLE_SDP_CAST(t,belle_sdp_time_description_t)
/***************************************************************************************
 * URI
 *
 **************************************************************************************/
typedef struct _belle_sdp_uri belle_sdp_uri_t;
BELLESIP_EXPORT belle_sdp_uri_t* belle_sdp_uri_new(void);
BELLESIP_EXPORT belle_sdp_uri_t* belle_sdp_uri_parse (const char* uri);
BELLESIP_EXPORT const char* belle_sdp_uri_get_value(const belle_sdp_uri_t* uri);
BELLESIP_EXPORT void belle_sdp_uri_set_value(belle_sdp_uri_t* uri, const char* value);
#define BELLE_SDP_URI(t) BELLE_SDP_CAST(t,belle_sdp_uri_t)
/***************************************************************************************
 * Version
 *
 **************************************************************************************/
typedef struct _belle_sdp_version belle_sdp_version_t;
belle_sdp_version_t* belle_sdp_version_new(void);
BELLESIP_EXPORT belle_sdp_version_t* belle_sdp_version_create(int version);
BELLESIP_EXPORT int belle_sdp_version_get_version(const belle_sdp_version_t* version);
BELLESIP_EXPORT void belle_sdp_version_set_version(belle_sdp_version_t* version, int value);
#define BELLE_SDP_VERSION(t) BELLE_SDP_CAST(t,belle_sdp_version_t)

/***************************************************************************************
 * Session Description
 *
 **************************************************************************************/
typedef struct _belle_sdp_session_description belle_sdp_session_description_t;
BELLESIP_EXPORT belle_sdp_session_description_t* belle_sdp_session_description_new(void);
BELLESIP_EXPORT belle_sdp_session_description_t* belle_sdp_session_description_parse (const char* session_description);

BELLESIP_EXPORT belle_sip_list_t * belle_sdp_session_description_get_attributes(const belle_sdp_session_description_t *session_description);
BELLESIP_EXPORT const char*	belle_sdp_session_description_get_attribute_value(const belle_sdp_session_description_t* session_description, const char* name);
BELLESIP_EXPORT const belle_sdp_attribute_t*	belle_sdp_session_description_get_attribute(const belle_sdp_session_description_t* session_description, const char* name);
BELLESIP_EXPORT int	belle_sdp_session_description_get_bandwidth(const belle_sdp_session_description_t* session_description, const char* name);
BELLESIP_EXPORT belle_sip_list_t*	belle_sdp_session_description_get_bandwidths(const belle_sdp_session_description_t* session_description);
BELLESIP_EXPORT belle_sdp_connection_t*	belle_sdp_session_description_get_connection(const belle_sdp_session_description_t* session_description);
BELLESIP_EXPORT belle_sip_list_t* belle_sdp_session_description_get_emails(const belle_sdp_session_description_t* session_description);
BELLESIP_EXPORT belle_sdp_info_t* belle_sdp_session_description_get_info(const belle_sdp_session_description_t* session_description);
/*belle_sdp_key_t*	belle_sdp_session_description_get_key(const belle_sdp_session_description_t* session_description);*/
BELLESIP_EXPORT belle_sip_list_t* belle_sdp_session_description_get_media_descriptions(const belle_sdp_session_description_t* session_description);
BELLESIP_EXPORT belle_sdp_origin_t*	belle_sdp_session_description_get_origin(const belle_sdp_session_description_t* session_description);
BELLESIP_EXPORT belle_sip_list_t* belle_sdp_session_description_get_phones(const belle_sdp_session_description_t* session_description);
BELLESIP_EXPORT belle_sdp_session_name_t* belle_sdp_session_description_get_session_name(const belle_sdp_session_description_t* session_description);
BELLESIP_EXPORT belle_sip_list_t* belle_sdp_session_description_get_time_descriptions(const belle_sdp_session_description_t* session_description);
BELLESIP_EXPORT belle_sdp_uri_t* belle_sdp_session_description_get_uri(const belle_sdp_session_description_t* session_description);
BELLESIP_EXPORT belle_sdp_version_t*	belle_sdp_session_description_get_version(const belle_sdp_session_description_t* session_description);
BELLESIP_EXPORT belle_sdp_uri_t* belle_sdp_session_description_get_zone_adjustments(const belle_sdp_session_description_t* session_description);
BELLESIP_EXPORT void belle_sdp_session_description_remove_attribute(belle_sdp_session_description_t* session_description, const char* name);
BELLESIP_EXPORT void belle_sdp_session_description_remove_bandwidth(belle_sdp_session_description_t* session_description, const char* name);
BELLESIP_EXPORT void belle_sdp_session_description_set_attribute_value(belle_sdp_session_description_t* session_description, const char* name, const char* value);
BELLESIP_EXPORT void belle_sdp_session_description_add_attribute(belle_sdp_session_description_t* session_description, const belle_sdp_attribute_t* attribute);
BELLESIP_EXPORT void belle_sdp_session_description_set_attributes(belle_sdp_session_description_t* session_description, belle_sip_list_t* Attributes);
BELLESIP_EXPORT void belle_sdp_session_description_set_bandwidth(belle_sdp_session_description_t* session_description, const char* name, int value);
BELLESIP_EXPORT void belle_sdp_session_description_set_bandwidths(belle_sdp_session_description_t* session_description, belle_sip_list_t* bandwidths);
BELLESIP_EXPORT void belle_sdp_session_description_add_bandwidth(belle_sdp_session_description_t* session_description, const belle_sdp_bandwidth_t* bandwidth);
BELLESIP_EXPORT void belle_sdp_session_description_set_connection(belle_sdp_session_description_t* session_description, belle_sdp_connection_t* conn);
BELLESIP_EXPORT void belle_sdp_session_description_set_emails(belle_sdp_session_description_t* session_description, belle_sip_list_t* emails);
BELLESIP_EXPORT void belle_sdp_session_description_set_info(belle_sdp_session_description_t* session_description, belle_sdp_info_t* i);
/*void belle_sdp_session_description_set_key(belle_sdp_session_description_t* session_description, belle_sdp_key_t* key);*/
BELLESIP_EXPORT void belle_sdp_session_description_set_media_descriptions(belle_sdp_session_description_t* session_description, belle_sip_list_t* mediaDescriptions);
BELLESIP_EXPORT void belle_sdp_session_description_add_media_description(belle_sdp_session_description_t* session_description, belle_sdp_media_description_t* media_description);
BELLESIP_EXPORT void belle_sdp_session_description_set_origin(belle_sdp_session_description_t* session_description, belle_sdp_origin_t* origin);
BELLESIP_EXPORT void belle_sdp_session_description_set_phones(belle_sdp_session_description_t* session_description, belle_sip_list_t* phones);
BELLESIP_EXPORT void belle_sdp_session_description_set_session_name(belle_sdp_session_description_t* session_description, belle_sdp_session_name_t* sessionName);
BELLESIP_EXPORT void belle_sdp_session_description_set_time_descriptions(belle_sdp_session_description_t* session_description, belle_sip_list_t* times);
BELLESIP_EXPORT void belle_sdp_session_description_set_time_description(belle_sdp_session_description_t* session_description, belle_sdp_time_description_t* time_desc);
BELLESIP_EXPORT void belle_sdp_session_description_set_uri(belle_sdp_session_description_t* session_description, belle_sdp_uri_t* uri);
BELLESIP_EXPORT void belle_sdp_session_description_set_version(belle_sdp_session_description_t* session_description, belle_sdp_version_t* v);
BELLESIP_EXPORT void belle_sdp_session_description_set_zone_adjustments(belle_sdp_session_description_t* session_description, belle_sdp_uri_t* zoneAdjustments);
#define BELLE_SDP_SESSION_DESCRIPTION(t) BELLE_SDP_CAST(t,belle_sdp_session_description_t)

BELLE_SIP_END_DECLS
#endif /* BELLE_SDP_H_ */
