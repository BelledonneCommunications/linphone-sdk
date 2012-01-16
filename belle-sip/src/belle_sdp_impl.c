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
#include "belle-sip/belle-sdp.h"
#include "belle_sip_internal.h"
#include "belle_sdpParser.h"
#include "belle_sdpLexer.h"

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
}

void belle_sdp_attribute_clone(belle_sdp_attribute_t *attribute, const belle_sdp_attribute_t *orig){
}
int belle_sdp_attribute_marshal(belle_sdp_attribute_t* attribute, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"a=%s"
								,attribute->name);
	if (attribute->value) {
		current_offset+=snprintf(	buff+current_offset
									,buff_size-current_offset
									,":%s"
									,attribute->value);
	}
	return current_offset-offset;
}
BELLE_SDP_NEW(attribute,belle_sip_object)
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
}

void belle_sdp_bandwidth_clone(belle_sdp_bandwidth_t *bandwidth, const belle_sdp_bandwidth_t *orig){
}
int belle_sdp_bandwidth_marshal(belle_sdp_bandwidth_t* bandwidth, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"b=%s:%i"
								,bandwidth->type,bandwidth->value);

	return current_offset-offset;
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
}

void belle_sdp_connection_clone(belle_sdp_connection_t *connection, const belle_sdp_connection_t *orig){
}
int belle_sdp_connection_marshal(belle_sdp_connection_t* connection, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"c=%s %s %s"
								,connection->network_type
								,connection->address_type
								,connection->address);
	return current_offset-offset;
}
BELLE_SDP_NEW(connection,belle_sip_object)
BELLE_SDP_PARSE(connection)
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
}

void belle_sdp_email_clone(belle_sdp_email_t *email, const belle_sdp_email_t *orig){
}
int belle_sdp_email_marshal(belle_sdp_email_t* email, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"e=%s"
								,email->value);
	return current_offset-offset;
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
}

void belle_sdp_info_clone(belle_sdp_info_t *info, const belle_sdp_info_t *orig){
}
int belle_sdp_info_marshal(belle_sdp_info_t* info, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"i=%s"
								,info->value);
	return current_offset-offset;
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
	media->media_formats = formats;
}
void belle_sdp_media_destroy(belle_sdp_media_t* media) {
}
static void belle_sdp_media_init(belle_sdp_media_t* media) {
	media->port_count=1;
}

void belle_sdp_media_clone(belle_sdp_media_t *media, const belle_sdp_media_t *orig){
}
int belle_sdp_media_marshal(belle_sdp_media_t* media, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	belle_sip_list_t* list=media->media_formats;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"m=%s %i"
								,media->media_type
								,media->media_port
								);
	if (media->port_count>1) {
		current_offset+=snprintf(buff+current_offset
								,buff_size-current_offset
								,"/%i"
								,media->port_count);
	}
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								," %s"
								,media->protocol);
	for(;list!=NULL;list=list->next){
		current_offset+=snprintf(	buff+current_offset
									,buff_size-current_offset
									," %li"
									,(long)list->data);
	}
	return current_offset-offset;
}
BELLE_SDP_NEW_WITH_CTR(media,belle_sip_object)
BELLE_SDP_PARSE(media)
GET_SET_STRING(belle_sdp_media,media_type);
GET_SET_STRING(belle_sdp_media,protocol);
GET_SET_INT(belle_sdp_media,media_port,int)
GET_SET_INT(belle_sdp_media,port_count,int)

/************************
 * base_description
 ***********************/

typedef struct _belle_sdp_base_description {
	belle_sip_object_t base;
	belle_sdp_info_t* info;
	belle_sdp_connection_t* connection;
	belle_sip_list_t* bandwidths;
	belle_sip_list_t* attributes;
} belle_sdp_base_description_t;

