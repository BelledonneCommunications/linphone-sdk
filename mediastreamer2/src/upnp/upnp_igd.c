#include "upnp_igd.h"
#include "upnp_igd_utils.h"
#include "upnp_igd_private.h"

#include <stdlib.h>
#include <upnp.h>
#include <upnptools.h>
#include <ixml.h>
#include <ithread.h>
#include <errno.h>
#include <sys/time.h>

const char *IGDDeviceType = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";

const char *IGDServiceType[] = {
	"urn:schemas-upnp-org:service:WANIPConnection:1",
};

const char *IGDServiceName[] = {
	"WANIPConnection"
};

const char *IGDVarName[IGD_SERVICE_SERVCOUNT][IGD_MAXVARS] = {
    {
    		"ExternalIPAddress",
    		"NATEnabled",
    		"ConnectionStatus"
    }
};

char IGDVarCount[IGD_SERVICE_SERVCOUNT] =  {
	IGD_SERVICE_WANIPCONNECTION_VARCOUNT
};

int IGDTimeOut[IGD_SERVICE_SERVCOUNT] = {
	1801
};


/********************************************************************************
 * upnp_igd_delete_node
 *
 * Description:
 *       Delete a device node from the context device list.  Note that this
 *       function is NOT thread safe, and should be called from another
 *       function that has already locked the global device list.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *   node     -- The device node
 *
 ********************************************************************************/
int upnp_igd_delete_node(upnp_igd_context *igd_ctxt, upnp_igd_device_node *node) {
	int rc, service, var;

	if (NULL == node) {
		upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "upnp_igd_delete_node: Node is empty\n");
		return 0;
	}

	upnp_igd_print(igd_ctxt, UPNP_IGD_MESSAGE, "Remove IGD device: %s[%s]\n", node->device.friendly_name, node->device.udn);

	for (service = 0; service < IGD_SERVICE_SERVCOUNT; service++) {
		/*
		   If we have a valid control SID, then unsubscribe
		 */
		if (strcmp(node->device.services[service].sid, "") != 0) {
			rc = UpnpUnSubscribe(igd_ctxt->upnp_handle, node->device.services[service].sid);
			if (UPNP_E_SUCCESS == rc) {
				upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "Unsubscribed from IGD %s EventURL with SID=%s\n", IGDServiceName[service], node->device.services[service].sid);
			} else {
				upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error unsubscribing to IGD %s EventURL -- %d\n", IGDServiceName[service], rc);
			}
		}

		for (var = 0; var < IGDVarCount[service]; var++) {
			if (node->device.services[service].variables[var]) {
				free(node->device.services[service].variables[var]);
			}
		}
	}

	free(node);
	node = NULL;

	if(igd_ctxt->callback_fct != NULL) {
		igd_ctxt->callback_fct(igd_ctxt->cookie, UPNP_IGD_DEVICE_REMOVED, NULL);
	}

	return 0;
}


/********************************************************************************
 * upnp_igd_remove_device
 *
 * Description:
 *       Remove a device from the context device list.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *   udn -- The Unique Device Name for the device to remove
 *
 ********************************************************************************/
int upnp_igd_remove_device(upnp_igd_context *igd_ctxt, const char *udn) {
	upnp_igd_device_node *curdevnode, *prevdevnode;

	ithread_mutex_lock(&igd_ctxt->devices_mutex);

	curdevnode = igd_ctxt->devices;
	if (!curdevnode) {
		upnp_igd_print(igd_ctxt, UPNP_IGD_WARNING, "upnp_igd_remove_device: Device list empty\n");
	} else {
		if (0 == strcmp(curdevnode->device.udn, udn)) {
			igd_ctxt->devices = curdevnode->next;
			upnp_igd_delete_node(igd_ctxt, curdevnode);
		} else {
			prevdevnode = curdevnode;
			curdevnode = curdevnode->next;
			while (curdevnode) {
				if (strcmp(curdevnode->device.udn, udn) == 0) {
					prevdevnode->next = curdevnode->next;
					upnp_igd_delete_node(igd_ctxt, curdevnode);
					break;
				}
				prevdevnode = curdevnode;
				curdevnode = curdevnode->next;
			}
		}
	}

	ithread_mutex_unlock(&igd_ctxt->devices_mutex);

	return 0;
}


/********************************************************************************
 * upnp_igd_remove_all
 *
 * Description:
 *       Remove all devices from the context device list.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *
 ********************************************************************************/
int upnp_igd_remove_all(upnp_igd_context *igd_ctxt) {
	upnp_igd_device_node *curdevnode, *next;

	ithread_mutex_lock(&igd_ctxt->devices_mutex);

	curdevnode = igd_ctxt->devices;
	igd_ctxt->devices = NULL;

	while (curdevnode) {
		next = curdevnode->next;
		upnp_igd_delete_node(igd_ctxt, curdevnode);
		curdevnode = next;
	}

	ithread_mutex_unlock(&igd_ctxt->devices_mutex);

	return 0;
}


