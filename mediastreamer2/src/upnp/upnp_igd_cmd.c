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

#include "mediastreamer2/upnp_igd.h"
#include "upnp_igd_utils.h"
#include "upnp_igd_private.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct _upnp_igd_port_mapping_context {
	upnp_igd_context *igd_ctxt;
	upnp_igd_port_mapping mapping;
} upnp_igd_port_mapping_context;

upnp_igd_port_mapping_context * upnp_igd_port_mapping_context_create(upnp_igd_context *igd_ctxt, const upnp_igd_port_mapping *mapping) {
	upnp_igd_port_mapping_context *igd_port_mapping_ctxt = (upnp_igd_port_mapping_context *)malloc(sizeof(upnp_igd_port_mapping_context));
	igd_port_mapping_ctxt->igd_ctxt = igd_ctxt;
	memcpy(&igd_port_mapping_ctxt->mapping, mapping, sizeof(upnp_igd_port_mapping));
	return igd_port_mapping_ctxt;
}

void upnp_igd_port_mapping_context_destroy(upnp_igd_port_mapping_context *igd_port_mapping_ctxt) {
	free(igd_port_mapping_ctxt);
}

int upnp_igd_port_mapping_handle_action(upnp_igd_port_mapping_context *igd_port_mapping_ctxt, int errcode, const char *controlURL, IXML_Document *action, IXML_Document *result) {
	upnp_igd_device_node *tmpdevnode;
	int service;
	IXML_Node *node;
	const char *ctmpstate = NULL;
	upnp_igd_context *igd_ctxt = igd_port_mapping_ctxt->igd_ctxt;

	ithread_mutex_lock(&igd_ctxt->devices_mutex);

	tmpdevnode = igd_ctxt->devices;
	while (tmpdevnode) {
		for (service = 0; service < IGD_SERVICE_SERVCOUNT; service++) {
			if (strcmp(tmpdevnode->device.services[service].control_url, controlURL) == 0) {
				node = ixmlNode_getFirstChild(&action->n);
				if(node && node->nodeType == eELEMENT_NODE) {
					ctmpstate = ixmlNode_getLocalName(node);
					if(ctmpstate != NULL) {
						igd_port_mapping_ctxt->mapping.retvalue = errcode; // Set the return value
						if(strcmp(ctmpstate, "AddPortMapping") == 0) {
							if(errcode == UPNP_E_SUCCESS)
								upnp_context_add_callback(igd_ctxt, UPNP_IGD_PORT_MAPPING_ADD_SUCCESS, &igd_port_mapping_ctxt->mapping);
							else
								upnp_context_add_callback(igd_ctxt, UPNP_IGD_PORT_MAPPING_ADD_FAILURE, &igd_port_mapping_ctxt->mapping);
						} else if(strcmp(ctmpstate, "DeletePortMapping") == 0) {
							if(errcode == UPNP_E_SUCCESS)
								upnp_context_add_callback(igd_ctxt, UPNP_IGD_PORT_MAPPING_REMOVE_SUCCESS, &igd_port_mapping_ctxt->mapping);
							else
								upnp_context_add_callback(igd_ctxt, UPNP_IGD_PORT_MAPPING_REMOVE_FAILURE, &igd_port_mapping_ctxt->mapping);
						}
					}
				}
				break;
			}
		}
		tmpdevnode = tmpdevnode->next;
	}

	ithread_mutex_unlock(&igd_ctxt->devices_mutex);


	return 0;
}

int upnp_igd_port_mapping_callback(Upnp_EventType event_type, void* event, void *cookie) {
	int ret = 1;
	upnp_igd_port_mapping_context *igd_port_mapping_ctxt = (upnp_igd_port_mapping_context*)cookie;
	upnp_context_add_client(igd_port_mapping_ctxt->igd_ctxt);
	ret = upnp_igd_callback(event_type, event, igd_port_mapping_ctxt->igd_ctxt);

	switch(event_type) {
		case UPNP_CONTROL_ACTION_COMPLETE: {
		struct Upnp_Action_Complete *a_event = (struct Upnp_Action_Complete *)event;
			upnp_igd_port_mapping_handle_action(igd_port_mapping_ctxt, a_event->ErrCode, UPNP_STRING(a_event->CtrlUrl), a_event->ActionRequest, a_event->ActionResult);
		}
		break;

		default:
		break;
	}

	upnp_context_handle_callbacks(igd_port_mapping_ctxt->igd_ctxt);
	upnp_context_remove_client(igd_port_mapping_ctxt->igd_ctxt);
	upnp_igd_port_mapping_context_destroy(igd_port_mapping_ctxt);

	return ret;
}


/********************************************************************************
 * upnp_igd_get_local_ipaddress
 *
 * Description:
 *       Return the local ip address or NULL if doesn't exist.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *
 ********************************************************************************/
char *upnp_igd_get_local_ipaddress(upnp_igd_context *igd_ctxt) {
	return UpnpGetServerIpAddress();
}


