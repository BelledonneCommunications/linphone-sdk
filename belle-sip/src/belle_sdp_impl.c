/*
	belle-sdp - SIP (RFC4566) library.
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
#include "belle-sip/belle-sip.h"
#include "belle_sdpParser.h"
#include "belle_sdpLexer.h"
#include "belle_sip_internal.h"
/***************************************************************************************
 * Attribute
 *
 **************************************************************************************/
struct _belle_sdp_attribute {
	belle_sip_object_t base;
	const char* name;
	const char* value;
};
void belle_sdp_attribute_destroy(belle_sdp_attribute_t* attribute) {
	if (attribute->name) belle_sip_free((void*)attribute->name);
	if (attribute->value) belle_sip_free((void*)attribute->value);
}

void belle_sdp_attribute_clone(belle_sdp_attribute_t *attribute, const belle_sdp_attribute_t *orig){
	CLONE_STRING(belle_sdp_attribute,name,attribute,orig)
	CLONE_STRING(belle_sdp_attribute,value,attribute,orig)
}

belle_sip_error_code belle_sdp_attribute_marshal(belle_sdp_attribute_t* attribute, char* buff, size_t buff_size, size_t *offset) {
	belle_sip_error_code error=belle_sip_snprintf(buff,buff_size,offset,"a=%s",attribute->name);
	if (error!=BELLE_SIP_OK) return error;
	if (attribute->value) {
		error=belle_sip_snprintf(buff,buff_size,offset,":%s",attribute->value);
		if (error!=BELLE_SIP_OK) return error;
	}
	return error;
}

BELLE_SDP_NEW(attribute,belle_sip_object)
belle_sdp_attribute_t* belle_sdp_attribute_create (const char* name,const char* value) {
	belle_sdp_attribute_t* attribute = belle_sdp_attribute_new();
	belle_sdp_attribute_set_name(attribute,name);
	belle_sdp_attribute_set_value(attribute,value);
	return attribute;
}
BELLE_SDP_PARSE(attribute)
GET_SET_STRING(belle_sdp_attribute,name);
GET_SET_STRING(belle_sdp_attribute,value);
unsigned int belle_sdp_attribute_as_value(const belle_sdp_attribute_t* attribute) {
	return attribute->value!=NULL;
}
/***************************************************************************************
 * Bandwidth
 *
 **************************************************************************************/
struct _belle_sdp_bandwidth {
	belle_sip_object_t base;
	const char* type;
	int value;
};
void belle_sdp_bandwidth_destroy(belle_sdp_bandwidth_t* bandwidth) {
	if (bandwidth->type) belle_sip_free((void*)bandwidth->type);
}

void belle_sdp_bandwidth_clone(belle_sdp_bandwidth_t *bandwidth, const belle_sdp_bandwidth_t *orig){
	CLONE_STRING(belle_sdp_bandwidth,type,bandwidth,orig)
	bandwidth->value=orig->value;
}

belle_sip_error_code belle_sdp_bandwidth_marshal(belle_sdp_bandwidth_t* bandwidth, char* buff, size_t buff_size, size_t *offset) {
	return belle_sip_snprintf(buff,buff_size,offset,"b=%s:%i",bandwidth->type,bandwidth->value);
}

BELLE_SDP_NEW(bandwidth,belle_sip_object)
BELLE_SDP_PARSE(bandwidth)
GET_SET_STRING(belle_sdp_bandwidth,type);
GET_SET_INT(belle_sdp_bandwidth,value,int)

/************************
 * connection
 ***********************/
struct _belle_sdp_connection {
	belle_sip_object_t base;
	const char* network_type;
	const char* address_type;
	const char* address;
 };

void belle_sdp_connection_destroy(belle_sdp_connection_t* connection) {
	DESTROY_STRING(connection,network_type)
	DESTROY_STRING(connection,address_type)
	DESTROY_STRING(connection,address)
}

void belle_sdp_connection_clone(belle_sdp_connection_t *connection, const belle_sdp_connection_t *orig){
	CLONE_STRING(belle_sdp_connection,network_type,connection,orig)
	CLONE_STRING(belle_sdp_connection,address_type,connection,orig)
	CLONE_STRING(belle_sdp_connection,address,connection,orig)

}

belle_sip_error_code belle_sdp_connection_marshal(belle_sdp_connection_t* connection, char* buff, size_t buff_size, size_t *offset) {
	return belle_sip_snprintf(buff,buff_size,offset,"c=%s %s %s",connection->network_type,connection->address_type,connection->address);
}

BELLE_SDP_NEW(connection,belle_sip_object)
BELLE_SDP_PARSE(connection)
belle_sdp_connection_t* belle_sdp_connection_create(const char* net_type, const char* addr_type, const char* addr) {
	belle_sdp_connection_t* connection = belle_sdp_connection_new();
	belle_sdp_connection_set_network_type(connection,net_type);
	belle_sdp_connection_set_address_type(connection,addr_type);
	belle_sdp_connection_set_address(connection,addr);
	return connection;
}
GET_SET_STRING(belle_sdp_connection,network_type);
GET_SET_STRING(belle_sdp_connection,address_type);
GET_SET_STRING(belle_sdp_connection,address);
/************************
 * email
 ***********************/
struct _belle_sdp_email {
	belle_sip_object_t base;
	const char* value;
 };

void belle_sdp_email_destroy(belle_sdp_email_t* email) {
	DESTROY_STRING(email,value)
}

void belle_sdp_email_clone(belle_sdp_email_t *email, const belle_sdp_email_t *orig){
	CLONE_STRING(belle_sdp_email,value,email,orig)
}

belle_sip_error_code belle_sdp_email_marshal(belle_sdp_email_t* email, char* buff, size_t buff_size, size_t *offset) {
	return belle_sip_snprintf(buff,buff_size,offset,"e=%s",email->value);
}

BELLE_SDP_NEW(email,belle_sip_object)
BELLE_SDP_PARSE(email)
GET_SET_STRING(belle_sdp_email,value);
/************************
 * info
 ***********************/
struct _belle_sdp_info {
	belle_sip_object_t base;
	const char* value;
 };

void belle_sdp_info_destroy(belle_sdp_info_t* info) {
	DESTROY_STRING(info,value)
}

void belle_sdp_info_clone(belle_sdp_info_t *info, const belle_sdp_info_t *orig){
	CLONE_STRING(belle_sdp_info,value,info,orig)
}

belle_sip_error_code belle_sdp_info_marshal(belle_sdp_info_t* info, char* buff, size_t buff_size, size_t *offset) {
	return belle_sip_snprintf(buff,buff_size,offset,"i=%s",info->value);
}

BELLE_SDP_NEW(info,belle_sip_object)
BELLE_SDP_PARSE(info)
GET_SET_STRING(belle_sdp_info,value);
/************************
 * media
 ***********************/
struct _belle_sdp_media {
	belle_sip_object_t base;
	const char* media_type;
	int media_port;
	belle_sip_list_t* media_formats;
	int port_count;
	const char* protocol;
 };
belle_sip_list_t*	belle_sdp_media_get_media_formats(const belle_sdp_media_t* media) {
	return media->media_formats;
}
void belle_sdp_media_set_media_formats( belle_sdp_media_t* media, belle_sip_list_t* formats) {
	/*belle_sip_list_free(media->media_formats); to allow easy list management might be better to add an append format method*/
	media->media_formats = formats;
}
void belle_sdp_media_destroy(belle_sdp_media_t* media) {
	DESTROY_STRING(media,media_type)
	belle_sip_list_free(media->media_formats);
	DESTROY_STRING(media,protocol)
}
static void belle_sdp_media_init(belle_sdp_media_t* media) {
	media->port_count=1;
}