/********************************************************************************
 * upnp_igd_verify_timeouts
 *
 * Description:
 *       Check all the device for refreshing which ones are close to timeout.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *   incr     -- Number of second before next check
 *
 ********************************************************************************/
void upnp_igd_verify_timeouts(upnp_igd_context *igd_ctxt, int incr) {
	upnp_igd_device_node *prevdevnode, *curdevnode;
	int ret;

	ithread_mutex_lock(&igd_ctxt->devices_mutex);

	prevdevnode = NULL;
	curdevnode = igd_ctxt->devices;
	while (curdevnode) {
		curdevnode->device.advr_time_out -= incr;
		upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "IGD device: %s[%s] | Advertisement Timeout: %d\n",
				curdevnode->device.friendly_name,
				curdevnode->device.udn,
				curdevnode->device.advr_time_out);
		if (curdevnode->device.advr_time_out <= 0) {
			/* This advertisement has expired, so we should remove the device
			 * from the list */
			if (igd_ctxt->devices == curdevnode)
				igd_ctxt->devices = curdevnode->next;
			else
				prevdevnode->next = curdevnode->next;
			upnp_igd_delete_node(igd_ctxt, curdevnode);
			if (prevdevnode)
				curdevnode = prevdevnode->next;
			else
				curdevnode = igd_ctxt->devices;
		} else {
			if (curdevnode->device.advr_time_out < 2 * incr) {
				/* This advertisement is about to expire, so
				 * send out a search request for this device
				 * UDN to try to renew */
				ret = UpnpSearchAsync(igd_ctxt->upnp_handle, incr, curdevnode->device.udn, igd_ctxt);
				if (ret != UPNP_E_SUCCESS)
					upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error sending search request for Device UDN: %s -- err = %d\n",
					     curdevnode->device.udn, ret);
			}
			prevdevnode = curdevnode;
			curdevnode = curdevnode->next;
		}
	}

	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
}


/********************************************************************************
 * upnp_igd_timer_loop
 *
 * Description:
 *       Thread function which check the timeouts.
 *
 * Parameters:
 *   args -- The upnp igd context
 *
 ********************************************************************************/
void *upnp_igd_timer_loop(void *args) {
	upnp_igd_context *igd_ctxt = (upnp_igd_context*)args;
	struct timespec ts;
	struct timeval tp;

	/* how often to verify the timeouts, in seconds */
	static int incr = 30;

	// Update timeout
	gettimeofday(&tp, NULL);
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000;
    ts.tv_sec += incr;
	ithread_mutex_lock(&igd_ctxt->timer_mutex);
	while(ithread_cond_timedwait(&igd_ctxt->timer_cond, &igd_ctxt->timer_mutex, &ts) == ETIMEDOUT) {
		upnp_igd_verify_timeouts(igd_ctxt, incr);

		// Update timeout
		gettimeofday(&tp, NULL);
	    ts.tv_sec  = tp.tv_sec;
	    ts.tv_nsec = tp.tv_usec * 1000;
	    ts.tv_sec += incr;
		ithread_mutex_lock(&igd_ctxt->timer_mutex);
	}

	return NULL;
}


/********************************************************************************
 * upnp_igd_get_var
 *
 * Description:
 *       Send a GetVar request to the specified service of a device.
 *
 * Parameters:
 *   igd_ctxt    -- The upnp igd context
 *   device_node -- The device
 *   service     -- The service
 *   variable    -- The variable to request.
 *   fun         -- Callback function
 *   cookie      -- Callback cookie
 *
 ********************************************************************************/
int upnp_igd_get_var(upnp_igd_context* igd_ctxt, upnp_igd_device_node *device_node, int service, int variable,
		Upnp_FunPtr fun, const void *cookie) {
	int ret;
	upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "Get %s.%s from IGD device %s[%s]\n",
			IGDServiceName[service],
			IGDVarName[service][variable],
			device_node->device.friendly_name,
			device_node->device.udn);

	ret = UpnpGetServiceVarStatusAsync(igd_ctxt->upnp_handle,
			device_node->device.services[service].control_url,
			IGDVarName[service][variable],
			fun,
			cookie);
	if (ret != UPNP_E_SUCCESS) {
		upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error in UpnpGetServiceVarStatusAsync -- %d\n", ret);
		ret = -1;
	}

	return 0;
}


/********************************************************************************
 * upnp_igd_send_action
 *
 * Description:
 *       Send an Action request to the specified service of a device.
 *
 * Parameters:
 *   igd_ctxt    -- The upnp igd context
 *   device_node -- The device
 *   service     -- The service
 *   actionname  -- The name of the action.
 *   param_name  -- An array of parameter names
 *   param_val   -- The corresponding parameter values
 *   param_count -- The number of parameters
 *   fun         -- Callback function
 *   cookie      -- Callback cookie
 *
 ********************************************************************************/