/********************************************************************************
 * upnp_igd_get_external_ipaddress
 *
 * Description:
 *       Return the external ip address or NULL if doesn't exist.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *
 ********************************************************************************/
const char *upnp_igd_get_external_ipaddress(upnp_igd_context* igd_ctxt) {
	static char ret[IGD_MAX_VAL_LEN]; 
	const char *address = NULL;
	ithread_mutex_lock(&igd_ctxt->devices_mutex);
	if(igd_ctxt->devices != NULL) {
		address = igd_ctxt->devices->device.services[IGD_SERVICE_WANIPCONNECTION].variables[IGD_SERVICE_WANIPCONNECTION_EXTERNAL_IP_ADDRESS];
		if(address != NULL) {
			if(strlen(address) == 0) {
				address = NULL;
			} else {
				upnp_igd_strncpy(ret, address, sizeof(ret));
				address = ret;
			}
		}
	}
	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
	return address;
}

/********************************************************************************
 * upnp_igd_get_device_id
 *
 * Description:
 *       Return the device identifier, NULL if doesn't exist.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *
 ********************************************************************************/
MS2_PUBLIC const char *upnp_igd_get_device_id(upnp_igd_context *igd_ctxt) {
	static char ret[250]; 
	const char *id = NULL;
	ithread_mutex_lock(&igd_ctxt->devices_mutex);
	if(igd_ctxt->devices != NULL) {
		id = igd_ctxt->devices->device.udn;
		if(id != NULL) {
			if(strlen(id) == 0) {
				id = NULL;
			} else {
				upnp_igd_strncpy(ret, id, sizeof(ret));
				id = ret;
			}
		}
	}
	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
	return id;
}

/********************************************************************************
 * upnp_igd_get_device_name
 *
 * Description:
 *       Return the device name, NULL if doesn't exist.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *
 ********************************************************************************/
MS2_PUBLIC const char *upnp_igd_get_device_name(upnp_igd_context *igd_ctxt) {
	static char ret[250]; 
	const char *id = NULL;
	ithread_mutex_lock(&igd_ctxt->devices_mutex);
	if(igd_ctxt->devices != NULL) {
		id = igd_ctxt->devices->device.friendly_name;
		if(id != NULL) {
			if(strlen(id) == 0) {
				id = NULL;
			} else {
				upnp_igd_strncpy(ret, id, sizeof(ret));
				id = ret;
			}
		}
	}
	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
	return id;
}

/********************************************************************************
 * upnp_igd_get_device_model_name
 *
 * Description:
 *       Return the device model name, NULL if doesn't exist.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *
 ********************************************************************************/
MS2_PUBLIC const char *upnp_igd_get_device_model_name(upnp_igd_context *igd_ctxt) {
	static char ret[250]; 
	const char *id = NULL;
	ithread_mutex_lock(&igd_ctxt->devices_mutex);
	if(igd_ctxt->devices != NULL) {
		id = igd_ctxt->devices->device.model_name;
		if(id != NULL) {
			if(strlen(id) == 0) {
				id = NULL;
			} else {
				upnp_igd_strncpy(ret, id, sizeof(ret));
				id = ret;
			}
		}
	}
	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
	return id;
}

/********************************************************************************
 * upnp_igd_get_device_model_number
 *
 * Description:
 *       Return the device model number, NULL if doesn't exist.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *
 ********************************************************************************/
MS2_PUBLIC const char *upnp_igd_get_device_model_number(upnp_igd_context *igd_ctxt) {
	static char ret[250]; 
	const char *id = NULL;
	ithread_mutex_lock(&igd_ctxt->devices_mutex);
	if(igd_ctxt->devices != NULL) {
		id = igd_ctxt->devices->device.model_number;
		if(id != NULL) {
			if(strlen(id) == 0) {
				id = NULL;
			} else {
				upnp_igd_strncpy(ret, id, sizeof(ret));
				id = ret;
			}
		}
	}
	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
	return id;
}

/********************************************************************************
 * upnp_igd_get_connection_status
 *
 * Description:
 *       Return the connection status or NULL if doesn't exist.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *
 ********************************************************************************/
const char *upnp_igd_get_connection_status(upnp_igd_context *igd_ctxt) {
	static char ret[IGD_MAX_VAL_LEN]; 
	const char *status = NULL;
	ithread_mutex_lock(&igd_ctxt->devices_mutex);
	if(igd_ctxt->devices != NULL) {
		status = igd_ctxt->devices->device.services[IGD_SERVICE_WANIPCONNECTION].variables[IGD_SERVICE_WANIPCONNECTION_CONNECTION_STATUS];
		if(status != NULL) {
			if(strlen(status) == 0) {
				status = NULL;
			} else {
				upnp_igd_strncpy(ret, status, sizeof(ret));
				status = ret;
			}
		}
	}
	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
	return status;
}