void belle_sdp_base_description_destroy(belle_sdp_base_description_t* base_description) {
}
void belle_sdp_base_description_init(belle_sdp_base_description_t* base_description) {
}
void belle_sdp_base_description_clone(belle_sdp_base_description_t *base_description, const belle_sdp_base_description_t *orig){
}
int belle_sdp_base_description_marshal(belle_sdp_base_description_t* base_description, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	belle_sip_list_t* bandwidths;
	belle_sip_list_t* attributes;
	if (base_description->info) {
		current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(base_description->info),buff,current_offset,buff_size);
		current_offset+=snprintf(buff+current_offset, buff_size-current_offset, "\r\n");
	}
	if (base_description->connection) {
		current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(base_description->connection),buff,current_offset,buff_size);
		current_offset+=snprintf(buff+current_offset, buff_size-current_offset, "\r\n");
	}
	for(bandwidths=base_description->bandwidths;bandwidths!=NULL;bandwidths=bandwidths->next){
		current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(bandwidths->data),buff,current_offset,buff_size);
		current_offset+=snprintf(buff+current_offset, buff_size-current_offset, "\r\n");
	}
	for(attributes=base_description->attributes;attributes!=NULL;attributes=attributes->next){
		current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(attributes->data),buff,current_offset,buff_size);
		current_offset+=snprintf(buff+current_offset, buff_size-current_offset, "\r\n");
	}
	return current_offset-offset;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sdp_base_description_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sdp_base_description_t,belle_sip_object_t,belle_sdp_base_description_destroy,NULL,belle_sdp_base_description_marshal);

