/*
linphone
Copyright (C) 2010  Simon MORLAT (simon.morlat@free.fr)

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

/** 
 This header files defines the Signaling Abstraction Layer.
 The purpose of this layer is too allow experiment different call signaling 
 protocols and implementations under linphone, for example SIP, JINGLE...
**/

#include "sal.h"

SalMediaDescription *sal_media_description_new(){
	SalMediaDescription *md=ms_new0(SalMediaDescription,1);
	md->refcount=1;
	return md;
}

static void sal_media_description_destroy(SalMediaDescription *md){
	int i;
	for(i=0;i<SAL_MEDIA_DESCRIPTION_MAX_STREAMS;i++){
		ms_list_for_each(md->streams[i].payloads,(void (*)(void *))payload_type_destroy);
		ms_list_free(md->streams[i].payloads);
		md->streams[i].payloads=NULL;
	}
	ms_free(md);
}

void sal_media_description_ref(SalMediaDescription *md){
	md->refcount++;
}

void sal_media_description_unref(SalMediaDescription *md){
	md->refcount--;
	if (md->refcount==0){
		sal_media_description_destroy (md);
	}
}

SalStreamDescription *sal_media_description_find_stream(SalMediaDescription *md,
    SalMediaProto proto, SalStreamType type){
	int i;
	for(i=0;i<md->nstreams;++i){
		SalStreamDescription *ss=&md->streams[i];
		if (ss->proto==proto && ss->type==type) return ss;
	}
	return NULL;
}

bool_t sal_media_description_empty(const SalMediaDescription *md){
	int i;
	for(i=0;i<md->nstreams;++i){
		const SalStreamDescription *ss=&md->streams[i];
		if (ss->port!=0) return FALSE;
	}
	return TRUE;
}

void sal_media_description_set_dir(SalMediaDescription *md, SalStreamDir stream_dir){
	int i;
	for(i=0;i<md->nstreams;++i){
		SalStreamDescription *ss=&md->streams[i];
		ss->dir=stream_dir;
	}
}

bool_t sal_media_description_has_dir(const SalMediaDescription *md, SalStreamDir stream_dir){
	int i;
	bool_t found=FALSE;

	/* we are looking for at least one stream with requested direction, inactive streams are ignored*/
	for(i=0;i<md->nstreams;++i){
		const SalStreamDescription *ss=&md->streams[i];
		if (ss->dir==stream_dir) found=TRUE;
		else{
			if (ss->dir!=SalStreamInactive) return FALSE;
		}
	}
	return found;
}

/*
static bool_t fmtp_equals(const char *p1, const char *p2){
	if (p1 && p2 && strcmp(p1,p2)==0) return TRUE;
	if (p1==NULL && p2==NULL) return TRUE;
	return FALSE;
}
*/

static bool_t payload_type_equals(const PayloadType *p1, const PayloadType *p2){
	if (p1->type!=p2->type) return FALSE;
	if (strcmp(p1->mime_type,p2->mime_type)!=0) return FALSE;
	if (p1->clock_rate!=p2->clock_rate) return FALSE;
	if (p1->channels!=p2->channels) return FALSE;
	/*
	 Do not compare fmtp right now: they are modified internally when the call is started
	*/
	/*
	if (!fmtp_equals(p1->recv_fmtp,p2->recv_fmtp) ||
	    !fmtp_equals(p1->send_fmtp,p2->send_fmtp))
		return FALSE;
	*/
	return TRUE;
}

static bool_t payload_list_equals(const MSList *l1, const MSList *l2){
	const MSList *e1,*e2;
	for(e1=l1,e2=l2;e1!=NULL && e2!=NULL; e1=e1->next,e2=e2->next){
		PayloadType *p1=(PayloadType*)e1->data;
		PayloadType *p2=(PayloadType*)e2->data;
		if (!payload_type_equals(p1,p2))
			return FALSE;
	}
	if (e1!=NULL || e2!=NULL){
		/*means one list is longer than the other*/
		abort();
		return FALSE;
	}
	return TRUE;
}