int upnp_igd_send_action(upnp_igd_context* igd_ctxt, upnp_igd_device_node *device_node, int service,
		const char *actionname, const char **param_name, const char **param_val, int param_count,
		Upnp_FunPtr fun, const void *cookie) {
	IXML_Document *actionNode = NULL;
	int ret = 0;
	int param;
	if (0 == param_count) {
		actionNode = UpnpMakeAction(actionname, IGDServiceType[service], 0, NULL);
	} else {
		for (param = 0; param < param_count; param++) {
			if (UpnpAddToAction(&actionNode, actionname, IGDServiceType[service], param_name[param], param_val[param]) != UPNP_E_SUCCESS) {
				upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "ERROR: TvCtrlPointSendAction: Trying to add action param\n");
				/*return -1; // TBD - BAD! leaves mutex locked */
			}
		}
	}

	ret = UpnpSendActionAsync(igd_ctxt->upnp_handle, device_node->device.services[service].control_url,
				 IGDServiceType[service], NULL, actionNode, fun, cookie);

	if (ret != UPNP_E_SUCCESS) {
		upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error in UpnpSendActionAsync -- %d\n", ret);
		ret = -1;
	}

	if (actionNode)
		ixmlDocument_free(actionNode);

	return ret;
}


/********************************************************************************
 * upnp_igd_add_device
 *
 * Description:
 *       If the device is not already included in the global device list,
 *       add it.  Otherwise, update its advertisement expiration timeout.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *   desc_doc -- The description document for the device
 *   d_event  -- event associated with the new device
 *
 ********************************************************************************/
void upnp_igd_add_device(upnp_igd_context *igd_ctxt, IXML_Document *desc_doc, struct Upnp_Discovery *d_event) {
	upnp_igd_device_node *deviceNode, *tmpdevnode;
	int found = 0;
	int ret;
	int service, var;
	char presURL[200];

	char *serviceId;
	char *event_url;
	char *controlURL;
	Upnp_SID eventSID;

	char *deviceType = NULL;
	char *friendlyName = NULL;
	char *baseURL = NULL;
	char *relURL = NULL;
	char *UDN = NULL;

	UDN = upnp_igd_get_first_document_item(igd_ctxt, desc_doc, "UDN");
	deviceType = upnp_igd_get_first_document_item(igd_ctxt, desc_doc, "deviceType");
	friendlyName = upnp_igd_get_first_document_item(igd_ctxt, desc_doc, "friendlyName");
	baseURL = upnp_igd_get_first_document_item(igd_ctxt, desc_doc, "URLBase");
	relURL = upnp_igd_get_first_document_item(igd_ctxt, desc_doc, "presentationURL");

	ret = UpnpResolveURL((baseURL ? baseURL : d_event->Location), relURL, presURL);

	if (UPNP_E_SUCCESS != ret) {
		upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error generating presURL from %s + %s\n", baseURL, relURL);
	}

	ithread_mutex_lock(&igd_ctxt->devices_mutex);

	if (strcmp(deviceType, IGDDeviceType) == 0) {
			upnp_igd_print(igd_ctxt, UPNP_IGD_MESSAGE, "Found IGD device: %s[%s]\n", friendlyName, UDN);

			/* Check if this device is already in the list */
			tmpdevnode = igd_ctxt->devices;
			while (tmpdevnode) {
				if (strcmp(tmpdevnode->device.udn, UDN) == 0) {
					found = 1;
					break;
				}
				tmpdevnode = tmpdevnode->next;
			}
			if (found) {
				/* The device is already there, so just update  */
				/* the advertisement timeout field */
				tmpdevnode->device.advr_time_out = d_event->Expires;
				upnp_igd_print(igd_ctxt, UPNP_IGD_MESSAGE, "IGD device: %s[%s] | Update expires(%d)\n", friendlyName, UDN, tmpdevnode->device.advr_time_out);
			} else {
				/* Create a new device node */
				deviceNode = (upnp_igd_device_node *)  malloc(sizeof(upnp_igd_device_node));
				memset(deviceNode->device.services, '\0', sizeof(upnp_igd_service) * IGD_SERVICE_SERVCOUNT);
				strcpy(deviceNode->device.udn, UDN);
				strcpy(deviceNode->device.desc_doc_url, d_event->Location);
				strcpy(deviceNode->device.friendly_name, friendlyName);
				strcpy(deviceNode->device.pres_url, presURL);
				deviceNode->device.advr_time_out = d_event->Expires;

				// Reset values
				serviceId = NULL;
				event_url = NULL;
				controlURL = NULL;
				eventSID[0] = '\0';

				for (service = 0; service < IGD_SERVICE_SERVCOUNT;
				     service++) {
					if (upnp_igd_get_find_and_parse_service(igd_ctxt, desc_doc, d_event->Location,
							IGDServiceType[service], &serviceId, &event_url, &controlURL)) {
						upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "Subscribing to EventURL %s...\n",event_url);
						ret =
						    UpnpSubscribe(igd_ctxt->upnp_handle, event_url, &IGDTimeOut[service], eventSID);
						if (ret == UPNP_E_SUCCESS) {
							upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "Subscribed to EventURL with SID=%s\n", eventSID);
						} else {
							upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error Subscribing to EventURL -- %d\n", ret);
							strcpy(eventSID, "");
						}
					} else {
						upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Could not find Service: %s\n", IGDServiceType[service]);
					}
					if(serviceId != NULL)
						strcpy(deviceNode->device.services[service].service_id, serviceId);
					strcpy(deviceNode->device.services[service].service_type, IGDServiceName[service]);
					if(controlURL != NULL)
						strcpy(deviceNode->device.services[service].control_url, controlURL);
					if(event_url != NULL)
						strcpy(deviceNode->device.services[service].event_url, event_url);
					if(eventSID != NULL)
						strcpy(deviceNode->device.services[service].sid, eventSID);
					for (var = 0; var < IGDVarCount[service]; var++) {
						deviceNode->device.services[service].variables[var] = (char *)malloc(IGD_MAX_VAL_LEN);
						strcpy(deviceNode->device.services[service].variables[var], "");
					}
				}

				deviceNode->next = NULL;
				/* Insert the new device node in the list */
				if ((tmpdevnode = igd_ctxt->devices)) {
					while (tmpdevnode) {
						if (tmpdevnode->next) {
							tmpdevnode = tmpdevnode->next;
						} else {
							tmpdevnode->next = deviceNode;
							break;
						}
					}
				} else {
					igd_ctxt->devices = deviceNode;
				}

				upnp_igd_print(igd_ctxt, UPNP_IGD_MESSAGE, "Add IGD device: %s[%s]\n", friendlyName, UDN);
				if(igd_ctxt->callback_fct != NULL) {
					igd_ctxt->callback_fct(igd_ctxt->cookie, UPNP_IGD_DEVICE_ADDED, NULL);
				}

				if (serviceId)
					free(serviceId);
				if (controlURL)
					free(controlURL);
				if (event_url)
					free(event_url);

				// Ask some details
				upnp_igd_send_action(igd_ctxt, deviceNode, IGD_SERVICE_WANIPCONNECTION, "GetNATRSIPStatus", NULL, NULL, 0, upnp_igd_callback, igd_ctxt);
			}
	}

	ithread_mutex_unlock(&igd_ctxt->devices_mutex);

	if (deviceType)
		free(deviceType);
	if (friendlyName)
		free(friendlyName);
	if (UDN)
		free(UDN);
	if (baseURL)
		free(baseURL);
	if (relURL)
		free(relURL);
}


