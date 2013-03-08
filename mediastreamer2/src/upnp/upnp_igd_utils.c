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

#include "upnp_igd_utils.h"
#include "upnp_igd_private.h"
#include <upnptools.h>
#include <string.h>
#include <stdlib.h>

char *upnp_igd_strncpy(char * destination, const char * source, size_t num) {
	char * ret = strncpy(destination, source, num - 1);
	destination[num - 1] = '\0';
	return ret;
}

void upnp_context_add_client(upnp_igd_context *igd_ctxt) {
	ithread_mutex_lock(&igd_ctxt->client_mutex);
	igd_ctxt->client_count++;
	ithread_mutex_unlock(&igd_ctxt->client_mutex);
}

void upnp_context_remove_client(upnp_igd_context *igd_ctxt) {
	ithread_mutex_lock(&igd_ctxt->client_mutex);
	igd_ctxt->client_count--;
	if(igd_ctxt->client_count == 0) {
		ithread_cond_signal(&igd_ctxt->client_cond);
	}
	ithread_mutex_unlock(&igd_ctxt->client_mutex);
}

void upnp_context_add_callback(upnp_igd_context *igd_ctxt, upnp_igd_event event, void *arg) {
	upnp_igd_callback_event_node *node, *list;
	if(igd_ctxt->callback_fct != NULL) {
		// Create the node
		node = (upnp_igd_callback_event_node *) malloc(sizeof(upnp_igd_callback_event_node));
		node->event.event = event;
		node->event.arg = arg;
		node->next = NULL;

		ithread_mutex_lock(&igd_ctxt->callback_mutex);
		// Append to the list
		if(igd_ctxt->callback_events == NULL) {
			igd_ctxt->callback_events = node;
		} else {
			list = igd_ctxt->callback_events;
			while(list->next != NULL) list = list->next;
			list->next = node;
		}
		ithread_mutex_unlock(&igd_ctxt->callback_mutex);
	}
}

void upnp_context_handle_callbacks(upnp_igd_context *igd_ctxt) {
	upnp_igd_callback_event_node *node;
	if(igd_ctxt->callback_fct != NULL) {
		ithread_mutex_lock(&igd_ctxt->callback_mutex);
		while(igd_ctxt->callback_events != NULL) {
			node = igd_ctxt->callback_events;
			igd_ctxt->callback_events = igd_ctxt->callback_events->next;	
			ithread_mutex_unlock(&igd_ctxt->callback_mutex);
		
			// Callback
			igd_ctxt->callback_fct(igd_ctxt->cookie, node->event.event, node->event.arg);
			free(node);
			
			ithread_mutex_lock(&igd_ctxt->callback_mutex);
		}
		ithread_mutex_unlock(&igd_ctxt->callback_mutex);
	}
}

void upnp_context_free_callbacks(upnp_igd_context *igd_ctxt) {
	upnp_igd_callback_event_node *node;
	if(igd_ctxt->callback_fct != NULL) {
		ithread_mutex_lock(&igd_ctxt->callback_mutex);
		while(igd_ctxt->callback_events != NULL) {
			node = igd_ctxt->callback_events;
			igd_ctxt->callback_events = node->next;	
			free(node);
		}
		ithread_mutex_unlock(&igd_ctxt->callback_mutex);
	}
}

void upnp_igd_print(upnp_igd_context *igd_ctxt, upnp_igd_print_level level, const char *fmt, ...) {
	va_list ap;

	/* Protect both the display and the static buffer with the mutex */
	ithread_mutex_lock(&igd_ctxt->print_mutex);

	va_start(ap, fmt);
	if (igd_ctxt->print_fct) {
		igd_ctxt->print_fct(igd_ctxt->cookie, level, fmt, ap);
	}
	va_end(ap);

	ithread_mutex_unlock(&igd_ctxt->print_mutex);
}

