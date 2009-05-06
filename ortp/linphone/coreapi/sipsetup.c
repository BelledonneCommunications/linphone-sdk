/*
linphone
Copyright (C) 2000  Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "linphonecore.h"

#ifndef LINPHONE_PLUGINS_DIR
#ifdef WIN32
#define LINPHONE_PLUGINS_DIR "liblinphone\\plugins\\"
#else
#define LINPHONE_PLUGINS_DIR "."
#endif
#endif

static SipSetup *all_sip_setups[]={
	NULL
};

static MSList *registered_sip_setups=NULL;

void sip_setup_register(SipSetup *ss){
	registered_sip_setups=ms_list_append(registered_sip_setups,ss);
}

void sip_setup_register_all(void){
	SipSetup **p=all_sip_setups;
	ms_load_plugins(LINPHONE_PLUGINS_DIR);
	while(*p!=NULL){
		sip_setup_register(*p);
	}
}

const MSList * linphone_core_get_sip_setups(LinphoneCore *lc){
	return registered_sip_setups;
}

SipSetup *sip_setup_lookup(const char *type_name){
	MSList *elem;
	for(elem=registered_sip_setups;elem!=NULL;elem=elem->next){
		SipSetup *ss=(SipSetup*)elem->data;
		if ( strcasecmp(ss->name,type_name)==0){
			if (!ss->initialized){
				ss->init();
				ss->initialized=TRUE;
				if (ss->capabilities==0){
					ms_error("%s SipSetup isn't capable of anything ?",ss->name);
				}
			}
			return ss;
		}
	}
	ms_warning("no %s setup manager declared.",type_name);
	return NULL;
}

void sip_setup_unregister_all(void){
	MSList *elem;
	for(elem=registered_sip_setups;elem!=NULL;elem=elem->next){
		SipSetup *ss=(SipSetup*)elem->data;
		if (ss->initialized){
			if (ss->exit) ss->exit();
			ss->initialized=FALSE;
		}
	}
}

LinphoneProxyConfig *sip_setup_context_get_proxy_config(const SipSetupContext *ctx){
	return ctx->cfg;
}

SipSetupContext *sip_setup_context_new(SipSetup *s, struct _LinphoneProxyConfig *cfg){
	SipSetupContext *obj=(SipSetupContext*)ms_new0(SipSetupContext,1);
	obj->funcs=s;
	obj->data=NULL;
	obj->cfg=cfg;
	if (obj->funcs->init_instance){
		obj->funcs->init_instance(obj);
	}
	return obj;
}

unsigned int sip_setup_get_capabilities(SipSetup *s){
	return s->capabilities;
}

int sip_setup_context_get_capabilities(SipSetupContext *ctx){
	return ctx->funcs->capabilities;
}

int sip_setup_context_create_account(SipSetupContext * ctx, const char *uri, const char *passwd){
	if (ctx->funcs->create_account)
		return ctx->funcs->create_account(ctx,uri, passwd);
	else return -1;
}

int sip_setup_context_account_exists(SipSetupContext *ctx, const char *uri){
	if (ctx->funcs->account_exists)
		return ctx->funcs->account_exists(ctx,uri);
	return -1;
}

int sip_setup_context_login_account(SipSetupContext * ctx, const char *uri, const char *passwd){
	osip_from_t *from;
	osip_from_init(&from);
	osip_from_parse(from,uri);
	strncpy(ctx->domain,from->url->host,sizeof(ctx->domain));
	strncpy(ctx->username,from->url->username,sizeof(ctx->username));
	osip_from_free(from);
	if (ctx->funcs->login_account)
		return ctx->funcs->login_account(ctx,uri,passwd);
	return -1;
}

int sip_setup_context_get_proxy(SipSetupContext *ctx, const char *domain, char *proxy, size_t sz){
	if (ctx->funcs->get_proxy)
		return ctx->funcs->get_proxy(ctx,domain ? domain : ctx->domain,proxy,sz);
	return -1;
}

int sip_setup_context_get_stun_servers(SipSetupContext *ctx, char *stun1, char *stun2, size_t size){
	if (ctx->funcs->get_stun_servers)
		return ctx->funcs->get_stun_servers(ctx,stun1,stun2,size);
	return -1;
}

int sip_setup_context_get_relay(SipSetupContext *ctx,char *relay, size_t size){
	if (ctx->funcs->get_relay)
		return ctx->funcs->get_relay(ctx,relay,size);
	return -1;
}

int sip_setup_context_lookup_buddy(SipSetupContext *ctx, const char *key){
	if (ctx->funcs->lookup_buddy)
		return ctx->funcs->lookup_buddy(ctx,key);
	return -1;
}

BuddyLookupStatus sip_setup_context_get_buddy_lookup_status(SipSetupContext *ctx){
	if (ctx->funcs->get_buddy_lookup_status)
		return ctx->funcs->get_buddy_lookup_status(ctx);
	return BuddyLookupFailure;
}

int sip_setup_context_get_buddy_lookup_results(SipSetupContext *ctx, MSList **results /*of BuddyInfo */){
	if (ctx->funcs->get_buddy_lookup_results)
		return ctx->funcs->get_buddy_lookup_results(ctx,results);
	return -1;
}

void sip_setup_context_free_results(MSList *results){
	ms_list_for_each(results,(void (*)(void*))&ms_free);
	ms_list_free(results);
}

void sip_setup_context_free(SipSetupContext *ctx){
	if (ctx->funcs->uninit_instance){
		ctx->funcs->uninit_instance(ctx);
	}
	ms_free(ctx);
}