/********************************************************************************
 * upnp_igd_get_nat_enabled
 *
 * Description:
 *       Return 1 if the nat is enabled otherwise 0.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *
 ********************************************************************************/
int upnp_igd_get_nat_enabled(upnp_igd_context *igd_ctxt) {
	int enabled = 0;
	const char *status = NULL;
	ithread_mutex_lock(&igd_ctxt->devices_mutex);
	if(igd_ctxt->devices != NULL) {
		status = igd_ctxt->devices->device.services[IGD_SERVICE_WANIPCONNECTION].variables[IGD_SERVICE_WANIPCONNECTION_NAT_ENABLED];
		if(status != NULL) {
			if(strcmp(status, "1") == 0) {
				enabled = 1;
			}
		}
	}
	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
	return enabled;
}


/********************************************************************************
 * upnp_igd_add_port_mapping
 *
 * Description:
 *       Add a port mapping.
 *
 * Parameters:
 *   igd_ctxt -- The upnp igd context
 *   mapping  -- The port mapping to add
 *
 ********************************************************************************/
int upnp_igd_add_port_mapping(upnp_igd_context *igd_ctxt, const upnp_igd_port_mapping *mapping) {
	int ret;
	char local_port_str[6], remote_port_str[6];
	upnp_igd_port_mapping_context * upnp_igd_port_mapping_ctxt = NULL;
	const char* variables[]={
			"NewProtocol",
			"NewInternalClient",
			"NewInternalPort",
			"NewRemoteHost",
			"NewExternalPort",
			"NewPortMappingDescription",
			"NewLeaseDuration",
			"NewEnabled"
	};
	const char* values[]={
			NULL,
			NULL,
			local_port_str,
			NULL,
			remote_port_str,
			NULL,
			"0",
			"1"
	};

	ithread_mutex_lock(&igd_ctxt->devices_mutex);
	if(igd_ctxt->devices != NULL && mapping != NULL && mapping->remote_host != NULL && mapping->local_host != NULL) {
		/* Set values */
		values[0] = (mapping->protocol == UPNP_IGD_IP_PROTOCOL_UDP)? "UDP": "TCP";
		values[1] = mapping->local_host;
		values[3] = mapping->remote_host;
		values[5] = mapping->description;

		/* Convert int to str */
		snprintf(local_port_str, sizeof(local_port_str)/sizeof(local_port_str[0]), "%d", mapping->local_port);
		snprintf(remote_port_str, sizeof(remote_port_str)/sizeof(remote_port_str[0]), "%d", mapping->remote_port);

		upnp_igd_port_mapping_ctxt = upnp_igd_port_mapping_context_create(igd_ctxt, mapping);

		ret = upnp_igd_send_action(igd_ctxt, igd_ctxt->devices, IGD_SERVICE_WANIPCONNECTION, "AddPortMapping",
				variables, values, sizeof(values)/sizeof(values[0]),
				upnp_igd_port_mapping_callback, upnp_igd_port_mapping_ctxt);
	} else {
		ret = 1;
	}
	ithread_mutex_unlock(&igd_ctxt->devices_mutex);

	return ret;
}


/********************************************************************************
 * upnp_igd_delete_port_mapping
 *
 * Description:
 *       Delete a port mapping.
 *
 * Parameters:
 *   igd_ctxt    -- The upnp igd context
 *   protocol    -- The protocol of map (UDP/TCP)
 *   remote_host -- The remote host of map
 *   remote_port -- The remote port of map
 *
 ********************************************************************************/
int upnp_igd_delete_port_mapping(upnp_igd_context *igd_ctxt, const upnp_igd_port_mapping *mapping) {
	int ret;
	char remote_port_str[6];
	upnp_igd_port_mapping_context * upnp_igd_port_mapping_ctxt = NULL;
	const char* variables[]={
			"NewProtocol",
			"NewRemoteHost",
			"NewExternalPort",
	};
	const char* values[]={
			NULL,
			NULL,
			remote_port_str
	};

	ithread_mutex_lock(&igd_ctxt->devices_mutex);
	if(igd_ctxt->devices != NULL && mapping != NULL && mapping->remote_host != NULL) {
		/* Set values */
		values[0] = (mapping->protocol == UPNP_IGD_IP_PROTOCOL_UDP)? "UDP": "TCP";
		values[1] = mapping->remote_host;

		/* Convert int to str */
		snprintf(remote_port_str, sizeof(remote_port_str)/sizeof(remote_port_str[0]), "%d", mapping->remote_port);

		upnp_igd_port_mapping_ctxt = upnp_igd_port_mapping_context_create(igd_ctxt, mapping);

		ret = upnp_igd_send_action(igd_ctxt, igd_ctxt->devices, IGD_SERVICE_WANIPCONNECTION, "DeletePortMapping",
				variables, values, sizeof(values)/sizeof(values[0]),
				upnp_igd_port_mapping_callback, upnp_igd_port_mapping_ctxt);
	} else {
		ret = -1;
	}
	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
	return ret;
}