/********************************************************************************
 * upnp_igd_refresh
 *
 * Description:
 *       Clear the current context device list and issue new search
 *	 requests to build it up again from scratch.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *
 ********************************************************************************/
int upnp_igd_refresh(upnp_igd_context* igd_ctxt) {
	int ret;

	upnp_igd_remove_all(igd_ctxt);

	upnp_igd_print(igd_ctxt, UPNP_IGD_MESSAGE, "IGD client searching...\n");
	ret = UpnpSearchAsync(igd_ctxt->upnp_handle, 5, IGDDeviceType, igd_ctxt);
	if (UPNP_E_SUCCESS != ret) {
		upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error sending search request%d\n", ret);
		return -1;
	}

	return 0;
}


/********************************************************************************
 * upnp_igd_var_updated
 *
 * Description:
 *       Function called when a variable is updated or grabbed
 *
 * Parameters:
 *   igd_ctxt    -- The upnp igd context
 *   device_node -- The device
 *   service     -- The service
 *   variable    -- The variable
 *   varValue    -- The value of the variable
 *
 ********************************************************************************/
void upnp_igd_var_updated(upnp_igd_context* igd_ctxt, upnp_igd_device_node *device_node, int service, int variable, const DOMString varValue) {
	upnp_igd_print(igd_ctxt, UPNP_IGD_MESSAGE, "IGD device: %s[%s] | %s.%s = %s\n",
			device_node->device.friendly_name, device_node->device.udn,
			IGDServiceName[service], IGDVarName[service][variable], varValue);
	if(igd_ctxt->callback_fct != NULL) {
		if(service == IGD_SERVICE_WANIPCONNECTION && variable == IGD_SERVICE_WANIPCONNECTION_EXTERNAL_IP_ADDRESS) {
			igd_ctxt->callback_fct(igd_ctxt->cookie, UPNP_IGD_EXTERNAL_IPADDRESS_CHANGED, (void*)varValue);
		} else if(service == IGD_SERVICE_WANIPCONNECTION && variable == IGD_SERVICE_WANIPCONNECTION_NAT_ENABLED) {
			igd_ctxt->callback_fct(igd_ctxt->cookie, UPNP_IGD_NAT_ENABLED_CHANGED, (void*)varValue);
		} else if(service == IGD_SERVICE_WANIPCONNECTION && variable == IGD_SERVICE_WANIPCONNECTION_CONNECTION_STATUS) {
			igd_ctxt->callback_fct(igd_ctxt->cookie, UPNP_IGD_CONNECTION_STATUS_CHANGED, (void*)varValue);
		}
	}
}