void upnp_igd_print_event_type(upnp_igd_context *igd_ctxt, upnp_igd_print_level level, Upnp_EventType S) {
	switch (S) {
	/* Discovery */
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		upnp_igd_print(igd_ctxt, level, "UPNP_DISCOVERY_ADVERTISEMENT_ALIVE");
		break;
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
		upnp_igd_print(igd_ctxt, level, "UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE");
		break;
	case UPNP_DISCOVERY_SEARCH_RESULT:
		upnp_igd_print(igd_ctxt, level,  "UPNP_DISCOVERY_SEARCH_RESULT");
		break;
	case UPNP_DISCOVERY_SEARCH_TIMEOUT:
		upnp_igd_print(igd_ctxt, level,  "UPNP_DISCOVERY_SEARCH_TIMEOUT");
		break;
	/* SOAP */
	case UPNP_CONTROL_ACTION_REQUEST:
		upnp_igd_print(igd_ctxt, level, "UPNP_CONTROL_ACTION_REQUEST");
		break;
	case UPNP_CONTROL_ACTION_COMPLETE:
		upnp_igd_print(igd_ctxt, level, "UPNP_CONTROL_ACTION_COMPLETE");
		break;
	case UPNP_CONTROL_GET_VAR_REQUEST:
		upnp_igd_print(igd_ctxt, level, "UPNP_CONTROL_GET_VAR_REQUEST");
		break;
	case UPNP_CONTROL_GET_VAR_COMPLETE:
		upnp_igd_print(igd_ctxt, level, "UPNP_CONTROL_GET_VAR_COMPLETE");
		break;
	/* GENA */
	case UPNP_EVENT_SUBSCRIPTION_REQUEST:
		upnp_igd_print(igd_ctxt, level, "UPNP_EVENT_SUBSCRIPTION_REQUEST");
		break;
	case UPNP_EVENT_RECEIVED:
		upnp_igd_print(igd_ctxt, level, "UPNP_EVENT_RECEIVED");
		break;
	case UPNP_EVENT_RENEWAL_COMPLETE:
		upnp_igd_print(igd_ctxt, level, "UPNP_EVENT_RENEWAL_COMPLETE");
		break;
	case UPNP_EVENT_SUBSCRIBE_COMPLETE:
		upnp_igd_print(igd_ctxt, level, "UPNP_EVENT_SUBSCRIBE_COMPLETE");
		break;
	case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
		upnp_igd_print(igd_ctxt, level, "UPNP_EVENT_UNSUBSCRIBE_COMPLETE");
		break;
	case UPNP_EVENT_AUTORENEWAL_FAILED:
		upnp_igd_print(igd_ctxt, level, "UPNP_EVENT_AUTORENEWAL_FAILED");
		break;
	case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
		upnp_igd_print(igd_ctxt, level, "UPNP_EVENT_SUBSCRIPTION_EXPIRED");
		break;
	}
}