void belle_sdp_media_clone(belle_sdp_media_t *media, const belle_sdp_media_t *orig){
	CLONE_STRING(belle_sdp_media,media_type,media,orig)
	media->media_port=orig->media_port;
	media->media_formats = belle_sip_list_copy(orig->media_formats);
	media->port_count=orig->port_count;
	CLONE_STRING(belle_sdp_media,protocol,media,orig)
}

belle_sip_error_code belle_sdp_media_marshal(belle_sdp_media_t* media, char* buff, size_t buff_size, size_t *offset) {
	belle_sip_list_t* list=media->media_formats;
	belle_sip_error_code error=belle_sip_snprintf(buff,buff_size,offset,"m=%s %i",media->media_type,media->media_port);
	if (error!=BELLE_SIP_OK) return error; 
	if (media->port_count>1) {
		error=belle_sip_snprintf(buff,buff_size,offset,"/%i",media->port_count);
		if (error!=BELLE_SIP_OK) return error; 
	}
	error=belle_sip_snprintf(buff,buff_size,offset," %s",media->protocol);
	if (error!=BELLE_SIP_OK) return error;
	for(;list!=NULL;list=list->next){
		error=belle_sip_snprintf(buff,buff_size,offset," %li",(long)list->data);
		if (error!=BELLE_SIP_OK) return error; 
	}
	return error;
}

BELLE_SDP_NEW_WITH_CTR(media,belle_sip_object)
BELLE_SDP_PARSE(media)
belle_sdp_media_t* belle_sdp_media_create(const char* media_type
                         ,int media_port
                         ,int port_count
                         ,const char* protocol
                         ,belle_sip_list_t* static_media_formats) {
	belle_sdp_media_t* media= belle_sdp_media_new();
	belle_sdp_media_set_media_type(media,media_type);
	belle_sdp_media_set_media_port(media,media_port);
	belle_sdp_media_set_port_count(media,port_count);
	belle_sdp_media_set_protocol(media,protocol);
	if (static_media_formats) belle_sdp_media_set_media_formats(media,static_media_formats);
	return media;
}
GET_SET_STRING(belle_sdp_media,media_type);
GET_SET_STRING(belle_sdp_media,protocol);
GET_SET_INT(belle_sdp_media,media_port,int)
GET_SET_INT(belle_sdp_media,port_count,int)

/************************
 * base_description
 ***********************/
static void belle_sip_object_freefunc(void* obj) {
	belle_sip_object_unref(BELLE_SIP_OBJECT(obj));
}
static void* belle_sip_object_copyfunc(void* obj) {
	return belle_sip_object_clone_and_ref(BELLE_SIP_OBJECT(obj));
}

typedef struct _belle_sdp_base_description {
	belle_sip_object_t base;
	belle_sdp_info_t* info;
	belle_sdp_connection_t* connection;
	belle_sip_list_t* bandwidths;
	belle_sip_list_t* attributes;
} belle_sdp_base_description_t;

static void belle_sdp_base_description_destroy(belle_sdp_base_description_t* base_description) {
	if (base_description->info) belle_sip_object_unref(BELLE_SIP_OBJECT(base_description->info));
	if (base_description->connection) belle_sip_object_unref(BELLE_SIP_OBJECT(base_description->connection));
	belle_sip_list_free_with_data(base_description->bandwidths,belle_sip_object_freefunc);
	belle_sip_list_free_with_data(base_description->attributes,belle_sip_object_freefunc);
}
static void belle_sdp_base_description_init(belle_sdp_base_description_t* base_description) {
}
static void belle_sdp_base_description_clone(belle_sdp_base_description_t *base_description, const belle_sdp_base_description_t *orig){
	if (orig->info) base_description->info = BELLE_SDP_INFO(belle_sip_object_clone_and_ref(BELLE_SIP_OBJECT(orig->info)));
	if (orig->connection) base_description->connection = BELLE_SDP_CONNECTION(belle_sip_object_clone_and_ref(BELLE_SIP_OBJECT(orig->connection)));
	base_description->bandwidths = belle_sip_list_copy_with_data(orig->bandwidths,belle_sip_object_copyfunc);
	base_description->attributes = belle_sip_list_copy_with_data(orig->attributes,belle_sip_object_copyfunc);

}

belle_sip_error_code belle_sdp_base_description_marshal(belle_sdp_base_description_t* base_description, char* buff, size_t buff_size, size_t *offset) {
	belle_sip_error_code error=BELLE_SIP_OK;
	belle_sip_list_t* bandwidths;
//	belle_sip_list_t* attributes;
	if (base_description->info) {
		error=belle_sip_object_marshal(BELLE_SIP_OBJECT(base_description->info),buff,buff_size,offset);
		if (error!=BELLE_SIP_OK) return error;
		error=belle_sip_snprintf(buff, buff_size, offset, "\r\n");
		if (error!=BELLE_SIP_OK) return error; 
	}
	if (base_description->connection) {
		error=belle_sip_object_marshal(BELLE_SIP_OBJECT(base_description->connection),buff,buff_size,offset);
		if (error!=BELLE_SIP_OK) return error;
		error=belle_sip_snprintf(buff, buff_size, offset, "\r\n");
		if (error!=BELLE_SIP_OK) return error;
	}
	for(bandwidths=base_description->bandwidths;bandwidths!=NULL;bandwidths=bandwidths->next){
		error=belle_sip_object_marshal(BELLE_SIP_OBJECT(bandwidths->data),buff,buff_size,offset);
		if (error!=BELLE_SIP_OK) return error;
		error=belle_sip_snprintf(buff, buff_size, offset, "\r\n");
		if (error!=BELLE_SIP_OK) return error;
	}
//	for(attributes=base_description->attributes;attributes!=NULL;attributes=attributes->next){
//		error=belle_sip_object_marshal(BELLE_SIP_OBJECT(attributes->data),buff,buff_size,offset);
//		error=belle_sip_snprintf(buff, buff_size, offset, "\r\n");
//	}
	return error;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sdp_base_description_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sdp_base_description_t
							,belle_sip_object_t
							,belle_sdp_base_description_destroy
							,belle_sdp_base_description_clone
							,belle_sdp_base_description_marshal
							,FALSE);

static int belle_sdp_base_description_attribute_comp_func(const belle_sdp_attribute_t* a, const char*b) {
	return strcmp(a->name,b);
}
const belle_sdp_attribute_t*	belle_sdp_base_description_get_attribute(const belle_sdp_base_description_t* base_description, const char* name) {
	belle_sip_list_t* attribute;
	attribute = belle_sip_list_find_custom(base_description->attributes, (belle_sip_compare_func)belle_sdp_base_description_attribute_comp_func, name);
	if (attribute) {
		return ((belle_sdp_attribute_t*)attribute->data);
	} else {
		return NULL;
	}
}
const char*	belle_sdp_base_description_get_attribute_value(const belle_sdp_base_description_t* base_description, const char* name) {
	const belle_sdp_attribute_t* attribute = belle_sdp_base_description_get_attribute(base_description,name);
	if (attribute)
		return belle_sdp_attribute_get_value(attribute);
	else
		return NULL;

}
belle_sip_list_t* belle_sdp_base_description_get_attributes(const belle_sdp_base_description_t* base_description) {
	return base_description->attributes;
}
static int belle_sdp_base_description_bandwidth_comp_func(const belle_sdp_bandwidth_t* a, const char*b) {
	return strcmp(a->type,b);
}

