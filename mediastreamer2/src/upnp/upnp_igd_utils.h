/*******************************************************************************
 *
 * Copyright (c) 2000-2003 Intel Corporation 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met: 
 *
 * - Redistributions of source code must retain the above copyright notice, 
 * this list of conditions and the following disclaimer. 
 * - Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation 
 * and/or other materials provided with the distribution. 
 * - Neither name of Intel Corporation nor the names of its contributors 
 * may be used to endorse or promote products derived from this software 
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef _UPNP_IGD_UTILS_H__
#define _UPNP_IGD_UTILS_H__

#include "mediastreamer2/upnp_igd.h"
#include <upnp.h>

char *upnp_igd_strncpy(char * destination, const char * source, size_t num);

void upnp_igd_print(upnp_igd_context *uIGD, upnp_igd_print_level level, const char *fmt, ...);
void upnp_igd_print_event_type(upnp_igd_context *uIGD, upnp_igd_print_level level, Upnp_EventType S);
void upnp_igd_print_event(upnp_igd_context *uIGD, upnp_igd_print_level level, Upnp_EventType EventType, void *Event);
char *upnp_igd_get_first_document_item(upnp_igd_context *uIGD, IXML_Document *doc, const char *item);
char *upnp_igd_get_element_value(upnp_igd_context *uIGD, IXML_Element *element);
char *upnp_igd_get_first_element_item(upnp_igd_context *uIGD,IXML_Element *element, const char *item);
int upnp_igd_get_find_and_parse_service(upnp_igd_context *uIGD, IXML_Document *DescDoc, const char *location,
		const char *serviceType, char **serviceId, char **eventURL, char **controlURL);
#endif //_UPNP_IGD_UTILS_H__
