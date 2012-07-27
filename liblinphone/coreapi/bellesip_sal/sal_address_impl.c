/*
linphone
Copyright (C) 2012  Belledonne Communications, Grenoble, France

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
#include "sal_impl.h"
/**/
/* Address manipulation API*/
SalAddress * sal_address_new(const char *uri){
	belle_sip_header_address_t*  result;
	if (uri) {
		return (SalAddress *)belle_sip_header_address_parse (uri);
	} else {
		result = belle_sip_header_address_new();
		belle_sip_header_address_set_uri(result,belle_sip_uri_new());
		return (SalAddress *)result;
	}
}
SalAddress * sal_address_clone(const SalAddress *addr){
	return (SalAddress *) belle_sip_object_clone(BELLE_SIP_OBJECT(addr));
}
const char *sal_address_get_scheme(const SalAddress *addr){
	belle_sip_header_address_t* header_addr = BELLE_SIP_HEADER_ADDRESS(addr);
	belle_sip_uri_t* uri = belle_sip_header_address_get_uri(header_addr);
	if (uri) {
		if (belle_sip_uri_is_secure(uri)) return "sips";
		else return "sip";
	} else
		return NULL;
}
const char *sal_address_get_display_name(const SalAddress* addr){
	belle_sip_header_address_t* header_addr = BELLE_SIP_HEADER_ADDRESS(addr);
	return belle_sip_header_address_get_displayname(header_addr);

}
const char *sal_address_get_display_name_unquoted(const SalAddress *addr){
	return sal_address_get_display_name(addr);
}
#define SAL_ADDRESS_GET(addr,param) \
belle_sip_header_address_t* header_addr = BELLE_SIP_HEADER_ADDRESS(addr);\
belle_sip_uri_t* uri = belle_sip_header_address_get_uri(header_addr);\
if (uri) {\
	return belle_sip_uri_get_##param(uri);\
} else\
	return NULL;

#define SAL_ADDRESS_SET(addr,param,value) \
belle_sip_header_address_t* header_addr = BELLE_SIP_HEADER_ADDRESS(addr);\
belle_sip_uri_t* uri = belle_sip_header_address_get_uri(header_addr);\
belle_sip_uri_set_##param(uri,value);

const char *sal_address_get_username(const SalAddress *addr){
	SAL_ADDRESS_GET(addr,user)
}
const char *sal_address_get_domain(const SalAddress *addr){
	SAL_ADDRESS_GET(addr,host)
}
const char * sal_address_get_port(const SalAddress *addr){
	ms_fatal("sal_address_get_port not implemented yet");
	return NULL;
}
int sal_address_get_port_int(const SalAddress *addr){
	belle_sip_header_address_t* header_addr = BELLE_SIP_HEADER_ADDRESS(addr);
	belle_sip_uri_t* uri = belle_sip_header_address_get_uri(header_addr);
	if (uri) {
		return belle_sip_uri_get_port(uri);
	} else
		return -1;
}
SalTransport sal_address_get_transport(const SalAddress* addr){
	belle_sip_header_address_t* header_addr = BELLE_SIP_HEADER_ADDRESS(addr);
	belle_sip_uri_t* uri = belle_sip_header_address_get_uri(header_addr);
	if (uri) {
		return sal_transport_parse(belle_sip_uri_get_transport_param(uri));
	} else
		return SalTransportUDP;
};

void sal_address_set_display_name(SalAddress *addr, const char *display_name){
	belle_sip_header_address_t* header_addr = BELLE_SIP_HEADER_ADDRESS(addr);
	belle_sip_header_address_set_displayname(header_addr,display_name);
}
void sal_address_set_username(SalAddress *addr, const char *username){
	SAL_ADDRESS_SET(addr,user,username);
}
void sal_address_set_domain(SalAddress *addr, const char *host){
	SAL_ADDRESS_SET(addr,host,host);
}
void sal_address_set_port(SalAddress *addr, const char *port){
	SAL_ADDRESS_SET(addr,port,atoi(port));
}
void sal_address_set_port_int(SalAddress *addr, int port){
	SAL_ADDRESS_SET(addr,port,port);
}
void sal_address_clean(SalAddress *addr){
	belle_sip_header_address_t* header_addr = BELLE_SIP_HEADER_ADDRESS(addr);
	belle_sip_parameters_clean(BELLE_SIP_PARAMETERS(header_addr));
	return ;
}
char *sal_address_as_string(const SalAddress *addr){
	return belle_sip_object_to_string(BELLE_SIP_OBJECT(addr));
}
char *sal_address_as_string_uri_only(const SalAddress *addr){
	belle_sip_header_address_t* header_addr = BELLE_SIP_HEADER_ADDRESS(addr);
	belle_sip_uri_t* uri = belle_sip_header_address_get_uri(header_addr);
	return belle_sip_object_to_string(BELLE_SIP_OBJECT(uri));
}
void sal_address_destroy(SalAddress *addr){
	belle_sip_header_address_t* header_addr = BELLE_SIP_HEADER_ADDRESS(addr);
	belle_sip_object_unref(header_addr);
	return ;
}
void sal_address_set_param(SalAddress *addr,const char* name,const char* value){
	belle_sip_parameters_t* parameters = BELLE_SIP_PARAMETERS(addr);
	belle_sip_parameters_set_parameter(parameters,name,value);
	return ;
}
void sal_address_set_transport(SalAddress* addr,SalTransport transport){
	SAL_ADDRESS_SET(addr,transport_param,sal_transport_to_string(transport));
}