static int belle_sdp_base_description_attribute_comp_func(const belle_sdp_attribute_t* a, const char*b) {
	return strcmp(a->name,b);
}
const char*	belle_sdp_base_description_get_attribute(const belle_sdp_base_description_t* base_description, const char* name) {
	belle_sip_list_t* attribute;
	attribute = belle_sip_list_find_custom(base_description->attributes, (belle_sip_compare_func)belle_sdp_base_description_attribute_comp_func, name);
	if (attribute) {
		return ((belle_sdp_attribute_t*)attribute->data)->value;
	} else {
		return NULL;
	}
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
void belle_sdp_base_description_set_attribute(belle_sdp_base_description_t* base_description, const char* name, const char* value) {
	belle_sdp_attribute_t* attribute = belle_sdp_attribute_new();
	belle_sdp_attribute_set_name(attribute,name);
	belle_sdp_attribute_set_value(attribute,value);
	base_description->attributes = belle_sip_list_append(base_description->attributes,attribute);
}
void belle_sdp_base_description_add_attribute(belle_sdp_base_description_t* base_description, const belle_sdp_attribute_t* attribute) {
	base_description->attributes = belle_sip_list_append(base_description->attributes,(void*)attribute);
}

#define SET_LIST(list_name,value) \
		belle_sip_list_t* list;\
		if (list_name) {\
			for (list=list_name;list !=NULL; list=list->next) {\
				belle_sip_object_unref(BELLE_SIP_OBJECT(list->data));\
			}\
			belle_sip_list_free(list_name); \
		} \
		list_name=value;


void belle_sdp_base_description_set_attributes(belle_sdp_base_description_t* base_description, belle_sip_list_t* attributes) {
	SET_LIST(base_description->attributes,attributes)
}
void belle_sdp_base_description_set_bandwidth(belle_sdp_base_description_t* base_description, const char* type, int value) {
	belle_sdp_bandwidth_t* bandwidth = belle_sdp_bandwidth_new();
	belle_sdp_bandwidth_set_type(bandwidth,type);
	belle_sdp_bandwidth_set_value(bandwidth,value);
	base_description->bandwidths = belle_sip_list_append(base_description->bandwidths,bandwidth);
}
void belle_sdp_base_description_add_bandwidth(belle_sdp_base_description_t* base_description, const belle_sdp_bandwidth_t* bandwidth) {
	base_description->bandwidths = belle_sip_list_append(base_description->bandwidths,(void *)bandwidth);
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
}

void belle_sdp_media_description_clone(belle_sdp_media_description_t *media_description, const belle_sdp_media_description_t *orig){
}
int belle_sdp_media_description_marshal(belle_sdp_media_description_t* media_description, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(media_description->media),buff,current_offset,buff_size);
	current_offset+=snprintf(buff+current_offset, buff_size-current_offset, "\r\n");
	current_offset+=belle_sdp_base_description_marshal(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),buff,current_offset,buff_size);
	return current_offset-offset;
}
BELLE_SDP_NEW(media_description,belle_sdp_base_description)
BELLE_SDP_PARSE(media_description)
void belle_sdp_media_description_add_dynamic_payloads(belle_sdp_media_description_t* media_description, belle_sip_list_t* payloadNames, belle_sip_list_t* payloadValues) {

}
const char*	belle_sdp_media_description_get_attribute(const belle_sdp_media_description_t* media_description, const char* name) {
	return belle_sdp_base_description_get_attribute(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),name);
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
struct static_payload static_payload_list [STATIC_PAYLOAD_LIST_LENTH] ={
	{0,1,"PCMU",8000},
	{3,1,"GSM",8000},
	{4,1,"G723",8000},
	{5,1,"DVI4",8000},
	{6,1,"DVI4",16000},
	{8,1,"PCMA",8000},
	{9,1,"G722",8000},
	{34,-1,"H263",90000}
};
static int mime_parameter_fill_from_static(belle_sdp_mime_parameter_t *mime_parameter,int format) {
	struct static_payload* iterator = static_payload_list;
	int i;
	for (i=0;i<STATIC_PAYLOAD_LIST_LENTH;i++) {
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
	ptime = belle_sdp_media_description_get_attribute(media_description,"ptime");
	ptime?ptime_as_int=atoi(ptime):-1;
	max_ptime = belle_sdp_media_description_get_attribute(media_description,"maxptime");
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
#define MAX_FMTP_LENGH 64

void belle_sdp_media_description_append_values_from_mime_parameter(belle_sdp_media_description_t* media_description, belle_sdp_mime_parameter_t* mime_parameter) {
	belle_sdp_media_t* media = belle_sdp_media_description_get_media(media_description);
	char atribute_value [MAX_FMTP_LENGH];
	belle_sdp_media_set_media_formats(media,belle_sip_list_append(belle_sdp_media_get_media_formats(media)
																,(void*)(long)(belle_sdp_mime_parameter_get_media_format(mime_parameter))));
	if (belle_sdp_mime_parameter_get_media_format(mime_parameter) > 34) {
		/*dynamic payload*/

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
		belle_sdp_media_description_set_attribute(media_description,"rtpmap",atribute_value);
		if (belle_sdp_mime_parameter_get_parameters(mime_parameter)) {
			snprintf(atribute_value,MAX_FMTP_LENGH,"%i %s"
					,belle_sdp_mime_parameter_get_media_format(mime_parameter)
					,belle_sdp_mime_parameter_get_parameters(mime_parameter));
			belle_sdp_media_description_set_attribute(media_description,"fmtp",atribute_value);
		}

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
void belle_sdp_media_description_set_attribute(belle_sdp_media_description_t* media_description, const char* name, const char* value) {
	belle_sdp_base_description_set_attribute(BELLE_SIP_CAST(media_description,belle_sdp_base_description_t),name,value);
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

void belle_sdp_media_description_set_connection(belle_sdp_media_description_t* media_description, belle_sdp_connection_t* conn) {
	belle_sdp_connection_t** current = &BELLE_SIP_CAST(media_description,belle_sdp_base_description_t)->connection;
	if (*current) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(*current));
	}
	*current=conn;
}
void belle_sdp_media_description_set_info(belle_sdp_media_description_t* media_description,belle_sdp_info_t* i) {
	belle_sdp_info_t** current = &BELLE_SIP_CAST(media_description,belle_sdp_base_description_t)->info;
	if (*current) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(*current));
	}
	*current=i;
}
/*void belle_sdp_media_description_set_key(belle_sdp_media_description_t* media_description,belle_sdp_key_t* key);*/
void belle_sdp_media_description_set_media(belle_sdp_media_description_t* media_description, belle_sdp_media_t* media) {
	belle_sdp_media_t** current = &media_description->media;
	if (*current) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(*current));
	}
	*current=media;
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
	int session_id;
	int session_version;

 };

void belle_sdp_origin_destroy(belle_sdp_origin_t* origin) {
}

void belle_sdp_origin_clone(belle_sdp_origin_t *origin, const belle_sdp_origin_t *orig){
}
int belle_sdp_origin_marshal(belle_sdp_origin_t* origin, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
									,buff_size-current_offset
									,"o=%s %i %i %s %s %s"
									,origin->username
									,origin->session_id
									,origin->session_version
									,origin->network_type
									,origin->address_type
									,origin->address);
	return current_offset-offset;
}
BELLE_SDP_NEW(origin,belle_sip_object)
BELLE_SDP_PARSE(origin)
GET_SET_STRING(belle_sdp_origin,username);
GET_SET_STRING(belle_sdp_origin,address);
GET_SET_STRING(belle_sdp_origin,address_type);
GET_SET_STRING(belle_sdp_origin,network_type);
GET_SET_INT(belle_sdp_origin,session_id,int);
GET_SET_INT(belle_sdp_origin,session_version,int);
/************************
 * session_name
 ***********************/