int	belle_sdp_base_description_get_bandwidth(const belle_sdp_base_description_t* base_description, const char* name) {
	belle_sip_list_t* bandwidth;
	bandwidth = belle_sip_list_find_custom(base_description->bandwidths, (belle_sip_compare_func)belle_sdp_base_description_bandwidth_comp_func, name);
	if (bandwidth) {
		return ((belle_sdp_bandwidth_t*)bandwidth->data)->value;
	} else {
		return -1;
	}
}
void belle_sdp_base_description_remove_attribute(belle_sdp_base_description_t* base_description,const char* name) {
	belle_sip_list_t* attribute;
	attribute = belle_sip_list_find_custom(base_description->attributes, (belle_sip_compare_func)belle_sdp_base_description_attribute_comp_func, name);
	if (attribute) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(attribute->data));
		base_description->attributes = belle_sip_list_remove_link(base_description->attributes,attribute);
	}

}
void belle_sdp_base_description_remove_bandwidth(belle_sdp_base_description_t* base_description,const char* name) {
	belle_sip_list_t* bandwidth;
	bandwidth = belle_sip_list_find_custom(base_description->bandwidths, (belle_sip_compare_func)belle_sdp_base_description_bandwidth_comp_func, name);
	if (bandwidth) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(bandwidth->data));
		base_description->bandwidths = belle_sip_list_remove_link(base_description->bandwidths,bandwidth);
	}
}
void belle_sdp_base_description_set_attribute_value(belle_sdp_base_description_t* base_description, const char* name, const char* value) {
	belle_sdp_attribute_t* attribute = belle_sdp_attribute_new();
	belle_sdp_attribute_set_name(attribute,name);
	belle_sdp_attribute_set_value(attribute,value);
	base_description->attributes = belle_sip_list_append(base_description->attributes,belle_sip_object_ref(attribute));
}
void belle_sdp_base_description_add_attribute(belle_sdp_base_description_t* base_description, const belle_sdp_attribute_t* attribute) {
	base_description->attributes = belle_sip_list_append(base_description->attributes,(void*)belle_sip_object_ref(BELLE_SIP_OBJECT(attribute)));
}

#define SET_LIST(list_name,value) \
		belle_sip_list_t* list;\
		if (list_name) {\
			for (list=list_name;list !=NULL; list=list->next) {\
				belle_sip_object_unref(BELLE_SIP_OBJECT(list->data));\
			}\
			belle_sip_list_free(list_name); \
		} \
		for (list=value;list !=NULL; list=list->next) {\
						belle_sip_object_ref(BELLE_SIP_OBJECT(list->data));\
		}\
		list_name=value;


void belle_sdp_base_description_set_attributes(belle_sdp_base_description_t* base_description, belle_sip_list_t* attributes) {
	SET_LIST(base_description->attributes,attributes)
}
void belle_sdp_base_description_set_bandwidth(belle_sdp_base_description_t* base_description, const char* type, int value) {
	belle_sdp_bandwidth_t* bandwidth = belle_sdp_bandwidth_new();
	belle_sdp_bandwidth_set_type(bandwidth,type);
	belle_sdp_bandwidth_set_value(bandwidth,value);
	base_description->bandwidths = belle_sip_list_append(base_description->bandwidths,belle_sip_object_ref(bandwidth));
}
void belle_sdp_base_description_add_bandwidth(belle_sdp_base_description_t* base_description, const belle_sdp_bandwidth_t* bandwidth) {
	base_description->bandwidths = belle_sip_list_append(base_description->bandwidths,(void *)belle_sip_object_ref((void *)bandwidth));
}
void belle_sdp_base_description_set_bandwidths(belle_sdp_base_description_t* base_description, belle_sip_list_t* bandwidths) {
	SET_LIST(base_description->bandwidths,bandwidths)
}

/************************
 * media_description
 ***********************/
struct _belle_sdp_media_description {
	belle_sdp_base_description_t base_description;
	belle_sdp_media_t* media;
};
void belle_sdp_media_description_destroy(belle_sdp_media_description_t* media_description) {
	if (media_description->media) belle_sip_object_unref(BELLE_SIP_OBJECT((media_description->media)));
}

void belle_sdp_media_description_clone(belle_sdp_media_description_t *media_description, const belle_sdp_media_description_t *orig){
	if (orig->media) media_description->media = BELLE_SDP_MEDIA(belle_sip_object_clone_and_ref(BELLE_SIP_OBJECT((orig->media))));
}

belle_sip_error_code belle_sdp_media_description_marshal(belle_sdp_media_description_t* media_description, char* buff, size_t buff_size, size_t *offset) {
	belle_sip_list_t* attributes;
	belle_sip_error_code error=belle_sip_object_marshal(BELLE_SIP_OBJECT(media_description->media),buff,buff_size,offset);
	if (error!=BELLE_SIP_OK) return error;
	error=belle_sip_snprintf(buff, buff_size, offset, "\r\n");
	if (error!=BELLE_SIP_OK) return error;
	error=belle_sdp_base_description_marshal(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),buff,buff_size,offset);
	if (error!=BELLE_SIP_OK) return error;

	for(attributes=media_description->base_description.attributes;attributes!=NULL;attributes=attributes->next){
		error=belle_sip_object_marshal(BELLE_SIP_OBJECT(attributes->data),buff,buff_size,offset);
		if (error!=BELLE_SIP_OK) return error;
		error=belle_sip_snprintf(buff, buff_size, offset, "\r\n");
		if (error!=BELLE_SIP_OK) return error;
	}
	return error;
}

BELLE_SDP_NEW(media_description,belle_sdp_base_description)
belle_sdp_media_description_t* belle_sdp_media_description_create(const char* media_type
                         	 	 	 	 	 	 	 	 	 	 ,int media_port
                         	 	 	 	 	 	 	 	 	 	 ,int port_count
                         	 	 	 	 	 	 	 	 	 	 ,const char* protocol
                         	 	 	 	 	 	 	 	 	 	 ,belle_sip_list_t* static_media_formats) {
	belle_sdp_media_description_t* media_desc=belle_sdp_media_description_new();
	belle_sdp_media_description_set_media(media_desc,belle_sdp_media_create(media_type,media_port,port_count,protocol,static_media_formats));
	return media_desc;
}
BELLE_SDP_PARSE(media_description)
void belle_sdp_media_description_add_dynamic_payloads(belle_sdp_media_description_t* media_description, belle_sip_list_t* payloadNames, belle_sip_list_t* payloadValues) {
	belle_sip_error("belle_sdp_media_description_add_dynamic_payloads not implemented yet");
}
const belle_sdp_attribute_t*	belle_sdp_media_description_get_attribute(const belle_sdp_media_description_t* media_description, const char* name) {
	return belle_sdp_base_description_get_attribute(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),name);
}
const char*	belle_sdp_media_description_get_attribute_value(const belle_sdp_media_description_t* media_description, const char* name) {
	return belle_sdp_base_description_get_attribute_value(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),name);
}
belle_sip_list_t* belle_sdp_media_description_get_attributes(const belle_sdp_media_description_t* media_description) {
	return BELLE_SIP_CAST(media_description,belle_sdp_base_description_t)->attributes;
}

int	belle_sdp_media_description_get_bandwidth(const belle_sdp_media_description_t* media_description, const char* name) {
	return belle_sdp_base_description_get_bandwidth(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),name);
}
belle_sip_list_t* belle_sdp_media_description_get_bandwidths(const belle_sdp_media_description_t* media_description) {
	return BELLE_SIP_CAST(media_description,belle_sdp_base_description_t)->bandwidths;
}
belle_sdp_connection_t*	belle_sdp_media_description_get_connection(const belle_sdp_media_description_t* media_description) {
	return BELLE_SIP_CAST(media_description,belle_sdp_base_description_t)->connection;
}
belle_sdp_info_t* belle_sdp_media_description_get_info(const belle_sdp_media_description_t* media_description) {
	return BELLE_SIP_CAST(media_description,belle_sdp_base_description_t)->info;
}
/*belle_sdp_key_t*  belle_sdp_media_description_get_key(const belle_sdp_media_description_t* media_description);*/
belle_sdp_media_t* belle_sdp_media_description_get_media(const belle_sdp_media_description_t* media_description) {
	return media_description->media;
}

