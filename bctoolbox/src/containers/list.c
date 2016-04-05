/*
	bctoolbox
    Copyright (C) 2016  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#define _CRT_RAND_S
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h> /*for gettimeofday*/
#include <dirent.h> /* available on POSIX system only */
#else
#include <direct.h>
#endif
#include "bctoolbox/port.h"
#include "bctoolbox/logging.h"
#include "bctoolbox/list.h"

struct _bctoolbox_list {
        struct _bctoolbox_list *next;
        struct _bctoolbox_list *prev;
        void *data;
};

bctoolbox_list_t* bctoolbox_list_new(void *data){
	bctoolbox_list_t* new_elem=bctoolbox_new0(bctoolbox_list_t,1);
	new_elem->data=data;
	return new_elem;
}
bctoolbox_list_t* bctoolbox_list_next(const bctoolbox_list_t *elem) {
	return elem->next;
}
void* bctoolbox_list_get_data(const bctoolbox_list_t *elem) {
	return elem->data;
}
bctoolbox_list_t*  bctoolbox_list_append_link(bctoolbox_list_t* elem,bctoolbox_list_t *new_elem){
	bctoolbox_list_t* it=elem;
	if (elem==NULL)  return new_elem;
	if (new_elem==NULL)  return elem;
	while (it->next!=NULL) it=bctoolbox_list_next(it);
	it->next=new_elem;
	new_elem->prev=it;
	return elem;
}

bctoolbox_list_t*  bctoolbox_list_append(bctoolbox_list_t* elem, void * data){
	bctoolbox_list_t* new_elem=bctoolbox_list_new(data);
	return bctoolbox_list_append_link(elem,new_elem);
}

bctoolbox_list_t*  bctoolbox_list_prepend_link(bctoolbox_list_t* elem, bctoolbox_list_t *new_elem){
	if (elem!=NULL) {
		new_elem->next=elem;
		elem->prev=new_elem;
	}
	return new_elem;
}

bctoolbox_list_t*  bctoolbox_list_prepend(bctoolbox_list_t* elem, void *data){
	return bctoolbox_list_prepend_link(elem,bctoolbox_list_new(data));
}

bctoolbox_list_t * bctoolbox_list_last_elem(const bctoolbox_list_t *l){
	if (l==NULL) return NULL;
	while(l->next){
		l=l->next;
	}
	return (bctoolbox_list_t*)l;
}

bctoolbox_list_t * bctoolbox_list_first_elem(const bctoolbox_list_t *l){
	if (l==NULL) return NULL;
	while(l->prev){
		l=l->prev;
	}
	return (bctoolbox_list_t*)l;
}

bctoolbox_list_t*  bctoolbox_list_concat(bctoolbox_list_t* first, bctoolbox_list_t* second){
	bctoolbox_list_t* it=first;
	if (it==NULL) return second;
	if (second==NULL) return first;
	while(it->next!=NULL) it=bctoolbox_list_next(it);
	it->next=second;
	second->prev=it;
	return first;
}

bctoolbox_list_t*  bctoolbox_list_free(bctoolbox_list_t* list){
	bctoolbox_list_t* elem = list;
	bctoolbox_list_t* tmp;
	if (list==NULL) return NULL;
	while(elem->next!=NULL) {
		tmp = elem;
		elem = elem->next;
		bctoolbox_free(tmp);
	}
	bctoolbox_free(elem);
	return NULL;
}

bctoolbox_list_t * bctoolbox_list_free_with_data(bctoolbox_list_t *list, void (*freefunc)(void*)){
	bctoolbox_list_t* elem = list;
	bctoolbox_list_t* tmp;
	if (list==NULL) return NULL;
	while(elem->next!=NULL) {
		tmp = elem;
		elem = elem->next;
		freefunc(tmp->data);
		bctoolbox_free(tmp);
	}
	freefunc(elem->data);
	bctoolbox_free(elem);
	return NULL;
}


bctoolbox_list_t*  _bctoolbox_list_remove(bctoolbox_list_t* first, void *data, int warn_if_not_found){
	bctoolbox_list_t* it;
	it=bctoolbox_list_find(first,data);
	if (it) return bctoolbox_list_delete_link(first,it);
	else if (warn_if_not_found){
		bctoolbox_warning("bctoolbox_list_remove: no element with %p data was in the list", data);
	}
	return first;
}

bctoolbox_list_t*  bctoolbox_list_remove(bctoolbox_list_t* first, void *data){
	return _bctoolbox_list_remove(first, data, TRUE);
}

int bctoolbox_list_size(const bctoolbox_list_t* first){
	int n=0;
	while(first!=NULL){
		++n;
		first=first->next;
	}
	return n;
}

void bctoolbox_list_for_each(const bctoolbox_list_t* list, void (*func)(void *)){
	for(;list!=NULL;list=list->next){
		func(list->data);
	}
}

void bctoolbox_list_for_each2(const bctoolbox_list_t* list, void (*func)(void *, void *), void *user_data){
	for(;list!=NULL;list=list->next){
		func(list->data,user_data);
	}
}