void upnp_igd_print_event(upnp_igd_context *igd_ctxt, upnp_igd_print_level level, Upnp_EventType EventType, void *Event) {
	ithread_mutex_lock(&igd_ctxt->print_mutex);

	upnp_igd_print(igd_ctxt, level, "======================================================================");
	upnp_igd_print_event_type(igd_ctxt, level, EventType);
	switch (EventType) {
	/* SSDP */
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
	case UPNP_DISCOVERY_SEARCH_RESULT: {
		struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)Event;

		upnp_igd_print(igd_ctxt, level, "ErrCode     =  %s(%d)",
			UpnpGetErrorMessage(d_event->ErrCode), d_event->ErrCode);
		upnp_igd_print(igd_ctxt, level, "Expires     =  %d",  d_event->Expires);
		upnp_igd_print(igd_ctxt, level, "DeviceId    =  %s",  d_event->DeviceId);
		upnp_igd_print(igd_ctxt, level, "DeviceType  =  %s",  d_event->DeviceType);
		upnp_igd_print(igd_ctxt, level, "ServiceType =  %s",  d_event->ServiceType);
		upnp_igd_print(igd_ctxt, level, "ServiceVer  =  %s",  d_event->ServiceVer);
		upnp_igd_print(igd_ctxt, level, "Location    =  %s",  d_event->Location);
		upnp_igd_print(igd_ctxt, level, "OS          =  %s",  d_event->Os);
		upnp_igd_print(igd_ctxt, level, "Ext         =  %s",  d_event->Ext);
		break;
	}
	case UPNP_DISCOVERY_SEARCH_TIMEOUT:
		/* Nothing to print out here */
		break;
	/* SOAP */
	case UPNP_CONTROL_ACTION_REQUEST: {
		struct Upnp_Action_Request *a_event =
			(struct Upnp_Action_Request *)Event;
		char *xmlbuff = NULL;

		upnp_igd_print(igd_ctxt, level, "ErrCode     =  %s(%d)",
			UpnpGetErrorMessage(a_event->ErrCode), a_event->ErrCode);
		upnp_igd_print(igd_ctxt, level, "ErrStr      =  %s", a_event->ErrStr);
		upnp_igd_print(igd_ctxt, level, "ActionName  =  %s", a_event->ActionName);
		upnp_igd_print(igd_ctxt, level, "UDN         =  %s", a_event->DevUDN);
		upnp_igd_print(igd_ctxt, level, "ServiceID   =  %s", a_event->ServiceID);
		if (a_event->ActionRequest) {
			xmlbuff = ixmlPrintNode((IXML_Node *)a_event->ActionRequest);
			if (xmlbuff) {
				upnp_igd_print(igd_ctxt, level, "ActRequest  =  %s", xmlbuff);
				ixmlFreeDOMString(xmlbuff);
			}
			xmlbuff = NULL;
		} else {
			upnp_igd_print(igd_ctxt, level, "ActRequest  =  (null)");
		}
		if (a_event->ActionResult) {
			xmlbuff = ixmlPrintNode((IXML_Node *)a_event->ActionResult);
			if (xmlbuff) {
				upnp_igd_print(igd_ctxt, level, "ActResult   =  %s", xmlbuff);
				ixmlFreeDOMString(xmlbuff);
			}
			xmlbuff = NULL;
		} else {
			upnp_igd_print(igd_ctxt, level, "ActResult   =  (null)");
		}
		break;
	}
	case UPNP_CONTROL_ACTION_COMPLETE: {
		struct Upnp_Action_Complete *a_event =
			(struct Upnp_Action_Complete *)Event;
		char *xmlbuff = NULL;

		upnp_igd_print(igd_ctxt, level, "ErrCode     =  %s(%d)",
			UpnpGetErrorMessage(a_event->ErrCode), a_event->ErrCode);
		upnp_igd_print(igd_ctxt, level, "CtrlUrl     =  %s", a_event->CtrlUrl);
		if (a_event->ActionRequest) {
			xmlbuff = ixmlPrintNode((IXML_Node *)a_event->ActionRequest);
			if (xmlbuff) {
				upnp_igd_print(igd_ctxt, level, "ActRequest  =  %s", xmlbuff);
				ixmlFreeDOMString(xmlbuff);
			}
			xmlbuff = NULL;
		} else {
			upnp_igd_print(igd_ctxt, level, "ActRequest  =  (null)");
		}
		if (a_event->ActionResult) {
			xmlbuff = ixmlPrintNode((IXML_Node *)a_event->ActionResult);
			if (xmlbuff) {
				upnp_igd_print(igd_ctxt, level, "ActResult   =  %s", xmlbuff);
				ixmlFreeDOMString(xmlbuff);
			}
			xmlbuff = NULL;
		} else {
			upnp_igd_print(igd_ctxt, level, "ActResult   =  (null)");
		}
		break;
	}
	case UPNP_CONTROL_GET_VAR_REQUEST: {
		struct Upnp_State_Var_Request *sv_event =
			(struct Upnp_State_Var_Request *)Event;

		upnp_igd_print(igd_ctxt, level, "ErrCode     =  %s(%d)",
			UpnpGetErrorMessage(sv_event->ErrCode), sv_event->ErrCode);
		upnp_igd_print(igd_ctxt, level, "ErrStr      =  %s", sv_event->ErrStr);
		upnp_igd_print(igd_ctxt, level, "UDN         =  %s", sv_event->DevUDN);
		upnp_igd_print(igd_ctxt, level, "ServiceID   =  %s", sv_event->ServiceID);
		upnp_igd_print(igd_ctxt, level, "StateVarName=  %s", sv_event->StateVarName);
		upnp_igd_print(igd_ctxt, level, "CurrentVal  =  %s", sv_event->CurrentVal);
		break;
	}
	case UPNP_CONTROL_GET_VAR_COMPLETE: {
		struct Upnp_State_Var_Complete *sv_event =
			(struct Upnp_State_Var_Complete *)Event;

		upnp_igd_print(igd_ctxt, level, "ErrCode     =  %s(%d)",
			UpnpGetErrorMessage(sv_event->ErrCode), sv_event->ErrCode);
		upnp_igd_print(igd_ctxt, level, "CtrlUrl     =  %s", sv_event->CtrlUrl);
		upnp_igd_print(igd_ctxt, level, "StateVarName=  %s", sv_event->StateVarName);
		upnp_igd_print(igd_ctxt, level, "CurrentVal  =  %s", sv_event->CurrentVal);
		break;
	}
	/* GENA */
	case UPNP_EVENT_SUBSCRIPTION_REQUEST: {
		struct Upnp_Subscription_Request *sr_event =
			(struct Upnp_Subscription_Request *)Event;

		upnp_igd_print(igd_ctxt, level, "ServiceID   =  %s", sr_event->ServiceId);
		upnp_igd_print(igd_ctxt, level, "UDN         =  %s", sr_event->UDN);
		upnp_igd_print(igd_ctxt, level, "SID         =  %s", sr_event->Sid);
		break;
	}
	case UPNP_EVENT_RECEIVED: {
		struct Upnp_Event *e_event = (struct Upnp_Event *)Event;
		char *xmlbuff = NULL;

		upnp_igd_print(igd_ctxt, level, "SID         =  %s", e_event->Sid);
		upnp_igd_print(igd_ctxt, level, "EventKey    =  %d",	e_event->EventKey);
		xmlbuff = ixmlPrintNode((IXML_Node *)e_event->ChangedVariables);
		upnp_igd_print(igd_ctxt, level, "ChangedVars =  %s", xmlbuff);
		ixmlFreeDOMString(xmlbuff);
		xmlbuff = NULL;
		break;
	}
	case UPNP_EVENT_RENEWAL_COMPLETE: {
		struct Upnp_Event_Subscribe *es_event =
			(struct Upnp_Event_Subscribe *)Event;

		upnp_igd_print(igd_ctxt, level, "SID         =  %s", es_event->Sid);
		upnp_igd_print(igd_ctxt, level, "ErrCode     =  %s(%d)",
			UpnpGetErrorMessage(es_event->ErrCode), es_event->ErrCode);
		upnp_igd_print(igd_ctxt, level, "TimeOut     =  %d", es_event->TimeOut);
		break;
	}
	case UPNP_EVENT_SUBSCRIBE_COMPLETE:
	case UPNP_EVENT_UNSUBSCRIBE_COMPLETE: {
		struct Upnp_Event_Subscribe *es_event =
			(struct Upnp_Event_Subscribe *)Event;

		upnp_igd_print(igd_ctxt, level, "SID         =  %s", es_event->Sid);
		upnp_igd_print(igd_ctxt, level, "ErrCode     =  %s(%d)",
			UpnpGetErrorMessage(es_event->ErrCode), es_event->ErrCode);
		upnp_igd_print(igd_ctxt, level, "PublisherURL=  %s", es_event->PublisherUrl);
		upnp_igd_print(igd_ctxt, level, "TimeOut     =  %d", es_event->TimeOut);
		break;
	}
	case UPNP_EVENT_AUTORENEWAL_FAILED:
	case UPNP_EVENT_SUBSCRIPTION_EXPIRED: {
		struct Upnp_Event_Subscribe *es_event =
			(struct Upnp_Event_Subscribe *)Event;

		upnp_igd_print(igd_ctxt, level, "SID         =  %s", es_event->Sid);
		upnp_igd_print(igd_ctxt, level, "ErrCode     =  %s(%d)",
			UpnpGetErrorMessage(es_event->ErrCode), es_event->ErrCode);
		upnp_igd_print(igd_ctxt, level, "PublisherURL=  %s", es_event->PublisherUrl);
		upnp_igd_print(igd_ctxt, level, "TimeOut     =  %d", es_event->TimeOut);
		break;
	}
	}
	upnp_igd_print(igd_ctxt, level,"======================================================================");

	ithread_mutex_unlock(&igd_ctxt->print_mutex);
}