struct static_payload {
	unsigned char number;
	int channel_count;
	const char* type;
	int	rate;
};
#define STATIC_PAYLOAD_LIST_LENTH 8
/*
 * rfc 3551
 * PT   encoding    media type  clock rate   channels
                    name                    (Hz)
               ___________________________________________________
               0    PCMU        A            8,000       1
               1    reserved    A
               2    reserved    A
               3    GSM         A            8,000       1
               4    G723        A            8,000       1
               5    DVI4        A            8,000       1
               6    DVI4        A           16,000       1
               7    LPC         A            8,000       1
               8    PCMA        A            8,000       1
               9    G722        A            8,000       1
               10   L16         A           44,100       2
               11   L16         A           44,100       1
               12   QCELP       A            8,000       1
               13   CN          A            8,000       1
               14   MPA         A           90,000       (see text)
               15   G728        A            8,000       1
               16   DVI4        A           11,025       1
               17   DVI4        A           22,050       1
               18   G729        A            8,000       1
               Table 4: Payload types (PT) for audio encodings

  PT      encoding    media type  clock rate
                       name                    (Hz)
               _____________________________________________
               24      unassigned  V
               25      CelB        V           90,000
               26      JPEG        V           90,000
               27      unassigned  V
               28      nv          V           90,000
               29      unassigned  V
               30      unassigned  V
               31      H261        V           90,000
               32      MPV         V           90,000
               33      MP2T        AV          90,000
               34      H263        V           90,000

               Table 5: Payload types (PT) for video and combined
                        encodings


 *
 * */

struct static_payload static_payload_list [] ={
	/*audio*/
	{0,1,"PCMU",8000},
	{3,1,"GSM",8000},
	{4,1,"G723",8000},
	{5,1,"DVI4",8000},
	{6,1,"DVI4",16000},
	{7,1,"LPC",8000},
	{8,1,"PCMA",8000},
	{9,1,"G722",8000},
	{10,2,"L16",44100},
	{11,1,"L16",44100},
	{12,1,"QCELP",8000},
	{13,1,"CN",8000},
	{14,1,"MPA",90000},
	{15,1,"G728",8000},
	{16,1,"DVI4",11025},
	{17,1,"DVI4",22050},
	{18,1,"G729",8000},
	/*video*/
	{25,-1,"CelB",90000},
	{26,-1,"JPEG",90000},
	{28,-1,"nv",90000},
	{31,-1,"H261",90000},
	{32,-1,"MPV",90000},
	{33,-1,"MP2T",90000},
	{34,-1,"H263",90000}
};
static int mime_parameter_fill_from_static(belle_sdp_mime_parameter_t *mime_parameter,int format) {
	struct static_payload* iterator = static_payload_list;
	int i;
	static size_t payload_list_element=sizeof(static_payload_list)/sizeof(struct static_payload);
	for (i=0;i<payload_list_element;i++) {
		if (iterator->number == format) {
			belle_sdp_mime_parameter_set_type(mime_parameter,iterator->type);
			belle_sdp_mime_parameter_set_rate(mime_parameter,iterator->rate);
			belle_sdp_mime_parameter_set_channel_count(mime_parameter,iterator->channel_count);
		} else {
			iterator++;
		}
	}
	return 0;
}
static int mime_parameter_fill_from_rtpmap(belle_sdp_mime_parameter_t *mime_parameter, const char *rtpmap){
	char *mime=belle_sip_strdup(rtpmap);
	char *p=strchr(mime,'/');
	if (p){
		char *chans;
		*p='\0';
		p++;
		chans=strchr(p,'/');
		if (chans){
			*chans='\0';
			chans++;
			belle_sdp_mime_parameter_set_channel_count(mime_parameter,atoi(chans));
		}else belle_sdp_mime_parameter_set_channel_count(mime_parameter,1);
		belle_sdp_mime_parameter_set_rate(mime_parameter,atoi(p));
	}
	belle_sdp_mime_parameter_set_type(mime_parameter,mime);
	belle_sip_free(mime);
	return 0;
}
/* return the value of attr "field" for payload pt at line pos (field=rtpmap,fmtp...)*/
static const char *belle_sdp_media_description_a_attr_value_get_with_pt(const belle_sdp_media_description_t* media_description,int pt,const char *field)
{
	int tmppt=0,scanned=0;
	const char *tmp;
	belle_sdp_attribute_t *attr;
	belle_sip_list_t* attribute_list;
	for (	attribute_list =belle_sdp_media_description_get_attributes(media_description)
						;attribute_list!=NULL
						;attribute_list=attribute_list->next) {

		attr = BELLE_SDP_ATTRIBUTE(attribute_list->data);
		if (strcmp(field,belle_sdp_attribute_get_name(attr))==0 && belle_sdp_attribute_get_value(attr)!=NULL){
			int nb = sscanf(belle_sdp_attribute_get_value(attr),"%i %n",&tmppt,&scanned);
			/* the return value may depend on how %n is interpreted by the libc: see manpage*/
			if (nb == 1 || nb==2 ){
				if (pt==tmppt){
					tmp=belle_sdp_attribute_get_value(attr)+scanned;
					if (strlen(tmp)>0)
						return tmp;
				}
			}else belle_sip_warning("sdp has a strange a= line (%s) nb=%i",belle_sdp_attribute_get_value(attr),nb);
		}
	}
	return NULL;
}

belle_sip_list_t* belle_sdp_media_description_build_mime_parameters(const belle_sdp_media_description_t* media_description) {
	/*First, get media type*/
	belle_sdp_media_t* media = belle_sdp_media_description_get_media(media_description);
	belle_sip_list_t* mime_parameter_list=NULL;
	belle_sip_list_t* media_formats=NULL;
	belle_sdp_mime_parameter_t* mime_parameter;
	const char* rtpmap=NULL;
	const char* fmtp=NULL;
	const char* ptime=NULL;
	const char* max_ptime=NULL;
	int ptime_as_int=-1;
	int max_ptime_as_int=-1;
	if (!media) {
		belle_sip_error("belle_sdp_media_description_build_mime_parameters: no media");
		return NULL;
	}
	ptime = belle_sdp_media_description_get_attribute_value(media_description,"ptime");
	ptime?ptime_as_int=atoi(ptime):-1;
	max_ptime = belle_sdp_media_description_get_attribute_value(media_description,"maxptime");
	max_ptime?max_ptime_as_int=atoi(max_ptime):-1;

	for (media_formats = belle_sdp_media_get_media_formats(media);media_formats!=NULL;media_formats=media_formats->next) {
		/*create mime parameters with format*/
		mime_parameter = belle_sdp_mime_parameter_new();
		belle_sdp_mime_parameter_set_ptime(mime_parameter,ptime_as_int);
		belle_sdp_mime_parameter_set_max_ptime(mime_parameter,max_ptime_as_int);
		belle_sdp_mime_parameter_set_media_format(mime_parameter,(int)(long)media_formats->data);
		mime_parameter_fill_from_static(mime_parameter,belle_sdp_mime_parameter_get_media_format(mime_parameter));
		/*get rtpmap*/
		rtpmap = belle_sdp_media_description_a_attr_value_get_with_pt(media_description
																		,belle_sdp_mime_parameter_get_media_format(mime_parameter)
																		,"rtpmap");
		if (rtpmap) {
			mime_parameter_fill_from_rtpmap(mime_parameter,rtpmap);
		}
		fmtp = belle_sdp_media_description_a_attr_value_get_with_pt(media_description
																		,belle_sdp_mime_parameter_get_media_format(mime_parameter)
																		,"fmtp");
		if (fmtp) {
			belle_sdp_mime_parameter_set_parameters(mime_parameter,fmtp);
		}

		mime_parameter_list=belle_sip_list_append(mime_parameter_list,mime_parameter);
	}
	return mime_parameter_list;
}
#define MAX_FMTP_LENGH 512