bool_t sal_stream_description_equals(const SalStreamDescription *sd1, const SalStreamDescription *sd2){
	if (sd1->proto!=sd2->proto) return FALSE;
	if (sd1->type!=sd2->type) return FALSE;
	if (strcmp(sd1->addr,sd2->addr)!=0) return FALSE;
	if (sd1->port!=sd2->port) return FALSE;
	if (!payload_list_equals(sd1->payloads,sd2->payloads)) return FALSE;
	if (sd1->bandwidth!=sd2->bandwidth) return FALSE;
	if (sd1->ptime!=sd2->ptime) return FALSE;
	/* compare candidates: TODO */
	if (sd1->dir!=sd2->dir) return FALSE;
	return TRUE;
}

bool_t sal_media_description_equals(const SalMediaDescription *md1, const SalMediaDescription *md2){
	int i;
	
	if (strcmp(md1->addr,md2->addr)!=0) return FALSE;
	if (md1->nstreams!=md2->nstreams) return FALSE;
	if (md1->bandwidth!=md2->bandwidth) return FALSE;
	for(i=0;i<md1->nstreams;++i){
		if (!sal_stream_description_equals(&md1->streams[i],&md2->streams[i]))
			return FALSE;
	}
	return TRUE;
}

static void assign_string(char **str, const char *arg){
	if (*str){
		ms_free(*str);
		*str=NULL;
	}
	if (arg)
		*str=ms_strdup(arg);
}

void sal_op_set_contact(SalOp *op, const char *contact){
	assign_string(&((SalOpBase*)op)->contact,contact);
}

void sal_op_set_route(SalOp *op, const char *route){
	assign_string(&((SalOpBase*)op)->route,route);
}

void sal_op_set_from(SalOp *op, const char *from){
	assign_string(&((SalOpBase*)op)->from,from);
}

void sal_op_set_to(SalOp *op, const char *to){
	assign_string(&((SalOpBase*)op)->to,to);
}

void sal_op_set_user_pointer(SalOp *op, void *up){
	((SalOpBase*)op)->user_pointer=up;
}

Sal *sal_op_get_sal(const SalOp *op){
	return ((SalOpBase*)op)->root;
}

const char *sal_op_get_from(const SalOp *op){
	return ((SalOpBase*)op)->from;
}

const char *sal_op_get_to(const SalOp *op){
	return ((SalOpBase*)op)->to;
}

const char *sal_op_get_contact(const SalOp *op){
	return ((SalOpBase*)op)->contact;
}

const char *sal_op_get_route(const SalOp *op){
	return ((SalOpBase*)op)->route;
}

const char *sal_op_get_remote_ua(const SalOp *op){
	return ((SalOpBase*)op)->remote_ua;
}

void *sal_op_get_user_pointer(const SalOp *op){
	return ((SalOpBase*)op)->user_pointer;
}

const char *sal_op_get_proxy(const SalOp *op){
	return ((SalOpBase*)op)->route;
}

const char *sal_op_get_network_origin(const SalOp *op){
	return ((SalOpBase*)op)->origin;
}

void __sal_op_init(SalOp *b, Sal *sal){
	memset(b,0,sizeof(SalOpBase));
	((SalOpBase*)b)->root=sal;
}

void __sal_op_set_network_origin(SalOp *op, const char *origin){
	assign_string(&((SalOpBase*)op)->origin,origin);
}


void __sal_op_free(SalOp *op){
	SalOpBase *b=(SalOpBase *)op;
	if (b->from) {
		ms_free(b->from);
		b->from=NULL;
	}
	if (b->to) {
		ms_free(b->to);
		b->to=NULL;
	}
	if (b->route) {
		ms_free(b->route);
		b->route=NULL;
	}
	if (b->contact) {
		ms_free(b->contact);
		b->contact=NULL;
	}
	if (b->origin){
		ms_free(b->origin);
		b->origin=NULL;
	}
	if (b->remote_ua){
		ms_free(b->remote_ua);
		b->remote_ua=NULL;
	}
	if (b->local_media)
		sal_media_description_unref(b->local_media);
	if (b->remote_media)
		sal_media_description_unref(b->remote_media);
	ms_free(op);
}