struct _belle_sdp_session_name {
	belle_sip_object_t base;
	const char* value;
 };

void belle_sdp_session_name_destroy(belle_sdp_session_name_t* session_name) {
}

void belle_sdp_session_name_clone(belle_sdp_session_name_t *session_name, const belle_sdp_session_name_t *orig){
}
int belle_sdp_session_name_marshal(belle_sdp_session_name_t* session_name, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"s=%s"
								,session_name->value);
	return current_offset-offset;
}
BELLE_SDP_NEW(session_name,belle_sip_object)
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
}

void belle_sdp_session_description_clone(belle_sdp_session_description_t *session_description, const belle_sdp_session_description_t *orig){
}
int belle_sdp_session_description_marshal(belle_sdp_session_description_t* session_description, char* buff,unsigned int offset,unsigned int buff_size) {
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
	unsigned int current_offset=offset;
	belle_sip_list_t* media_descriptions;
	belle_sip_list_t* times;

	current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(session_description->version),buff,current_offset,buff_size);
	current_offset+=snprintf(buff+current_offset, buff_size-current_offset, "\r\n");

	current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(session_description->origin),buff,current_offset,buff_size);
	current_offset+=snprintf(buff+current_offset, buff_size-current_offset, "\r\n");

	current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(session_description->session_name),buff,current_offset,buff_size);
	current_offset+=snprintf(buff+current_offset, buff_size-current_offset, "\r\n");

	current_offset+=belle_sdp_base_description_marshal((belle_sdp_base_description_t*)(&session_description->base_description),buff,current_offset,buff_size);

	current_offset+=snprintf(buff+current_offset, buff_size-current_offset, "t=");
	for(times=session_description->times;times!=NULL;times=times->next){
		current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(times->data),buff,current_offset,buff_size);
		current_offset+=snprintf(buff+current_offset, buff_size-current_offset, "\r\n");
	}

	for(media_descriptions=session_description->media_descriptions;media_descriptions!=NULL;media_descriptions=media_descriptions->next){
		current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(media_descriptions->data),buff,current_offset,buff_size);
	}

	return current_offset-offset;
}
BELLE_SDP_NEW(session_description,belle_sdp_base_description)
BELLE_SDP_PARSE(session_description)

const char*	belle_sdp_session_description_get_attribute(const belle_sdp_session_description_t* session_description, const char* name) {
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
void belle_sdp_session_description_set_attribute(belle_sdp_session_description_t* session_description, const char* name, const char* value) {
	belle_sdp_base_description_set_attribute(BELLE_SIP_CAST(session_description,belle_sdp_base_description_t),name,value);
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
void belle_sdp_session_description_set_connection(belle_sdp_session_description_t* session_description, belle_sdp_connection_t* conn) {
	belle_sdp_connection_t** current = &BELLE_SIP_CAST(session_description,belle_sdp_base_description_t)->connection;
	if (*current) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(*current));
	}
	*current=conn;
}
void belle_sdp_session_description_set_emails(belle_sdp_session_description_t* session_description, belle_sip_list_t* emails) {
	SET_LIST(session_description->emails,emails)
}
void belle_sdp_session_description_set_info(belle_sdp_session_description_t* session_description, belle_sdp_info_t* i) {
	belle_sdp_info_t** current = &BELLE_SIP_CAST(session_description,belle_sdp_base_description_t)->info;
	if (*current) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(*current));
	}
	*current=i;
}
/*void belle_sdp_session_description_set_key(belle_sdp_session_description_t* session_description, belle_sdp_key_t* key);*/
void belle_sdp_session_description_set_media_descriptions(belle_sdp_session_description_t* session_description, belle_sip_list_t* media_descriptions) {
	SET_LIST(session_description->media_descriptions,media_descriptions)
}
void belle_sdp_session_description_add_media_description(belle_sdp_session_description_t* session_description, belle_sdp_media_description_t* media_description) {
	session_description->media_descriptions = belle_sip_list_append(session_description->media_descriptions,media_description);
}

