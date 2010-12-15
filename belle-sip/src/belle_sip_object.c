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

static uint8_t *find_type(belle_sip_object_t *obj, belle_sip_type_id_t id){
	int i;
	for(i=0;i<sizeof(obj->type_ids);++i){
		if (obj->type_ids[i]==(uint8_t)id)
			return &obj->type_ids[i];
	}
	return NULL;
}

void _belle_sip_object_init_type(belle_sip_object_t *obj, belle_sip_type_id_t id){
	uint8_t * t=find_type(obj,id);
	if (t!=NULL) belle_sip_fatal("This object already inherits type %i",id);
	t=find_type(obj,0);
	if (t==NULL) belle_sip_fatal("This object has too much inheritance !");
	*t=id;
}

belle_sip_object_t * _belle_sip_object_new(size_t objsize, belle_sip_type_id_t id, belle_sip_object_destroy_t destroy_func, int initially_unowed){
	belle_sip_object_t *obj=(belle_sip_object_t *)belle_sip_malloc0(objsize);
	obj->type_ids[0]=id;
	obj->ref=initially_unowed ? 0 : 1;
	obj->destroy=destroy_func;
	return obj;
}

int belle_sip_object_is_unowed(const belle_sip_object_t *obj){
	return obj->ref==0;
}

belle_sip_object_t * _belle_sip_object_ref(belle_sip_object_t *obj){
	obj->ref++;
	return obj;
}

void _belle_sip_object_unref(belle_sip_object_t *obj){
	if (obj->ref==0){
		belle_sip_warning("Destroying unowed object");
		belle_sip_object_destroy(obj);
		return;
	}
	obj->ref--;
	if (obj->ref==0){
		belle_sip_object_destroy(obj);
	}
}

void _belle_sip_object_destroy(belle_sip_object_t *obj){
	if (obj->ref!=0){
		belle_sip_error("Destroying referenced object !");
		if (obj->destroy) obj->destroy(obj);
		belle_sip_free(obj);
	}
}

void *belle_sip_object_cast(belle_sip_object_t *obj, belle_sip_type_id_t id, const char *castname, const char *file, int fileno){
	if (find_type(obj,id)==NULL){
		belle_sip_fatal("Bad cast to %s at %s:%i",castname,file,fileno);
		return NULL;
	}
	return obj;
}
void belle_sip_object_init(belle_sip_object_t *obj) {
	belle_sip_object_init_type(obj,belle_sip_object_t);
}