/********************************************************************************
 * upnp_igd_handle_get_var
 *
 * Description:
 *       Function called when a variable is grabbed
 *
 * Parameters:
 *   igd_ctxt   -- The upnp igd context
 *   controlURL -- The control url used for the update
 *   varName    -- The variable name
 *   varValue   -- The value of the variable
 *
 ********************************************************************************/
void upnp_igd_handle_get_var(upnp_igd_context* igd_ctxt, const char *controlURL, const char *varName, const DOMString varValue) {
	upnp_igd_device_node *tmpdevnode;
	int service, variable;

	ithread_mutex_lock(&igd_ctxt->devices_mutex);

	tmpdevnode = igd_ctxt->devices;
	while (tmpdevnode) {
		for (service = 0; service < IGD_SERVICE_SERVCOUNT; service++) {
			if (strcmp(tmpdevnode->device.services[service].control_url, controlURL) == 0) {
				for (variable = 0; variable < IGDVarCount[service]; variable++) {
					if (strcmp(IGDVarName[service][variable], varName) == 0) {
						strcpy(tmpdevnode->device.services[service].variables[variable], varValue);
						upnp_igd_var_updated(igd_ctxt, tmpdevnode, service, variable, varValue);
						break;
					}
				}
				if(variable < IGDVarCount[service]) {
					break;
				}
			}
		}
		tmpdevnode = tmpdevnode->next;
	}

	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
}


/********************************************************************************
 * upnp_igd_handle_send_action
 *
 * Description:
 *       Function called when a variable is grabbed
 *
 * Parameters:
 *   igd_ctxt   -- The upnp igd context
 *   controlURL -- The control url used for the update
 *   varName    -- The variable name
 *   varValue   -- The value of the variable
 *
 ********************************************************************************/
void upnp_igd_handle_send_action(upnp_igd_context* igd_ctxt, const char *controlURL, IXML_Document *action, IXML_Document *result) {
	upnp_igd_device_node *tmpdevnode;
	int service;
	IXML_Node *node;
	IXML_Element *variable;
	IXML_NodeList *variables;
	long unsigned int length1;
	int j;
	char *tmpstate = NULL;
	const char *ctmpstate = NULL;
	char variable_name[IGD_MAX_VAR_LEN + 3];

	ithread_mutex_lock(&igd_ctxt->devices_mutex);

	tmpdevnode = igd_ctxt->devices;
	while (tmpdevnode) {
		for (service = 0; service < IGD_SERVICE_SERVCOUNT; service++) {
			if (strcmp(tmpdevnode->device.services[service].control_url, controlURL) == 0) {
				for (j = 0; j < IGDVarCount[service]; j++) {
						// Build the element name from the variable
						strcpy(variable_name, "New");
						strcat(variable_name, IGDVarName[service][j]);
						variables = ixmlDocument_getElementsByTagName(result, variable_name);

						/* If a match is found, extract
						 * the value, and update the state table */
						if (variables) {
							length1 = ixmlNodeList_length(variables);
							if (length1) {
								variable = (IXML_Element *) ixmlNodeList_item(variables, 0);
								tmpstate = upnp_igd_get_element_value(igd_ctxt, variable);
								if (tmpstate) {
									strcpy(tmpdevnode->device.services[service].variables[j], tmpstate);
									upnp_igd_var_updated(igd_ctxt, tmpdevnode, service, j, tmpdevnode->device.services[service].variables[j]);
								}
								if (tmpstate) {
									free(tmpstate);
								}
								tmpstate = NULL;
							}
							ixmlNodeList_free(variables);
							variables = NULL;
						}
				}
				if(igd_ctxt->callback_fct != NULL) {
					node = ixmlNode_getFirstChild(&result->n);
					if(node && node->nodeType == eELEMENT_NODE) {
						variable = (IXML_Element *)node;
						ctmpstate = ixmlElement_getTagName(variable);
						if(ctmpstate != NULL) {
							if(strcmp(ctmpstate, "AddPortMappingResponse") == 0) {
								igd_ctxt->callback_fct(igd_ctxt->cookie, UPNP_IGD_PORT_MAPPING_ADDED, NULL);
							} else if(strcmp(ctmpstate, "DeletePortMappingResponse") == 0) {
								igd_ctxt->callback_fct(igd_ctxt->cookie, UPNP_IGD_PORT_MAPPING_REMOVED, NULL);
							}
						}
					}
				}
				break;
			}
		}
		tmpdevnode = tmpdevnode->next;
	}

	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
}


