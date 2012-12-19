#ifndef _UPNP_IGD_UTILS_H__
#define _UPNP_IGD_UTILS_H__

#include "upnp_igd.h"
#include <upnp.h>

void upnp_igd_print(upnp_igd_context *uIGD, upnp_igd_print_level level, const char *fmt, ...);
void upnp_igd_print_event_type(upnp_igd_context *uIGD, upnp_igd_print_level level, Upnp_EventType S);
void upnp_igd_print_event(upnp_igd_context *uIGD, upnp_igd_print_level level, Upnp_EventType EventType, void *Event);
char *upnp_igd_get_first_document_item(upnp_igd_context *uIGD, IXML_Document *doc, const char *item);
char *upnp_igd_get_element_value(upnp_igd_context *uIGD, IXML_Element *element);
char *upnp_igd_get_first_element_item(upnp_igd_context *uIGD,IXML_Element *element, const char *item);
int upnp_igd_get_find_and_parse_service(upnp_igd_context *uIGD, IXML_Document *DescDoc, const char *location,
		const char *serviceType, char **serviceId, char **eventURL, char **controlURL);
#endif //_UPNP_IGD_UTILS_H__