void belle_sdp_media_description_append_values_from_mime_parameter(belle_sdp_media_description_t* media_description, const belle_sdp_mime_parameter_t* mime_parameter) {
	belle_sdp_media_t* media = belle_sdp_media_description_get_media(media_description);
	char atribute_value [MAX_FMTP_LENGH];
	int current_ptime=0;
	int current_max_ptime=0;

	belle_sdp_media_set_media_formats(media,belle_sip_list_append(belle_sdp_media_get_media_formats(media)
																,(void*)(long)(belle_sdp_mime_parameter_get_media_format(mime_parameter))));

	if (belle_sdp_media_description_get_attribute_value(media_description,"ptime")) {
		current_ptime=atoi(belle_sdp_media_description_get_attribute_value(media_description,"ptime"));
		belle_sdp_media_description_remove_attribute(media_description,"ptime");
	}

	if (belle_sdp_media_description_get_attribute_value(media_description,"maxptime")) {
		current_max_ptime=atoi(belle_sdp_media_description_get_attribute_value(media_description,"maxptime"));
		belle_sdp_media_description_remove_attribute(media_description,"maxptime");
	}

#ifndef BELLE_SDP_FORCE_RTP_MAP /* defined to for RTP map even for static codec*/
	if (belle_sdp_mime_parameter_get_media_format(mime_parameter) > 34) {
		/*dynamic payload*/
#endif
		if (belle_sdp_mime_parameter_get_channel_count(mime_parameter)>1) {
			snprintf(atribute_value,MAX_FMTP_LENGH,"%i %s/%i/%i"
					,belle_sdp_mime_parameter_get_media_format(mime_parameter)
					,belle_sdp_mime_parameter_get_type(mime_parameter)
					,belle_sdp_mime_parameter_get_rate(mime_parameter)
					,belle_sdp_mime_parameter_get_channel_count(mime_parameter));
		} else {
			snprintf(atribute_value,MAX_FMTP_LENGH,"%i %s/%i"
					,belle_sdp_mime_parameter_get_media_format(mime_parameter)
					,belle_sdp_mime_parameter_get_type(mime_parameter)
					,belle_sdp_mime_parameter_get_rate(mime_parameter));
		}
		belle_sdp_media_description_set_attribute_value(media_description,"rtpmap",atribute_value);
		if (belle_sdp_mime_parameter_get_parameters(mime_parameter)) {
			snprintf(atribute_value,MAX_FMTP_LENGH,"%i %s"
					,belle_sdp_mime_parameter_get_media_format(mime_parameter)
					,belle_sdp_mime_parameter_get_parameters(mime_parameter));
			belle_sdp_media_description_set_attribute_value(media_description,"fmtp",atribute_value);
		}
#ifndef BELLE_SDP_FORCE_RTP_MAP
	}
#endif
	if (belle_sdp_mime_parameter_get_ptime(mime_parameter)>current_ptime) {
		current_ptime=belle_sdp_mime_parameter_get_ptime(mime_parameter);
	}
	if (current_ptime>0){
		char  ptime[10];
		snprintf(ptime,sizeof(ptime),"%i",current_ptime);
		belle_sdp_media_description_set_attribute_value(media_description,"ptime",ptime);
	}

	if (belle_sdp_mime_parameter_get_max_ptime(mime_parameter)>current_max_ptime) {
		current_max_ptime=belle_sdp_mime_parameter_get_max_ptime(mime_parameter);
	}
	if (current_max_ptime>0){
		char  max_ptime[10];
		snprintf(max_ptime,sizeof(max_ptime),"%i",current_max_ptime);
		belle_sdp_media_description_set_attribute_value(media_description,"maxptime",max_ptime);
	}

}
belle_sip_list_t* belle_sdp_media_description_get_mime_types(const belle_sdp_media_description_t* media_description) {
	belle_sip_error("belle_sdp_media_description_get_mime_types: not implemented yet");
	return NULL;
}

void belle_sdp_media_description_remove_attribute(belle_sdp_media_description_t* media_description,const char* name) {
	belle_sdp_base_description_remove_attribute(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),name);
}
void belle_sdp_media_description_remove_bandwidth(belle_sdp_media_description_t* media_description,const char* name) {
	belle_sdp_base_description_remove_bandwidth(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),name);
}
void belle_sdp_media_description_set_attribute_value(belle_sdp_media_description_t* media_description, const char* name, const char* value) {
	belle_sdp_base_description_set_attribute_value(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),name,value);
}
void belle_sdp_media_description_set_attributes(belle_sdp_media_description_t* media_description, belle_sip_list_t* value) {
	belle_sdp_base_description_set_attributes(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),value);
}

void belle_sdp_media_description_add_attribute(belle_sdp_media_description_t* media_description, const belle_sdp_attribute_t* attribute) {
	belle_sdp_base_description_add_attribute(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),attribute);
}
void belle_sdp_media_description_set_bandwidth(belle_sdp_media_description_t* media_description, const char* type, int value) {
	belle_sdp_base_description_set_bandwidth(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),type,value);
}
void belle_sdp_media_description_add_bandwidth(belle_sdp_media_description_t* media_description, const belle_sdp_bandwidth_t* bandwidth) {
	belle_sdp_base_description_add_bandwidth(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),bandwidth);
}
void belle_sdp_media_description_set_bandwidths(belle_sdp_media_description_t* media_description, belle_sip_list_t* bandwidths) {
	belle_sdp_base_description_set_bandwidths(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),bandwidths);
}
#define SET_OBJECT(object,param,param_type) \
		param_type** current = &object->param; \
		if (param) belle_sip_object_ref(param); \
		if (*current) { \
			belle_sip_object_unref(BELLE_SIP_OBJECT(*current)); \
		} \
		*current=param; \


void belle_sdp_media_description_set_connection(belle_sdp_media_description_t* media_description, belle_sdp_connection_t* connection) {
	SET_OBJECT(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),connection,belle_sdp_connection_t)
}
void belle_sdp_media_description_set_info(belle_sdp_media_description_t* media_description,belle_sdp_info_t* info) {
	SET_OBJECT(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),info,belle_sdp_info_t)
}
/*void belle_sdp_media_description_set_key(belle_sdp_media_description_t* media_description,belle_sdp_key_t* key);*/
void belle_sdp_media_description_set_media(belle_sdp_media_description_t* media_description, belle_sdp_media_t* media) {
	SET_OBJECT(media_description,media,belle_sdp_media_t)
}

/************************
 * origin
 ***********************/
struct _belle_sdp_origin {
	belle_sip_object_t base;
	const char* address;
	const char* address_type;
	const char* network_type;
	const char* username;
	unsigned int session_id;
	unsigned int session_version;

 };

void belle_sdp_origin_destroy(belle_sdp_origin_t* origin) {
	DESTROY_STRING(origin,address)
	DESTROY_STRING(origin,address_type)
	DESTROY_STRING(origin,network_type)
	DESTROY_STRING(origin,username)
}

void belle_sdp_origin_clone(belle_sdp_origin_t *origin, const belle_sdp_origin_t *orig){
	CLONE_STRING(belle_sdp_origin,username,origin,orig);
	CLONE_STRING(belle_sdp_origin,address,origin,orig);
	CLONE_STRING(belle_sdp_origin,address_type,origin,orig);
	CLONE_STRING(belle_sdp_origin,network_type,origin,orig);
	origin->session_id = orig->session_id;
	origin->session_version = orig->session_version;
}

