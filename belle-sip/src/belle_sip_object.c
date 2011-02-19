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

#include "belle_sip_internal.h"

static int has_type(belle_sip_object_t *obj, belle_sip_type_id_t id){
	belle_sip_object_vptr_t *vptr=obj->vptr;
	
	while(vptr!=NULL){
		if (vptr->id==id) return TRUE;
		vptr=vptr->parent;
	}
	return FALSE;
}

belle_sip_object_t * _belle_sip_object_new(size_t objsize, belle_sip_object_vptr_t *vptr, int initially_unowed){
	belle_sip_object_t *obj=(belle_sip_object_t *)belle_sip_malloc0(objsize);
	obj->ref=initially_unowed ? 0 : 1;
	obj->vptr=vptr;
	obj->size=objsize;
	return obj;
}

int belle_sip_object_is_unowed(const belle_sip_object_t *obj){
	return obj->ref==0;
}

belle_sip_object_t * belle_sip_object_ref(void *obj){
	BELLE_SIP_OBJECT(obj)->ref++;
	return obj;
}

void belle_sip_object_unref(void *ptr){
	belle_sip_object_t *obj=BELLE_SIP_OBJECT(ptr);
	if (obj->ref==0){
		belle_sip_warning("Destroying unowed object");
		belle_sip_object_delete(obj);
		return;
	}
	obj->ref--;
	if (obj->ref==0){
		belle_sip_object_delete(obj);
	}
}

static void _belle_sip_object_uninit(belle_sip_object_t *obj){
	if (obj->name)
		belle_sip_free(obj->name);
}

static void _belle_sip_object_clone(belle_sip_object_t *obj, const belle_sip_object_t *orig){
	if (orig->name!=NULL) obj->name=belle_sip_strdup(obj->name);
}

belle_sip_object_vptr_t belle_sip_object_t_vptr={
	BELLE_SIP_TYPE_ID(belle_sip_object_t),
	NULL, /*no parent, it's god*/
	NULL,
	_belle_sip_object_uninit,
	_belle_sip_object_clone
};

void belle_sip_object_delete(void *ptr){
	belle_sip_object_t *obj=BELLE_SIP_OBJECT(ptr);
	belle_sip_object_vptr_t *vptr;
	if (obj->ref!=0){
		belle_sip_error("Destroying referenced object !");
		vptr=obj->vptr;
		while(vptr!=NULL){
			if (vptr->destroy) vptr->destroy(obj);
			vptr=vptr->parent;
		}
		belle_sip_free(obj);
	}
}

belle_sip_object_t *belle_sip_object_clone(const belle_sip_object_t *obj){
	belle_sip_object_t *newobj;
	belle_sip_object_vptr_t *vptr;
	
	newobj=belle_sip_malloc0(obj->size);
	newobj->ref=1;
	newobj->vptr=obj->vptr;
	
	vptr=obj->vptr;
	while(vptr!=NULL){
		if (vptr->clone==NULL){
			belle_sip_fatal("Object of type %i cannot be cloned, it does not provide a clone() implementation.",vptr->id);
			return NULL;
		}else vptr->clone(newobj,obj);
	}
	return newobj;
}

void *belle_sip_object_cast(belle_sip_object_t *obj, belle_sip_type_id_t id, const char *castname, const char *file, int fileno){
	if (has_type(obj,id)==0){
		belle_sip_fatal("Bad cast to %s at %s:%i",castname,file,fileno);
		return NULL;
	}
	return obj;
}


void belle_sip_object_set_name(belle_sip_object_t* object,const char* name) {
	if (object->name) {
		belle_sip_free(object->name);
		object->name=NULL;
	}
	if (name)
		object->name=belle_sip_strdup(name);
}

const char* belle_sip_object_get_name(belle_sip_object_t* object) {
	return object->name;
}
