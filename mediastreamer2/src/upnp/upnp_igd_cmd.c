#include "upnp_igd.h"
#include "upnp_igd_private.h"
#include <stdlib.h>
#include <stdio.h>

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
	if(igd_ctxt->devices != NULL) {
		address = igd_ctxt->devices->device.services[IGD_SERVICE_WANIPCONNECTION].variables[IGD_SERVICE_WANIPCONNECTION_EXTERNAL_IP_ADDRESS];
		if(address != NULL) {
			if(strlen(address) == 0) {
				address = NULL;
			}
		}
	}
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
	if(igd_ctxt->devices != NULL) {
		status = igd_ctxt->devices->device.services[IGD_SERVICE_WANIPCONNECTION].variables[IGD_SERVICE_WANIPCONNECTION_CONNECTION_STATUS];
		if(status != NULL) {
			if(strlen(status) == 0) {
				status = NULL;
			}
		}
	}
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
	if(igd_ctxt->devices != NULL) {
		status = igd_ctxt->devices->device.services[IGD_SERVICE_WANIPCONNECTION].variables[IGD_SERVICE_WANIPCONNECTION_NAT_ENABLED];
		if(status != NULL) {
			if(strcmp(status, "1") == 0) {
				enabled = 1;
			}
		}
	}
	return enabled;
}


/********************************************************************************
 * upnp_igd_add_port_mapping
 *
 * Description:
 *       Add a port mapping.
 *
 * Parameters:
 *   igd_ctxt    -- The upnp igd context
 *   protocol    -- The protocol to map (UDP/TCP)
 *   local_host  -- The local host to map
 *   local_port  -- The local port to map
 *   remote_host -- The remote host to map
 *   remote_port -- The remote port to map
 *   description -- The description associated with the mapping
 *
 ********************************************************************************/
int upnp_igd_add_port_mapping(upnp_igd_context *igd_ctxt,
		upnp_igd_ip_protocol protocol,
		const char* local_host, int local_port,
		const char* remote_host, int remote_port,
		const char* description) {
	int ret;
	char local_port_str[6], remote_port_str[6];
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
			(protocol == UPNP_IGD_IP_PROTOCOL_UDP)? "UDP": "TCP",
			local_host,
			local_port_str,
			remote_host,
			remote_port_str,
			description,
			"0",
			"1"
	};

	if(igd_ctxt->devices == NULL || remote_host == NULL || local_host == NULL) {
			return -1;
	}

	/* Convert int to str */
	snprintf(local_port_str, sizeof(local_port_str)/sizeof(local_port_str[0]), "%d", local_port);
	snprintf(remote_port_str, sizeof(remote_port_str)/sizeof(remote_port_str[0]), "%d", remote_port);

	ret = upnp_igd_send_action(igd_ctxt, igd_ctxt->devices, IGD_SERVICE_WANIPCONNECTION, "AddPortMapping",
			variables, values, sizeof(values)/sizeof(values[0]),
			upnp_igd_callback, igd_ctxt);

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
int upnp_igd_delete_port_mapping(upnp_igd_context *igd_ctxt,
		upnp_igd_ip_protocol protocol,
		const char* remote_host, int remote_port) {
	int ret;
	char remote_port_str[6];
	const char* variables[]={
			"NewProtocol",
			"NewRemoteHost",
			"NewExternalPort",
	};
	const char* values[]={
			(protocol == UPNP_IGD_IP_PROTOCOL_UDP)? "UDP": "TCP",
			remote_host,
			remote_port_str
	};

	if(igd_ctxt->devices == NULL) {
			return -1;
	}

	/* Convert int to str */
	snprintf(remote_port_str, sizeof(remote_port_str)/sizeof(remote_port_str[0]), "%d", remote_port);

	ret = upnp_igd_send_action(igd_ctxt, igd_ctxt->devices, IGD_SERVICE_WANIPCONNECTION, "DeletePortMapping",
			variables, values, sizeof(values)/sizeof(values[0]),
			upnp_igd_callback, igd_ctxt);

	return ret;
}