bctoolbox_list_t * bctoolbox_list_pop_front(bctoolbox_list_t *list, void **front_data){
	bctoolbox_list_t *front_elem=list;
	if (front_elem==NULL){
		*front_data=NULL;
		return NULL;
	}
	*front_data=front_elem->data;
	list=bctoolbox_list_remove_link(list,front_elem);
	bctoolbox_free(front_elem);
	return list;
}

bctoolbox_list_t* bctoolbox_list_remove_link(bctoolbox_list_t* list, bctoolbox_list_t* elem){
	bctoolbox_list_t* ret;
	if (elem==list){
		ret=elem->next;
		elem->prev=NULL;
		elem->next=NULL;
		if (ret!=NULL) ret->prev=NULL;
		return ret;
	}
	elem->prev->next=elem->next;
	if (elem->next!=NULL) elem->next->prev=elem->prev;
	elem->next=NULL;
	elem->prev=NULL;
	return list;
}

bctoolbox_list_t * bctoolbox_list_delete_link(bctoolbox_list_t* list, bctoolbox_list_t* elem){
	bctoolbox_list_t *ret=bctoolbox_list_remove_link(list,elem);
	bctoolbox_free(elem);
	return ret;
}

bctoolbox_list_t* bctoolbox_list_find(bctoolbox_list_t* list, void *data){
	for(;list!=NULL;list=list->next){
		if (list->data==data) return list;
	}
	return NULL;
}

bctoolbox_list_t* bctoolbox_list_find_custom(const bctoolbox_list_t* list, bctoolbox_compare_func compare_func, const void *user_data){
	for(;list!=NULL;list=list->next){
		if (compare_func(list->data,user_data)==0) return (bctoolbox_list_t *)list;
	}
	return NULL;
}

bctoolbox_list_t *bctoolbox_list_delete_custom(bctoolbox_list_t *list, bctoolbox_compare_func compare_func, const void *user_data){
	bctoolbox_list_t *elem=bctoolbox_list_find_custom(list,compare_func,user_data);
	if (elem!=NULL){
		list=bctoolbox_list_delete_link(list,elem);
	}
	return list;
}

void * bctoolbox_list_nth_data(const bctoolbox_list_t* list, int index){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (i==index) return list->data;
	}
	bctoolbox_error("bctoolbox_list_nth_data: no such index in list.");
	return NULL;
}

int bctoolbox_list_position(const bctoolbox_list_t* list, bctoolbox_list_t* elem){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (elem==list) return i;
	}
	bctoolbox_error("bctoolbox_list_position: no such element in list.");
	return -1;
}

int bctoolbox_list_index(const bctoolbox_list_t* list, void *data){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (data==list->data) return i;
	}
	bctoolbox_error("bctoolbox_list_index: no such element in list.");
	return -1;
}

bctoolbox_list_t* bctoolbox_list_insert_sorted(bctoolbox_list_t* list, void *data, int (*compare_func)(const void *, const void*)){
	bctoolbox_list_t* it,*previt=NULL;
	bctoolbox_list_t* nelem;
	bctoolbox_list_t* ret=list;
	if (list==NULL) return bctoolbox_list_append(list,data);
	else{
		nelem=bctoolbox_list_new(data);
		for(it=list;it!=NULL;it=it->next){
			previt=it;
			if (compare_func(data,it->data)<=0){
				nelem->prev=it->prev;
				nelem->next=it;
				if (it->prev!=NULL)
					it->prev->next=nelem;
				else{
					ret=nelem;
				}
				it->prev=nelem;
				return ret;
			}
		}
		previt->next=nelem;
		nelem->prev=previt;
	}
	return ret;
}

bctoolbox_list_t* bctoolbox_list_insert(bctoolbox_list_t* list, bctoolbox_list_t* before, void *data){
	bctoolbox_list_t* elem;
	if (list==NULL || before==NULL) return bctoolbox_list_append(list,data);
	for(elem=list;elem!=NULL;elem=bctoolbox_list_next(elem)){
		if (elem==before){
			if (elem->prev==NULL)
				return bctoolbox_list_prepend(list,data);
			else{
				bctoolbox_list_t* nelem=bctoolbox_list_new(data);
				nelem->prev=elem->prev;
				nelem->next=elem;
				elem->prev->next=nelem;
				elem->prev=nelem;
			}
		}
	}
	return list;
}

bctoolbox_list_t* bctoolbox_list_copy(const bctoolbox_list_t* list){
	bctoolbox_list_t* copy=NULL;
	const bctoolbox_list_t* iter;
	for(iter=list;iter!=NULL;iter=bctoolbox_list_next(iter)){
		copy=bctoolbox_list_append(copy,iter->data);
	}
	return copy;
}

bctoolbox_list_t* bctoolbox_list_copy_with_data(const bctoolbox_list_t* list, void* (*copyfunc)(void*)){
	bctoolbox_list_t* copy=NULL;
	const bctoolbox_list_t* iter;
	for(iter=list;iter!=NULL;iter=bctoolbox_list_next(iter)){
		copy=bctoolbox_list_append(copy,copyfunc(iter->data));
	}
	return copy;
}