/********************************************************************************
 * upnp_igd_state_update
 *
 * Description:
 *       Handle a UPnP event that was received.  Process the event and update
 *       the appropriate service state table.
 *
 * Parameters:
 *   igd_ctxt          -- The upnp igd context
 *   device_node       -- The device updated
 *   service           -- The service updated
 *   changed_variables -- The updated variables
 *   values            -- The values of variables to update
 *
 ********************************************************************************/
void upnp_igd_state_update(upnp_igd_context* igd_ctxt, upnp_igd_device_node *device_node, int service, IXML_Document *changed_variables, char **values) {
	IXML_NodeList *properties;
	IXML_NodeList *variables;
	IXML_Element *property;
	IXML_Element *variable;
	long unsigned int length;
	long unsigned int length1;
	long unsigned int i;
	int j;
	char *tmpstate = NULL;

	upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "IGD State Update (service %d):\n", service);
	/* Find all of the e:property tags in the document */
	properties = ixmlDocument_getElementsByTagName(changed_variables, "e:property");
	if (properties) {
		length = ixmlNodeList_length(properties);
		for (i = 0; i < length; i++) {
			/* Loop through each property change found */
			property = (IXML_Element *)ixmlNodeList_item(properties, i);
			/* For each variable name in the state table,
			 * check if this is a corresponding property change */
			for (j = 0; j < IGDVarCount[service]; j++) {
				variables = ixmlElement_getElementsByTagName(property, IGDVarName[service][j]);
				/* If a match is found, extract
				 * the value, and update the state table */
				if (variables) {
					length1 = ixmlNodeList_length(variables);
					if (length1) {
						variable = (IXML_Element *) ixmlNodeList_item(variables, 0);
						tmpstate = upnp_igd_get_element_value(igd_ctxt, variable);
						if (tmpstate) {
							strcpy(values[j], tmpstate);
							upnp_igd_var_updated(igd_ctxt, device_node, service, j, values[j]);
						}
						if (tmpstate) {
							free(tmpstate);
						}
						tmpstate = NULL;
					}
					ixmlNodeList_free(variables);
					variables = NULL;
				}
			}
		}
		ixmlNodeList_free(properties);
	}
	return;
}


/********************************************************************************
 * upnp_igd_handle_event
 *
 * Description:
 *       Handle a UPnP event that was received.  Process the event and update
 *       the appropriate service state table.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *   sid      -- The subscription id for the event
 *   eventkey -- The eventkey number for the event
 *   changes  -- The DOM document representing the changes
 *
 ********************************************************************************/
void upnp_igd_handle_event(upnp_igd_context* igd_ctxt, const char *sid, int eventkey, IXML_Document *changes) {
	upnp_igd_device_node *tmpdevnode;
	int service;

	ithread_mutex_lock(&igd_ctxt->devices_mutex);

	tmpdevnode = igd_ctxt->devices;
	while (tmpdevnode) {
		for (service = 0; service < IGD_SERVICE_SERVCOUNT; ++service) {
			if (strcmp(tmpdevnode->device.services[service].sid, sid) ==  0) {
				upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "Received IGD %s Event: %d for SID %s\n", IGDServiceName[service], eventkey, sid);
				upnp_igd_state_update(igd_ctxt, tmpdevnode, service, changes, (char **)&tmpdevnode->device.services[service].variables);
				break;
			}
		}
		tmpdevnode = tmpdevnode->next;
	}

	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
}


/********************************************************************************
 * upnp_igd_handle_subscribe_update
 *
 * Description:
 *       Handle a UPnP subscription update that was received.  Find the
 *       service the update belongs to, and update its subscription
 *       timeout.
 *
 * Parameters:
 *   igd_ctxt  -- The upnp igd context
 *   event_url -- The event URL for the subscription
 *   sid       -- The subscription id for the subscription
 *   timeout   -- The new timeout for the subscription
 *
 ********************************************************************************/
void upnp_igd_handle_subscribe_update(upnp_igd_context* igd_ctxt, const char *event_url, const Upnp_SID sid, int timeout) {
	upnp_igd_device_node *tmpdevnode;
	int service;

	ithread_mutex_lock(&igd_ctxt->devices_mutex);

	tmpdevnode = igd_ctxt->devices;
	while (tmpdevnode) {
		for (service = 0; service < IGD_SERVICE_SERVCOUNT; service++) {
			if (strcmp(tmpdevnode->device.services[service].event_url, event_url) == 0) {
				upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "Received IGD %s Event Renewal for event_url %s\n", IGDServiceName[service], event_url);
				strcpy(tmpdevnode->device.services[service].sid, sid);
				break;
			}
		}

		tmpdevnode = tmpdevnode->next;
	}

	ithread_mutex_unlock(&igd_ctxt->devices_mutex);

	return;
}