char *upnp_igd_get_first_document_item(upnp_igd_context *igd_ctxt, IXML_Document *doc, const char *item) {
	IXML_NodeList *nodeList = NULL;
	IXML_Node *textNode = NULL;
	IXML_Node *tmpNode = NULL;
	char *ret = NULL;

	nodeList = ixmlDocument_getElementsByTagName(doc, (char *)item);
	if (nodeList) {
		tmpNode = ixmlNodeList_item(nodeList, 0);
		if (tmpNode) {
			textNode = ixmlNode_getFirstChild(tmpNode);
			if (!textNode) {
				upnp_igd_print(igd_ctxt, UPNP_IGD_WARNING, "%s(%d): (BUG) ixmlNode_getFirstChild(tmpNode) returned NULL",
					__FILE__, __LINE__);
				ret = strdup("");
				goto epilogue;
			}
			ret = strdup(ixmlNode_getNodeValue(textNode));
			if (!ret) {
				upnp_igd_print(igd_ctxt, UPNP_IGD_WARNING, "%s(%d): ixmlNode_getNodeValue returned NULL",
					__FILE__, __LINE__);
				ret = strdup("");
			}
		} else
			upnp_igd_print(igd_ctxt, UPNP_IGD_WARNING, "%s(%d): ixmlNodeList_item(nodeList, 0) returned NULL",
				__FILE__, __LINE__);
	} else
		upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "%s(%d): Error finding %s in XML Node",
			__FILE__, __LINE__, item);

