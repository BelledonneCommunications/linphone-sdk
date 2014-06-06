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

#ifndef _UPNP_IGD_PRIVATE_H__
#define _UPNP_IGD_PRIVATE_H__

#include "mediastreamer2/upnp_igd.h"
#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif
#ifdef __clang__

/*in case of compile with -g static inline can produce this type of warning*/
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

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
    char model_name[250];
    char model_number[250];
    char pres_url[250];
    int  advr_time_out;
    struct _upnp_igd_service services[IGD_SERVICE_SERVCOUNT];
} upnp_igd_device;

typedef struct _upnp_igd_device_node {
    struct _upnp_igd_device device;
    struct _upnp_igd_device_node *next;
} upnp_igd_device_node;

typedef struct _upnp_igd_callback_event {
	upnp_igd_event event;
	void *arg;
} upnp_igd_callback_event;

typedef struct _upnp_igd_callback_event_node {
	struct _upnp_igd_callback_event event;
	struct _upnp_igd_callback_event_node *next;
} upnp_igd_callback_event_node;

struct _upnp_igd_context {
	ithread_mutex_t mutex;
	
	ithread_t timer_thread;
	ithread_cond_t timer_cond;
	ithread_mutex_t timer_mutex;
	int timer_timeout;
	
	int max_adv_timeout;

	UpnpClient_Handle upnp_handle;

	ithread_mutex_t devices_mutex;
	upnp_igd_device_node *devices;

	ithread_cond_t client_cond;
	ithread_mutex_t client_mutex;
	int client_count;

	upnp_igd_callback_function callback_fct;
	upnp_igd_callback_event_node *callback_events;
	ithread_mutex_t callback_mutex;

	ithread_mutex_t print_mutex;
	upnp_igd_print_function print_fct;
	void *cookie;
};


#ifndef USE_PATCHED_UPNP
#define UPNP_STRING(x) (x)
#else
#define UPNP_STRING(x) UpnpString_get_String(x)
#endif //USE_PATCHED_UPNP

extern const char *IGDDeviceType;
extern const char *IGDServiceType[];
extern const char *IGDServiceName[];
extern const char *IGDVarName[IGD_SERVICE_SERVCOUNT][IGD_MAXVARS];
extern char IGDVarCount[IGD_SERVICE_SERVCOUNT];
extern int IGDTimeOut[IGD_SERVICE_SERVCOUNT];

void upnp_context_add_client(upnp_igd_context *igd_ctx);
void upnp_context_remove_client(upnp_igd_context *igd_ctx);
void upnp_context_add_callback(upnp_igd_context *igd_ctx, upnp_igd_event event, void *arg); 
void upnp_context_handle_callbacks(upnp_igd_context *igd_ctx);
void upnp_context_free_callbacks(upnp_igd_context *igd_ctx);

int upnp_igd_callback(Upnp_EventType event_type, void* event, void *cookie);
int upnp_igd_send_action(upnp_igd_context* igd_ctxt, upnp_igd_device_node *device_node, int service,
		const char *actionname, const char **param_name, const char **param_val, int param_count,
		Upnp_FunPtr fun, const void *cookie);
int upnp_igd_get_var(upnp_igd_context* igd_ctxt, upnp_igd_device_node *device_node, int service, int variable,
		Upnp_FunPtr fun, const void *cookie);
#endif //_UPNP_IGD_PRIVATE_H__
