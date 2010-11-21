/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "belle_sip_resolver.h"

void belle_sip_resolver_context_destroy(belle_sip_resolver_context_t *ctx){
	if (ctx->thread!=0){
		if (!ctx->exited){
			ctx->cancelled=1;
			pthread_cancel(ctx->thread);
		}
		pthread_join(ctx->thread,NULL);
	}
	if (ctx->name)
		belle_sip_free(ctx->name);
	if (ctx->ai){
		freeaddrinfo(ctx->ai);
	}
	belle_sip_free(ctx);
}

static int resolver_callback(belle_sip_resolver_context_t *ctx){
	ctx->cb(ctx->cb_data, ctx->name, ctx->ai);
	return 0;
}

belle_sip_resolver_context_t *belle_sip_resolver_context_new(){
	belle_sip_resolver_context_t *ctx=belle_sip_new0(belle_sip_resolver_context_t);
	if (pipe(ctx->ctlpipe)==-1){
		belle_sip_fatal("pipe() failed: %s",strerror(errno));
	}
	belle_sip_fd_source_init(&ctx->source,(belle_sip_source_func_t)resolver_callback,ctx,ctx->ctlpipe[0],BELLE_SIP_EVENT_READ,-1);
	ctx->source.on_remove=(belle_sip_source_remove_callback_t)belle_sip_resolver_context_destroy;
	return ctx;
}

static void *belle_sip_resolver_thread(void *ptr){
	belle_sip_resolver_context_t *ctx=(belle_sip_resolver_context_t *)ptr;
	if (write(ctx->ctlpipe[1],"q",1)==-1){
		belle_sip_error("belle_sip_resolver_thread(): Fail to write on pipe.");
	}
	return NULL;
}

unsigned long belle_sip_resolve(const char *name, unsigned int hints, belle_sip_resolver_callback_t cb , void *data, belle_sip_main_loop_t *ml){
	belle_sip_resolver_context_t *ctx=belle_sip_resolver_context_new();
	ctx->cb_data=data;
	ctx->cb=cb;
	ctx->name=belle_sip_strdup(name);
	ctx->hints=hints;
	belle_sip_main_loop_add_source(ml,(belle_sip_source_t*)ctx);
	pthread_create(&ctx->thread,NULL,belle_sip_resolver_thread,ctx);
	return ctx->source.id;
}