epilogue:
	if (nodeList)
		ixmlNodeList_free(nodeList);

	return ret;
}

char *upnp_igd_get_element_value(upnp_igd_context *igd_ctxt, IXML_Element *element)
{
	IXML_Node *child = ixmlNode_getFirstChild((IXML_Node *)element);
	char *temp = NULL;

	if (child != 0 && ixmlNode_getNodeType(child) == eTEXT_NODE)
		temp = strdup(ixmlNode_getNodeValue(child));

	return temp;
}


char *upnp_igd_get_first_element_item(upnp_igd_context *igd_ctxt,IXML_Element *element, const char *item)
{
	IXML_NodeList *nodeList = NULL;
	IXML_Node *textNode = NULL;
	IXML_Node *tmpNode = NULL;
	char *ret = NULL;

	nodeList = ixmlElement_getElementsByTagName(element, (char *)item);
	if (nodeList == NULL) {
		upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "%s(%d): Error finding %s in XML Node",
			__FILE__, __LINE__, item);
		return NULL;
	}
	tmpNode = ixmlNodeList_item(nodeList, 0);
	if (!tmpNode) {
		upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "%s(%d): Error finding %s value in XML Node",
			__FILE__, __LINE__, item);
		ixmlNodeList_free(nodeList);
		return NULL;
	}
	textNode = ixmlNode_getFirstChild(tmpNode);
	ret = strdup(ixmlNode_getNodeValue(textNode));
	if (!ret) {
		upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "%s(%d): Error allocating memory for %s in XML Node",
			__FILE__, __LINE__, item);
		ixmlNodeList_free(nodeList);
		return NULL;
	}
	ixmlNodeList_free(nodeList);

	return ret;
}