void belle_sdp_session_description_set_origin(belle_sdp_session_description_t* session_description, belle_sdp_origin_t* origin) {
	belle_sdp_origin_t** current = &session_description->origin;
	if (*current) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(*current));
	}
	*current=origin;
}
void belle_sdp_session_description_set_phones(belle_sdp_session_description_t* session_description, belle_sip_list_t* phones) {
	SET_LIST(session_description->phones,phones)
}
void belle_sdp_session_description_set_session_name(belle_sdp_session_description_t* session_description, belle_sdp_session_name_t* session_name) {
	belle_sdp_session_name_t** current = &session_description->session_name;
	if (*current) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(*current));
	}
	*current=session_name;
}
void belle_sdp_session_description_set_time_descriptions(belle_sdp_session_description_t* session_description, belle_sip_list_t* times) {
	SET_LIST(session_description->times,times)
}
void belle_sdp_session_description_set_uri(belle_sdp_session_description_t* session_description, belle_sdp_uri_t* uri) {
	belle_sdp_uri_t** current = &session_description->uri;
	if (*current) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(*current));
	}
	*current=uri;
}
void belle_sdp_session_description_set_version(belle_sdp_session_description_t* session_description, belle_sdp_version_t* version) {
	belle_sdp_version_t** current = &session_description->version;
	if (*current) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(*current));
	}
	*current=version;
}
void belle_sdp_session_description_set_zone_adjustments(belle_sdp_session_description_t* session_description, belle_sdp_uri_t* zone_adjustments) {
	belle_sdp_uri_t** current = &session_description->zone_adjustments;
	if (*current) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(*current));
	}
	*current=zone_adjustments;
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
}
int belle_sdp_time_marshal(belle_sdp_time_t* time, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"%i %i"
								,time->start
								,time->stop);
	return current_offset-offset;
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
}

void belle_sdp_time_description_clone(belle_sdp_time_description_t *time_description, const belle_sdp_time_description_t *orig){
}
int belle_sdp_time_description_marshal(belle_sdp_time_description_t* time_description, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(time_description->time),buff,current_offset,buff_size);
	return current_offset-offset;
}
BELLE_SDP_NEW(time_description,belle_sip_object)

belle_sip_list_t* belle_sdp_time_description_get_repeate_times(const belle_sdp_time_description_t* time_description) {
	return NULL;
}
belle_sdp_time_t* belle_sdp_time_description_get_time(const belle_sdp_time_description_t* time_description) {
	return time_description->time;
}
void belle_sdp_time_description_set_repeate_times(belle_sdp_time_description_t* time_description, belle_sip_list_t* times) {
	belle_sip_error("time description repeat time not implemented");
}
void belle_sdp_time_description_set_time(belle_sdp_time_description_t* time_description, belle_sdp_time_t* value) {
	belle_sdp_time_t** current = &time_description->time;
	if (*current) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(*current));
	}
	*current=value;
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

}
int belle_sdp_version_marshal(belle_sdp_version_t* version, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"v=%i"
								,version->version);
	return current_offset-offset;
}
BELLE_SDP_NEW(version,belle_sip_object)
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

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sdp_mime_parameter_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sdp_mime_parameter_t,belle_sip_object_t,belle_sdp_mime_parameter_destroy,NULL,NULL);

belle_sdp_mime_parameter_t* belle_sdp_mime_parameter_new() {
	belle_sdp_mime_parameter_t* l_param = belle_sip_object_new(belle_sdp_mime_parameter_t);
	l_param->ptime = -1;
	l_param->max_ptime = -1;
	return l_param;
}
GET_SET_INT(belle_sdp_mime_parameter,rate,int);
GET_SET_INT(belle_sdp_mime_parameter,channel_count,int);
GET_SET_INT(belle_sdp_mime_parameter,ptime,int);
GET_SET_INT(belle_sdp_mime_parameter,max_ptime,int);
GET_SET_INT(belle_sdp_mime_parameter,media_format,int);
GET_SET_STRING(belle_sdp_mime_parameter,type);
GET_SET_STRING(belle_sdp_mime_parameter,parameters);

