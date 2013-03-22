#ifndef _UPNP_IGD_H__
#define _UPNP_IGD_H__

#include <stdarg.h>

#if defined(_MSC_VER)
#define MS2_PUBLIC	__declspec(dllexport)
#else
#define MS2_PUBLIC
#endif

typedef enum _upnp_igd_print_level {
	UPNP_IGD_DEBUG = 0,
	UPNP_IGD_MESSAGE,
	UPNP_IGD_WARNING,
	UPNP_IGD_ERROR
} upnp_igd_print_level;

typedef enum _upnp_igd_ip_protocol {
	UPNP_IGD_IP_PROTOCOL_UDP = 0,
	UPNP_IGD_IP_PROTOCOL_TCP
} upnp_igd_ip_protocol;

typedef enum _upnp_igd_event {
	UPNP_IGD_EXTERNAL_IPADDRESS_CHANGED = 0,
	UPNP_IGD_NAT_ENABLED_CHANGED,
	UPNP_IGD_CONNECTION_STATUS_CHANGED,
	UPNP_IGD_PORT_MAPPING_ADD_SUCCESS,
	UPNP_IGD_PORT_MAPPING_ADD_FAILURE,
	UPNP_IGD_PORT_MAPPING_REMOVE_SUCCESS,
	UPNP_IGD_PORT_MAPPING_REMOVE_FAILURE,
	UPNP_IGD_DEVICE_ADDED = 100,
	UPNP_IGD_DEVICE_REMOVED,
} upnp_igd_event;

typedef struct _upnp_igd_port_mapping {
	upnp_igd_ip_protocol protocol;

	const char* local_host;
	int local_port;

	const char* remote_host;
	int remote_port;

	const char* description;

	void *cookie;
	int retvalue;
} upnp_igd_port_mapping;

typedef void (*upnp_igd_callback_function)(void *cookie, upnp_igd_event event, void *arg);
typedef void (*upnp_igd_print_function)(void *cookie, upnp_igd_print_level level, const char *fmt, va_list list);

typedef struct _upnp_igd_context upnp_igd_context;

MS2_PUBLIC upnp_igd_context* upnp_igd_create(upnp_igd_callback_function cb_fct, upnp_igd_print_function print_fct, const char* address, void *cookie);
MS2_PUBLIC int upnp_igd_start(upnp_igd_context*igd_ctxt);
int upnp_igd_is_started(upnp_igd_context *igd_ctxt);
int upnp_igd_stop(upnp_igd_context*igd_ctxt);
MS2_PUBLIC void upnp_igd_destroy(upnp_igd_context *igd_ctxt);
MS2_PUBLIC char *upnp_igd_get_local_ipaddress(upnp_igd_context *igd_ctxt);
MS2_PUBLIC const char *upnp_igd_get_device_id(upnp_igd_context *igd_ctxt);
MS2_PUBLIC const char *upnp_igd_get_external_ipaddress(upnp_igd_context *igd_ctxt);
MS2_PUBLIC const char *upnp_igd_get_connection_status(upnp_igd_context *igd_ctxt);
MS2_PUBLIC int upnp_igd_get_nat_enabled(upnp_igd_context *igd_ctxt);

MS2_PUBLIC int upnp_igd_add_port_mapping(upnp_igd_context *igd_ctxt, const upnp_igd_port_mapping *mapping);
MS2_PUBLIC int upnp_igd_delete_port_mapping(upnp_igd_context *igd_ctxt, const upnp_igd_port_mapping *mapping);

MS2_PUBLIC int upnp_igd_refresh(upnp_igd_context *igd_ctxt);
MS2_PUBLIC void upnp_igd_set_devices_timeout(upnp_igd_context *igd_ctxt, int seconds);
MS2_PUBLIC int upnp_igd_get_devices_timeout(upnp_igd_context *igd_ctxt);

#endif //_UPNP_IGD_H__