belle_sip_error_code belle_sdp_origin_marshal(belle_sdp_origin_t* origin, char* buff, size_t buff_size, size_t *offset) {
	return belle_sip_snprintf(	buff
								,buff_size
								,offset
								,"o=%s %i %i %s %s %s"
								,origin->username
								,origin->session_id
								,origin->session_version
								,origin->network_type
								,origin->address_type
								,origin->address);
}

BELLE_SDP_NEW(origin,belle_sip_object)
belle_sdp_origin_t* belle_sdp_origin_create(const char* user_name
											, unsigned int session_id
											, unsigned int session_version
											, const char* network_type
											, const char* addr_type
											, const char* address) {
	belle_sdp_origin_t* origin=belle_sdp_origin_new();
	belle_sdp_origin_set_username(origin,user_name);
	belle_sdp_origin_set_session_id(origin,session_id);
	belle_sdp_origin_set_session_version(origin,session_version);
	belle_sdp_origin_set_network_type(origin,network_type);
	belle_sdp_origin_set_address_type(origin,addr_type);
	belle_sdp_origin_set_address(origin,address);
	return origin;
}
BELLE_SDP_PARSE(origin)
GET_SET_STRING(belle_sdp_origin,username);
GET_SET_STRING(belle_sdp_origin,address);
GET_SET_STRING(belle_sdp_origin,address_type);
GET_SET_STRING(belle_sdp_origin,network_type);
GET_SET_INT(belle_sdp_origin,session_id,unsigned int);
GET_SET_INT(belle_sdp_origin,session_version,unsigned int);
/************************
 * session_name
 ***********************/
struct _belle_sdp_session_name {
	belle_sip_object_t base;
	const char* value;
 };

void belle_sdp_session_name_destroy(belle_sdp_session_name_t* session_name) {
	DESTROY_STRING(session_name,value)
}

void belle_sdp_session_name_clone(belle_sdp_session_name_t *session_name, const belle_sdp_session_name_t *orig){
	CLONE_STRING(belle_sdp_session_name,value,session_name,orig);
}

belle_sip_error_code belle_sdp_session_name_marshal(belle_sdp_session_name_t* session_name, char* buff, size_t buff_size, size_t *offset) {
	return belle_sip_snprintf(buff,buff_size,offset,"s=%s",session_name->value);
}

BELLE_SDP_NEW(session_name,belle_sip_object)
belle_sdp_session_name_t* belle_sdp_session_name_create (const char* name) {
	belle_sdp_session_name_t* n=belle_sdp_session_name_new();
	belle_sdp_session_name_set_value(n,name);
	return n;
}
//BELLE_SDP_PARSE(session_name)
GET_SET_STRING(belle_sdp_session_name,value);


/************************
 * session_description
 ***********************/
struct _belle_sdp_session_description {
	belle_sdp_base_description_t base_description;
	belle_sdp_version_t* version;
	belle_sip_list_t* emails;
	belle_sdp_origin_t* origin;
	belle_sdp_session_name_t* session_name;
	belle_sip_list_t* phones;
	belle_sip_list_t* times;
	belle_sdp_uri_t* uri;
	belle_sdp_uri_t* zone_adjustments;
	belle_sip_list_t* media_descriptions;

 };
void belle_sdp_session_description_destroy(belle_sdp_session_description_t* session_description) {
	if (session_description->version) belle_sip_object_unref(BELLE_SIP_OBJECT(session_description->version));
	belle_sip_list_free_with_data(session_description->emails,belle_sip_object_freefunc);
	if (session_description->origin) belle_sip_object_unref(BELLE_SIP_OBJECT(session_description->origin));
	if (session_description->session_name) belle_sip_object_unref(BELLE_SIP_OBJECT(session_description->session_name));
	belle_sip_list_free_with_data(session_description->phones,belle_sip_object_freefunc);
	belle_sip_list_free_with_data(session_description->times,belle_sip_object_freefunc);
	if (session_description->uri) belle_sip_object_unref(BELLE_SIP_OBJECT(session_description->uri));
	if (session_description->zone_adjustments) belle_sip_object_unref(BELLE_SIP_OBJECT(session_description->zone_adjustments));
	belle_sip_list_free_with_data(session_description->media_descriptions,belle_sip_object_freefunc);
}

void belle_sdp_session_description_clone(belle_sdp_session_description_t *session_description, const belle_sdp_session_description_t *orig){
	if (orig->version) session_description->version = BELLE_SDP_VERSION(belle_sip_object_clone_and_ref(BELLE_SIP_OBJECT(orig->version)));
	session_description->emails = belle_sip_list_copy_with_data(orig->emails,belle_sip_object_copyfunc);
	if (orig->origin) session_description->origin = BELLE_SDP_ORIGIN(belle_sip_object_clone_and_ref(BELLE_SIP_OBJECT(orig->origin)));
	if (orig->session_name) session_description->session_name = BELLE_SDP_SESSION_NAME(belle_sip_object_clone_and_ref(BELLE_SIP_OBJECT(orig->session_name)));
	session_description->phones = belle_sip_list_copy_with_data(orig->phones,belle_sip_object_copyfunc);
	session_description->times = belle_sip_list_copy_with_data(orig->times,belle_sip_object_copyfunc);
	if (orig->uri) session_description->uri = BELLE_SDP_URI(belle_sip_object_clone_and_ref(BELLE_SIP_OBJECT(orig->uri)));
	if (orig->zone_adjustments) session_description->zone_adjustments = BELLE_SDP_URI(belle_sip_object_clone_and_ref(BELLE_SIP_OBJECT(orig->zone_adjustments)));
	session_description->media_descriptions = belle_sip_list_copy_with_data(orig->media_descriptions,belle_sip_object_copyfunc);
}

belle_sip_error_code belle_sdp_session_description_marshal(belle_sdp_session_description_t* session_description, char* buff, size_t buff_size, size_t *offset) {
/*session_description:   proto_version CR LF
                         origin_field
                         session_name_field
                         (info CR LF)?
                         uri_field?
                         (email CR LF)*
                         phone_field*
                         (connection CR LF)?
                         (bandwidth CR LF)*
                         time_field
                         (repeat_time CR LF)?
                         (zone_adjustments CR LF)?
                         (key_field CR LF)?
                         (attribute CR LF)*
                         media_descriptions;
 */
	belle_sip_error_code error=BELLE_SIP_OK;
	belle_sip_list_t* media_descriptions;
	belle_sip_list_t* times;
	belle_sip_list_t* attributes;

	error=belle_sip_object_marshal(BELLE_SIP_OBJECT(session_description->version),buff,buff_size,offset);
	if (error!=BELLE_SIP_OK) return error;
	error=belle_sip_snprintf(buff, buff_size, offset, "\r\n");
	if (error!=BELLE_SIP_OK) return error;

	error=belle_sip_object_marshal(BELLE_SIP_OBJECT(session_description->origin),buff,buff_size,offset);
	if (error!=BELLE_SIP_OK) return error;
	error=belle_sip_snprintf(buff, buff_size, offset, "\r\n");
	if (error!=BELLE_SIP_OK) return error;

	error=belle_sip_object_marshal(BELLE_SIP_OBJECT(session_description->session_name),buff,buff_size,offset);
	if (error!=BELLE_SIP_OK) return error;
	error=belle_sip_snprintf(buff, buff_size, offset, "\r\n");
	if (error!=BELLE_SIP_OK) return error;

	error=belle_sdp_base_description_marshal((belle_sdp_base_description_t*)(&session_description->base_description),buff,buff_size, offset);
	if (error!=BELLE_SIP_OK) return error;
	
	error=belle_sip_snprintf(buff, buff_size, offset, "t=");
	if (error!=BELLE_SIP_OK) return error;
	for(times=session_description->times;times!=NULL;times=times->next){
		error=belle_sip_object_marshal(BELLE_SIP_OBJECT(times->data),buff,buff_size,offset);
		if (error!=BELLE_SIP_OK) return error;
		error=belle_sip_snprintf(buff, buff_size, offset, "\r\n");
		if (error!=BELLE_SIP_OK) return error;
	}

	for(attributes=session_description->base_description.attributes;attributes!=NULL;attributes=attributes->next){
		error=belle_sip_object_marshal(BELLE_SIP_OBJECT(attributes->data),buff,buff_size,offset);
		if (error!=BELLE_SIP_OK) return error;
		error=belle_sip_snprintf(buff, buff_size, offset, "\r\n");
		if (error!=BELLE_SIP_OK) return error;
	}

	for(media_descriptions=session_description->media_descriptions;media_descriptions!=NULL;media_descriptions=media_descriptions->next){
		error=belle_sip_object_marshal(BELLE_SIP_OBJECT(media_descriptions->data),buff,buff_size,offset);
		if (error!=BELLE_SIP_OK) return error;
	}
	return error;
}