/********************************************************************************
 * upnp_igd_callback
 *
 * Description:
 *       The callback handler registered with the SDK while registering
 *       the control point.  Detects the type of callback, and passes the
 *       request on to the appropriate function.
 *
 * Parameters:
 *   event_type -- The type of callback event
 *   event      -- Data structure containing event data
 *   cookie     -- Optional data specified during callback registration
 *
 ********************************************************************************/
int upnp_igd_callback(Upnp_EventType event_type, void* event, void *cookie) {
	int ret = 1;
	upnp_igd_context *igd_ctxt = (upnp_igd_context*)cookie;
	upnp_igd_print_event(igd_ctxt, UPNP_IGD_DEBUG, event_type, event);
    switch(event_type) {
    	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    	case UPNP_DISCOVERY_SEARCH_RESULT: {
    		struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)event;
    		IXML_Document *desc_doc = NULL;
    		int ret;

    		if (d_event->ErrCode != UPNP_E_SUCCESS) {
    			upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error in Discovery Callback -- %d\n", d_event->ErrCode);
    		}
    		ret = UpnpDownloadXmlDoc(d_event->Location, &desc_doc);
    		if (ret != UPNP_E_SUCCESS) {
    			upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error obtaining device description from %s -- error = %d\n", d_event->Location, ret);
    		} else {
    			upnp_igd_add_device(igd_ctxt, desc_doc, d_event);
    		}
    		if (desc_doc) {
    			ixmlDocument_free(desc_doc);
    		}
    	}
    	break;
    	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE: {
    		struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)event;
    		if (d_event->ErrCode != UPNP_E_SUCCESS) {
    			upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error in Discovery ByeBye Callback -- %d\n", d_event->ErrCode);
    		}
    		upnp_igd_remove_device(igd_ctxt, d_event->DeviceId);
    	}
    	break;
    	/* SOAP Stuff */
    	case UPNP_CONTROL_ACTION_COMPLETE: {
    		struct Upnp_Action_Complete *a_event = (struct Upnp_Action_Complete *)event;

    		if (a_event->ErrCode != UPNP_E_SUCCESS) {
    			upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error in  Action Complete Callback -- %d\n", a_event->ErrCode);
    		} else {
    			upnp_igd_handle_send_action(igd_ctxt, a_event->CtrlUrl, a_event->ActionRequest, a_event->ActionResult);
    		}
    	}
    	break;
    	case UPNP_CONTROL_GET_VAR_COMPLETE: {
    		struct Upnp_State_Var_Complete *sv_event = (struct Upnp_State_Var_Complete *)event;

    		if (sv_event->ErrCode != UPNP_E_SUCCESS) {
    			upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error in Get Var Complete Callback -- %d\n", sv_event->ErrCode);
    		} else {
    			upnp_igd_handle_get_var(igd_ctxt, sv_event->CtrlUrl, sv_event->StateVarName, sv_event->CurrentVal);
    		}
    	}
    	break;
    	/* GENA Stuff */
    	case UPNP_EVENT_RECEIVED: {
    		struct Upnp_Event *e_event = (struct Upnp_Event *)event;

    		upnp_igd_handle_event(igd_ctxt, e_event->Sid, e_event->EventKey, e_event->ChangedVariables);
    	}
    	break;
    	case UPNP_EVENT_SUBSCRIBE_COMPLETE:
    	case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
    	case UPNP_EVENT_RENEWAL_COMPLETE: {
    		struct Upnp_Event_Subscribe *es_event = (struct Upnp_Event_Subscribe *)event;

    		if (es_event->ErrCode != UPNP_E_SUCCESS) {
    			upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error in Event Subscribe Callback -- %d\n", es_event->ErrCode);
    		} else {
    			upnp_igd_handle_subscribe_update(igd_ctxt, es_event->PublisherUrl, es_event->Sid, es_event->TimeOut);
    		}
    	}
    	break;
    	case UPNP_EVENT_AUTORENEWAL_FAILED:
    	case UPNP_EVENT_SUBSCRIPTION_EXPIRED: {
    		struct Upnp_Event_Subscribe *es_event = (struct Upnp_Event_Subscribe *)event;
    		int TimeOut = 1801;
    		Upnp_SID newSID;
    		int ret;

    		ret = UpnpSubscribe(igd_ctxt->upnp_handle, es_event->PublisherUrl, &TimeOut, newSID);
    		if (ret == UPNP_E_SUCCESS) {
    			upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "Subscribed to EventURL with SID=%s\n", newSID);
    			upnp_igd_handle_subscribe_update(igd_ctxt, es_event->PublisherUrl, newSID, TimeOut);
    		} else {
    			upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error Subscribing to EventURL -- %d\n", ret);
    		}
    	}
    	break;
    	default:
    	break;
    }

    return ret;
}


