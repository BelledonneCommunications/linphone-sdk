#ifndef _UPNP_IGD_PRIVATE_H__
#define _UPNP_IGD_PRIVATE_H__

#include "mediastreamer2/upnp_igd.h"

#include <upnp.h>
#include <ithread.h>

enum {
	IGD_SERVICE_WANIPCONNECTION = 0,
	IGD_SERVICE_SERVCOUNT
};

enum {
	IGD_SERVICE_WANIPCONNECTION_EXTERNAL_IP_ADDRESS = 0,
	IGD_SERVICE_WANIPCONNECTION_NAT_ENABLED,
	IGD_SERVICE_WANIPCONNECTION_CONNECTION_STATUS,
	IGD_SERVICE_WANIPCONNECTION_VARCOUNT
};

#define IGD_MAXVARS 3
#define IGD_MAX_VAL_LEN 256
#define IGD_MAX_VAR_LEN 256

typedef struct _upnp_igd_service {
    char service_id[NAME_SIZE];
    char service_type[NAME_SIZE];
    char *variables[IGD_MAXVARS];
    char event_url[NAME_SIZE];
    char control_url[NAME_SIZE];
    char sid[NAME_SIZE];
} upnp_igd_service;

typedef struct _upnp_igd_device {
    char udn[250];
    char desc_doc_url[250];
    char friendly_name[250];
    char pres_url[250];
    int  advr_time_out;
    struct _upnp_igd_service services[IGD_SERVICE_SERVCOUNT];
} upnp_igd_device;

typedef struct _upnp_igd_device_node {
    struct _upnp_igd_device device;
    struct _upnp_igd_device_node *next;
} upnp_igd_device_node;

struct _upnp_igd_context {
	ithread_t timer_thread;
	ithread_cond_t timer_cond;
	ithread_mutex_t timer_mutex;

	UpnpClient_Handle upnp_handle;

	ithread_mutex_t devices_mutex;
	upnp_igd_device_node *devices;

	upnp_igd_callback_function callback_fct;
	ithread_mutex_t print_mutex;
	upnp_igd_print_function print_fct;
	void *cookie;
};


extern const char *IGDDeviceType;
extern const char *IGDServiceType[];
extern const char *IGDServiceName[];
extern const char *IGDVarName[IGD_SERVICE_SERVCOUNT][IGD_MAXVARS];
extern char IGDVarCount[IGD_SERVICE_SERVCOUNT];
extern int IGDTimeOut[IGD_SERVICE_SERVCOUNT];

int upnp_igd_callback(Upnp_EventType event_type, void* event, void *cookie);
int upnp_igd_send_action(upnp_igd_context* igd_ctxt, upnp_igd_device_node *device_node, int service,
		const char *actionname, const char **param_name, const char **param_val, int param_count,
		Upnp_FunPtr fun, const void *cookie);
int upnp_igd_get_var(upnp_igd_context* igd_ctxt, upnp_igd_device_node *device_node, int service, int variable,
		Upnp_FunPtr fun, const void *cookie);
#endif //_UPNP_IGD_PRIVATE_H__