IXML_NodeList *upnp_igd_get_nth_service_list(upnp_igd_context *igd_ctxt,
	/*! [in] . */
	IXML_Document *doc,
	/*! [in] . */
	unsigned int n) {
	IXML_NodeList *ServiceList = NULL;
	IXML_NodeList *servlistnodelist = NULL;
	IXML_Node *servlistnode = NULL;

	/*  ixmlDocument_getElementsByTagName()
	 *  Returns a NodeList of all Elements that match the given
	 *  tag name in the order in which they were encountered in a preorder
	 *  traversal of the Document tree.
	 *
	 *  return (NodeList*) A pointer to a NodeList containing the
	 *                      matching items or NULL on an error. 	 */
	servlistnodelist = ixmlDocument_getElementsByTagName(doc, "serviceList");
	if (servlistnodelist && ixmlNodeList_length(servlistnodelist) && n < ixmlNodeList_length(servlistnodelist)) {
		/* For the first service list (from the root device),
		 * we pass 0 */
		/*servlistnode = ixmlNodeList_item( servlistnodelist, 0 );*/

		/* Retrieves a Node from a NodeList} specified by a
		 *  numerical index.
		 *
		 *  return (Node*) A pointer to a Node or NULL if there was an
		 *                  error. */
		servlistnode = ixmlNodeList_item(servlistnodelist, n);
		if (servlistnode) {
			/* create as list of DOM nodes */
			ServiceList = ixmlElement_getElementsByTagName((IXML_Element *)servlistnode, "service");
		} else {
			upnp_igd_print(igd_ctxt, UPNP_IGD_WARNING, "%s(%d): ixmlNodeList_item(nodeList, n) returned NULL", __FILE__, __LINE__);
		}
	}
	if (servlistnodelist) {
		ixmlNodeList_free(servlistnodelist);
	}

	return ServiceList;
}

int upnp_igd_get_find_and_parse_service(upnp_igd_context *igd_ctxt, IXML_Document *DescDoc, const char *location,
		const char *serviceType, char **serviceId, char **eventURL, char **controlURL) {
	unsigned int i;
	unsigned long length;
	int found = 0;
	int ret;
	unsigned int sindex = 0;
	char *tempServiceType = NULL;
	char *baseURL = NULL;
	const char *base = NULL;
	char *relcontrolURL = NULL;
	char *releventURL = NULL;
	IXML_NodeList *serviceList = NULL;
	IXML_Element *service = NULL;
	baseURL = upnp_igd_get_first_document_item(igd_ctxt, DescDoc, "URLBase");
	if (baseURL)
		base = baseURL;
	else
		base = location;
	for (sindex = 0;
	     (serviceList = upnp_igd_get_nth_service_list(igd_ctxt, DescDoc , sindex)) != NULL;
	     sindex++) {
		tempServiceType = NULL;
		relcontrolURL = NULL;
		releventURL = NULL;
		service = NULL;
		length = ixmlNodeList_length(serviceList);
		for (i = 0; i < length; i++) {
			service = (IXML_Element *)ixmlNodeList_item(serviceList, i);
			tempServiceType = upnp_igd_get_first_element_item(igd_ctxt,
				(IXML_Element *)service, "serviceType");
			if (tempServiceType && strcmp(tempServiceType, serviceType) == 0) {
				upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "Found service: %s", serviceType);
				*serviceId = upnp_igd_get_first_element_item(igd_ctxt, service, "serviceId");
				upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "serviceId: %s", *serviceId);
				relcontrolURL = upnp_igd_get_first_element_item(igd_ctxt, service, "controlURL");
				releventURL = upnp_igd_get_first_element_item(igd_ctxt, service, "eventSubURL");
				*controlURL = malloc(strlen(base) + strlen(relcontrolURL) + 1);
				if (*controlURL) {
					ret = UpnpResolveURL(base, relcontrolURL, *controlURL);
					if (ret != UPNP_E_SUCCESS)
						upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error generating controlURL from %s + %s",
							base, relcontrolURL);
				}
				*eventURL = malloc(strlen(base) + strlen(releventURL) + 1);
				if (*eventURL) {
					ret = UpnpResolveURL(base, releventURL, *eventURL);
					if (ret != UPNP_E_SUCCESS)
						upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error generating eventURL from %s + %s",
							base, releventURL);
				}
				free(relcontrolURL);
				free(releventURL);
				relcontrolURL = NULL;
				releventURL = NULL;
				found = 1;
				break;
			}
			free(tempServiceType);
			tempServiceType = NULL;
		}
		free(tempServiceType);
		tempServiceType = NULL;
		if (serviceList)
			ixmlNodeList_free(serviceList);
		serviceList = NULL;
	}
	free(baseURL);

	return found;
}