BELLE_SDP_NEW(session_description,belle_sdp_base_description)
BELLE_SDP_PARSE(session_description)

belle_sdp_session_description_t* belle_sdp_session_description_create(belle_sip_message_t* message) {
	belle_sdp_session_description_t* session_desc=NULL;
	belle_sip_header_content_type_t* content_type=belle_sip_message_get_header_by_type(message,belle_sip_header_content_type_t);
	if (content_type
		&& strcmp("application",belle_sip_header_content_type_get_type(content_type))==0
		&&	strcmp("sdp",belle_sip_header_content_type_get_subtype(content_type))==0) {
		session_desc=belle_sdp_session_description_parse(belle_sip_message_get_body(message));
	}
	return session_desc;
}
const char*	belle_sdp_session_description_get_attribute_value(const belle_sdp_session_description_t* session_description, const char* name) {
	return belle_sdp_base_description_get_attribute_value(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),name);
}
const belle_sdp_attribute_t*	belle_sdp_session_description_get_attribute(const belle_sdp_session_description_t* session_description, const char* name) {
	return belle_sdp_base_description_get_attribute(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),name);
}
int	belle_sdp_session_description_get_bandwidth(const belle_sdp_session_description_t* session_description, const char* name) {
	return belle_sdp_base_description_get_bandwidth(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),name);
}
belle_sip_list_t*	belle_sdp_session_description_get_bandwidths(const belle_sdp_session_description_t* session_description) {
	return BELLE_SIP_CAST(session_description,belle_sdp_base_description_t)->bandwidths;
}
belle_sdp_connection_t*	belle_sdp_session_description_get_connection(const belle_sdp_session_description_t* session_description) {
	return BELLE_SIP_CAST(session_description,belle_sdp_base_description_t)->connection;
}
belle_sip_list_t* belle_sdp_session_description_get_emails(const belle_sdp_session_description_t* session_description){
	return session_description->emails;
}
belle_sdp_info_t* belle_sdp_session_description_get_info(const belle_sdp_session_description_t* session_description) {
	return BELLE_SIP_CAST(session_description,belle_sdp_base_description_t)->info;
}
/*belle_sdp_key_t*	belle_sdp_session_description_get_key(const belle_sdp_session_description_t* session_description);*/
belle_sip_list_t* belle_sdp_session_description_get_media_descriptions(const belle_sdp_session_description_t* session_description) {
	return session_description->media_descriptions;
}
belle_sdp_origin_t*	belle_sdp_session_description_get_origin(const belle_sdp_session_description_t* session_description){
	return session_description->origin;
}
belle_sip_list_t* belle_sdp_session_description_get_phones(const belle_sdp_session_description_t* session_description) {
	return session_description->phones;
}
belle_sdp_session_name_t* belle_sdp_session_description_get_session_name(const belle_sdp_session_description_t* session_description) {
	return session_description->session_name;
}
belle_sip_list_t* belle_sdp_session_description_get_time_descriptions(const belle_sdp_session_description_t* session_description) {
	return session_description->times;
}
belle_sdp_uri_t* belle_sdp_session_description_get_uri(const belle_sdp_session_description_t* session_description) {
	return session_description->uri;
}
belle_sdp_version_t*	belle_sdp_session_description_get_version(const belle_sdp_session_description_t* session_description) {
	return session_description->version;
}
belle_sdp_uri_t* belle_sdp_session_description_get_zone_adjustments(const belle_sdp_session_description_t* session_description) {
	return session_description->zone_adjustments;
}
void belle_sdp_session_description_remove_attribute(belle_sdp_session_description_t* session_description, const char* name) {
	belle_sdp_base_description_remove_attribute(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),name);
}
void belle_sdp_session_description_remove_bandwidth(belle_sdp_session_description_t* session_description, const char* name) {
	belle_sdp_base_description_remove_bandwidth(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),name);
}
void belle_sdp_session_description_set_attribute_value(belle_sdp_session_description_t* session_description, const char* name, const char* value) {
	belle_sdp_base_description_set_attribute_value(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),name,value);
}
void belle_sdp_session_description_set_attributes(belle_sdp_session_description_t* session_description, belle_sip_list_t* attributes) {
	belle_sdp_base_description_set_attributes(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),attributes);
}
void belle_sdp_session_description_add_attribute(belle_sdp_session_description_t* session_description, const belle_sdp_attribute_t* attribute) {
	belle_sdp_base_description_add_attribute(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),attribute);
}
void belle_sdp_session_description_set_bandwidth(belle_sdp_session_description_t* session_description, const char* type, int value) {
	belle_sdp_base_description_set_bandwidth(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),type,value);
}
void belle_sdp_session_description_set_bandwidths(belle_sdp_session_description_t* session_description, belle_sip_list_t* bandwidths) {
	belle_sdp_base_description_set_bandwidths(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),bandwidths);
}
void belle_sdp_session_description_add_bandwidth(belle_sdp_session_description_t* session_description, const belle_sdp_bandwidth_t* bandwidth) {
	belle_sdp_base_description_add_bandwidth(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),bandwidth);
}
void belle_sdp_session_description_set_connection(belle_sdp_session_description_t* session_description, belle_sdp_connection_t* connection) {
	SET_OBJECT(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),connection,belle_sdp_connection_t)
}
void belle_sdp_session_description_set_emails(belle_sdp_session_description_t* session_description, belle_sip_list_t* emails) {
	SET_LIST(session_description->emails,emails)
}
void belle_sdp_session_description_set_info(belle_sdp_session_description_t* session_description, belle_sdp_info_t* info) {
	SET_OBJECT(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),info,belle_sdp_info_t)
}
/*void belle_sdp_session_description_set_key(belle_sdp_session_description_t* session_description, belle_sdp_key_t* key);*/
void belle_sdp_session_description_set_media_descriptions(belle_sdp_session_description_t* session_description, belle_sip_list_t* media_descriptions) {
	SET_LIST(session_description->media_descriptions,media_descriptions)
}
void belle_sdp_session_description_add_media_description(belle_sdp_session_description_t* session_description, belle_sdp_media_description_t* media_description) {
	session_description->media_descriptions = belle_sip_list_append(session_description->media_descriptions,belle_sip_object_ref(media_description));
}