/********************************************************************************
 * upnp_igd_create
 *
 * Description:
 *       Create and  return uPnP IGD context if there is no error otherwise
 *       NULL.
 *
 * Parameters:
 *   cb_fct    -- The function to call back for each events
 *   print_fct -- The function used for print logs
 *   cookie    -- The cookie pass in cb_fct or print_fct
 *
 ********************************************************************************/
upnp_igd_context* upnp_igd_create(upnp_igd_callback_function cb_fct, upnp_igd_print_function print_fct, void *cookie) {
	int ret;
	unsigned short port = 0;
	char *ip_address = NULL;
	upnp_igd_context *igd_ctxt = (upnp_igd_context*)malloc(sizeof(upnp_igd_context));
	igd_ctxt->devices = NULL;
	igd_ctxt->callback_fct = cb_fct;
	igd_ctxt->cookie = cookie;

	/* Initialize print mutex */
	{
		ithread_mutexattr_t attr;
		ithread_mutexattr_init(&attr);
		ithread_mutexattr_setkind_np(&attr, ITHREAD_MUTEX_RECURSIVE_NP);
		ithread_mutex_init(&igd_ctxt->print_mutex, &attr);
		ithread_mutexattr_destroy(&attr);
		ithread_mutex_lock(&igd_ctxt->print_mutex);
		igd_ctxt->print_fct = print_fct;
		ithread_mutex_unlock(&igd_ctxt->print_mutex);
	}

	/* Initialize device mutex */
	{
		ithread_mutexattr_t attr;
		ithread_mutexattr_init(&attr);
		ithread_mutexattr_setkind_np(&attr, ITHREAD_MUTEX_RECURSIVE_NP);
		ithread_mutex_init(&igd_ctxt->devices_mutex, &attr);
		ithread_mutexattr_destroy(&attr);
	}

	upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "Initializing UPnP IGD with ipaddress:%s port:%u\n", ip_address ? ip_address : "{NULL}", port);

	ret = UpnpInit(ip_address, port);
	if (ret != UPNP_E_SUCCESS) {
		upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "UpnpInit() Error: %d\n", ret);
		UpnpFinish();
		ithread_mutex_destroy(&igd_ctxt->print_mutex);
		free(igd_ctxt);
		return NULL;
	}
	if (!ip_address) {
		ip_address = UpnpGetServerIpAddress();
	}
	if (!port) {
		port = UpnpGetServerPort();
	}

	upnp_igd_print(igd_ctxt, UPNP_IGD_MESSAGE, "UPnP IGD Initialized ipaddress:%s port:%u\n", ip_address ? ip_address : "{NULL}", port);
	upnp_igd_print(igd_ctxt, UPNP_IGD_DEBUG, "UPnP IGD client registering...\n");
	ret = UpnpRegisterClient(upnp_igd_callback, igd_ctxt, &igd_ctxt->upnp_handle);
	if (ret != UPNP_E_SUCCESS) {
		upnp_igd_print(igd_ctxt, UPNP_IGD_ERROR, "Error registering IGD client: %d\n", ret);
		UpnpFinish();
		ithread_mutex_destroy(&igd_ctxt->print_mutex);
		free(igd_ctxt);
		return NULL;
	}

	upnp_igd_print(igd_ctxt, UPNP_IGD_MESSAGE, "UPnP IGD client registered\n");



	/* Initialize timer stuff */
	{
		ithread_mutexattr_t attr;
		ithread_mutexattr_init(&attr);
		ithread_mutexattr_setkind_np(&attr, ITHREAD_MUTEX_FAST_NP);
		ithread_mutex_init(&igd_ctxt->timer_mutex, &attr);
		ithread_mutexattr_destroy(&attr);
		ithread_cond_init(&igd_ctxt->timer_cond, NULL);
		ithread_create(&igd_ctxt->timer_thread, NULL, upnp_igd_timer_loop, igd_ctxt);
	}
	//
	//ithread_detach(timer_thread);

	upnp_igd_refresh(igd_ctxt);

	return igd_ctxt;
}

/********************************************************************************
 * upnp_igd_destroy
 *
 * Description:
 *       Destroy an existing uPnP IGD context.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *
 ********************************************************************************/
void upnp_igd_destroy(upnp_igd_context* igd_ctxt) {
	ithread_mutex_lock(&igd_ctxt->timer_mutex);
	ithread_cond_signal(&igd_ctxt->timer_cond);
	ithread_mutex_unlock(&igd_ctxt->timer_mutex);
	ithread_join(igd_ctxt->timer_thread, NULL);

	upnp_igd_remove_all(igd_ctxt);

	ithread_mutex_destroy(&igd_ctxt->devices_mutex);

	ithread_cond_destroy(&igd_ctxt->timer_cond);
	ithread_mutex_destroy(&igd_ctxt->timer_mutex);

	ithread_mutex_destroy(&igd_ctxt->print_mutex);

	UpnpUnRegisterClient(igd_ctxt->upnp_handle);
	UpnpFinish();
	free(igd_ctxt);
}
