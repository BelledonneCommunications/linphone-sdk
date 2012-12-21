#include "mediastreamer2/upnp_igd.h"
#include "upnp_igd_private.h"
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
				if(igd_ctxt->callback_fct != NULL) {
					node = ixmlNode_getFirstChild(&action->n);
					if(node && node->nodeType == eELEMENT_NODE) {
						ctmpstate = ixmlNode_getLocalName(node);
						if(ctmpstate != NULL) {
							if(strcmp(ctmpstate, "AddPortMapping") == 0) {
								if(errcode == UPNP_E_SUCCESS)
									igd_ctxt->callback_fct(igd_ctxt->cookie, UPNP_IGD_PORT_MAPPING_ADD_SUCCESS, &igd_port_mapping_ctxt->mapping);
								else
									igd_ctxt->callback_fct(igd_ctxt->cookie, UPNP_IGD_PORT_MAPPING_ADD_FAILURE, &igd_port_mapping_ctxt->mapping);
							} else if(strcmp(ctmpstate, "DeletePortMapping") == 0) {
								if(errcode == UPNP_E_SUCCESS)
									igd_ctxt->callback_fct(igd_ctxt->cookie, UPNP_IGD_PORT_MAPPING_REMOVE_SUCCESS, &igd_port_mapping_ctxt->mapping);
								else
									igd_ctxt->callback_fct(igd_ctxt->cookie, UPNP_IGD_PORT_MAPPING_REMOVE_FAILURE, &igd_port_mapping_ctxt->mapping);
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


	return 0;
}

int upnp_igd_port_mapping_callback(Upnp_EventType event_type, void* event, void *cookie) {
	int ret = 1;
	upnp_igd_port_mapping_context *igd_port_mapping_ctxt = (upnp_igd_port_mapping_context*)cookie;
	ret = upnp_igd_callback(event_type, event, igd_port_mapping_ctxt->igd_ctxt);

	switch(event_type) {
		case UPNP_CONTROL_ACTION_COMPLETE: {
		struct Upnp_Action_Complete *a_event = (struct Upnp_Action_Complete *)event;
			upnp_igd_port_mapping_handle_action(igd_port_mapping_ctxt, a_event->ErrCode, a_event->CtrlUrl, a_event->ActionRequest, a_event->ActionResult);
		}
		break;

		default:
		break;
	}

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
	const char *address = NULL;
	ithread_mutex_lock(&igd_ctxt->devices_mutex);
	if(igd_ctxt->devices != NULL) {
		address = igd_ctxt->devices->device.services[IGD_SERVICE_WANIPCONNECTION].variables[IGD_SERVICE_WANIPCONNECTION_EXTERNAL_IP_ADDRESS];
		if(address != NULL) {
			if(strlen(address) == 0) {
				address = NULL;
			}
		}
	}
	ithread_mutex_unlock(&igd_ctxt->devices_mutex);
	return address;
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
	const char *status = NULL;
	ithread_mutex_lock(&igd_ctxt->devices_mutex);
	if(igd_ctxt->devices != NULL) {
		status = igd_ctxt->devices->device.services[IGD_SERVICE_WANIPCONNECTION].variables[IGD_SERVICE_WANIPCONNECTION_CONNECTION_STATUS];
		if(status != NULL) {
			if(strlen(status) == 0) {
				status = NULL;
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