void belle_sdp_session_description_set_origin(belle_sdp_session_description_t* session_description, belle_sdp_origin_t* origin) {
	SET_OBJECT(session_description,origin,belle_sdp_origin_t)
}
void belle_sdp_session_description_set_phones(belle_sdp_session_description_t* session_description, belle_sip_list_t* phones) {
	SET_LIST(session_description->phones,phones)
}
void belle_sdp_session_description_set_session_name(belle_sdp_session_description_t* session_description, belle_sdp_session_name_t* session_name) {
	SET_OBJECT(session_description,session_name,belle_sdp_session_name_t)
}
void belle_sdp_session_description_set_time_descriptions(belle_sdp_session_description_t* session_description, belle_sip_list_t* times) {
	SET_LIST(session_description->times,times)
}
void belle_sdp_session_description_set_time_description(belle_sdp_session_description_t* session_description, belle_sdp_time_description_t* time_desc) {
	belle_sdp_session_description_set_time_descriptions(session_description,belle_sip_list_new(time_desc));
}
void belle_sdp_session_description_set_uri(belle_sdp_session_description_t* session_description, belle_sdp_uri_t* uri) {
	SET_OBJECT(session_description,uri,belle_sdp_uri_t)
}
void belle_sdp_session_description_set_version(belle_sdp_session_description_t* session_description, belle_sdp_version_t* version) {
	SET_OBJECT(session_description,version,belle_sdp_version_t)
}
void belle_sdp_session_description_set_zone_adjustments(belle_sdp_session_description_t* session_description, belle_sdp_uri_t* zone_adjustments) {
	SET_OBJECT(session_description,zone_adjustments,belle_sdp_uri_t)
}
/************************
 * time
 ***********************/
struct _belle_sdp_time {
	belle_sip_object_t base;
	int start;
	int stop;
 };

void belle_sdp_time_destroy(belle_sdp_time_t* time) {

}

void belle_sdp_time_clone(belle_sdp_time_t *time, const belle_sdp_time_t *orig){
	time->start=orig->start;
	time->stop=orig->stop;
}

belle_sip_error_code belle_sdp_time_marshal(belle_sdp_time_t* time, char* buff, size_t buff_size, size_t *offset) {
	return belle_sip_snprintf(buff,buff_size,offset,"%i %i",time->start,time->stop);
}

BELLE_SDP_NEW(time,belle_sip_object)
//BELLE_SDP_PARSE(version)
GET_SET_INT(belle_sdp_time,start,int);
GET_SET_INT(belle_sdp_time,stop,int);

/************************
 * time description
 ***********************/
struct _belle_sdp_time_description {
	belle_sip_object_t base;
	belle_sdp_time_t* time;

 };

void belle_sdp_time_description_destroy(belle_sdp_time_description_t* time_description) {
	if (time_description->time) belle_sip_object_unref(BELLE_SIP_OBJECT(time_description->time));
}

void belle_sdp_time_description_clone(belle_sdp_time_description_t *time_description, const belle_sdp_time_description_t *orig){
	if (orig->time) time_description->time = BELLE_SDP_TIME(belle_sip_object_clone_and_ref(BELLE_SIP_OBJECT(orig->time)));
}

belle_sip_error_code belle_sdp_time_description_marshal(belle_sdp_time_description_t* time_description, char* buff, size_t buff_size, size_t *offset) {
	return belle_sip_object_marshal(BELLE_SIP_OBJECT(time_description->time),buff,buff_size,offset);
}

BELLE_SDP_NEW(time_description,belle_sip_object)

belle_sdp_time_description_t* belle_sdp_time_description_create (int start,int stop) {
	belle_sdp_time_description_t* time_desc= belle_sdp_time_description_new();
	belle_sdp_time_t* time = belle_sdp_time_new();
	belle_sdp_time_set_start(time,start);
	belle_sdp_time_set_stop(time,stop);
	belle_sdp_time_description_set_time(time_desc,time);
	return time_desc;
}
belle_sip_list_t* belle_sdp_time_description_get_repeate_times(const belle_sdp_time_description_t* time_description) {
	return NULL;
}
belle_sdp_time_t* belle_sdp_time_description_get_time(const belle_sdp_time_description_t* time_description) {
	return time_description->time;
}
void belle_sdp_time_description_set_repeate_times(belle_sdp_time_description_t* time_description, belle_sip_list_t* times) {
	belle_sip_error("time description repeat time not implemented");
}
void belle_sdp_time_description_set_time(belle_sdp_time_description_t* time_description, belle_sdp_time_t* time) {
	SET_OBJECT(time_description,time,belle_sdp_time_t)
}
#define BELLE_SDP_TIME_DESCRIPTION(t) BELLE_SDP_CAST(t,belle_sdp_time_description_t);

/************************
 * version
 ***********************/
struct _belle_sdp_version {
	belle_sip_object_t base;
	int version;
 };

void belle_sdp_version_destroy(belle_sdp_version_t* version) {

}

void belle_sdp_version_clone(belle_sdp_version_t *version, const belle_sdp_version_t *orig){
	version->version = orig->version;
}

belle_sip_error_code belle_sdp_version_marshal(belle_sdp_version_t* version, char* buff, size_t buff_size, size_t *offset) {
	return belle_sip_snprintf(buff,buff_size,offset,"v=%i",version->version);
}

BELLE_SDP_NEW(version,belle_sip_object)
belle_sdp_version_t* belle_sdp_version_create(int version) {
	belle_sdp_version_t* v = belle_sdp_version_new();
	belle_sdp_version_set_version(v,version);
	return v;
}
//BELLE_SDP_PARSE(version)
GET_SET_INT(belle_sdp_version,version,int);

/***************************************************************************************
 * mime_parameter
 *
 **************************************************************************************/
struct _belle_sdp_mime_parameter {
	belle_sip_object_t base;
	int rate;
	int channel_count;
	int ptime;
	int max_ptime;
	int media_format;
	const char* type;
	const char* parameters;

};
static void belle_sdp_mime_parameter_destroy(belle_sdp_mime_parameter_t *mime_parameter) {
	if (mime_parameter->type) belle_sip_free((void*)mime_parameter->type);
	if (mime_parameter->parameters) belle_sip_free((void*)mime_parameter->parameters);
}
static void belle_sdp_mime_parameter_clone(belle_sdp_mime_parameter_t *mime_parameter,belle_sdp_mime_parameter_t *orig) {
	mime_parameter->rate = orig->rate;
	mime_parameter->channel_count = orig->channel_count;
	mime_parameter->ptime = orig->ptime;
	mime_parameter->max_ptime = orig->max_ptime;
	mime_parameter->media_format = orig->media_format;
	CLONE_STRING(belle_sdp_mime_parameter,type,mime_parameter,orig);
	CLONE_STRING(belle_sdp_mime_parameter,parameters,mime_parameter,orig);
}
BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sdp_mime_parameter_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sdp_mime_parameter_t
							,belle_sip_object_t
							,belle_sdp_mime_parameter_destroy
							,belle_sdp_mime_parameter_clone
							,NULL
							,TRUE);

belle_sdp_mime_parameter_t* belle_sdp_mime_parameter_new() {
	belle_sdp_mime_parameter_t* l_param = belle_sip_object_new(belle_sdp_mime_parameter_t);
	l_param->ptime = -1;
	l_param->max_ptime = -1;
	return l_param;
}
belle_sdp_mime_parameter_t* belle_sdp_mime_parameter_create(const char* type, int media_format, int rate,int channel_count) {
	belle_sdp_mime_parameter_t* mime_param= belle_sdp_mime_parameter_new();
	belle_sdp_mime_parameter_set_type(mime_param,type);
	belle_sdp_mime_parameter_set_media_format(mime_param,media_format);
	belle_sdp_mime_parameter_set_rate(mime_param,rate);
	belle_sdp_mime_parameter_set_channel_count(mime_param,channel_count);
	return mime_param;
}
GET_SET_INT(belle_sdp_mime_parameter,rate,int);
GET_SET_INT(belle_sdp_mime_parameter,channel_count,int);
GET_SET_INT(belle_sdp_mime_parameter,ptime,int);
GET_SET_INT(belle_sdp_mime_parameter,max_ptime,int);
GET_SET_INT(belle_sdp_mime_parameter,media_format,int);
GET_SET_STRING(belle_sdp_mime_parameter,type);
GET_SET_STRING(belle_sdp_mime_parameter,parameters);

