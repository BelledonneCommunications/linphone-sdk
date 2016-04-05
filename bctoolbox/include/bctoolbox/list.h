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

#ifndef BCTOOLBOX_LIST_H_
#define BCTOOLBOX_LIST_H_

#include "bctoolbox/port.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct _bctoolbox_list bctoolbox_list_t;

BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_append(bctoolbox_list_t * elem, void * data);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_prepend(bctoolbox_list_t * elem, void * data);
BCTOOLBOX_PUBLIC bctoolbox_list_t*  bctoolbox_list_prepend_link(bctoolbox_list_t* elem, bctoolbox_list_t *new_elem);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_last_elem(const bctoolbox_list_t *l);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_first_elem(const bctoolbox_list_t *l);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_free(bctoolbox_list_t * elem);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_concat(bctoolbox_list_t * first, bctoolbox_list_t * second);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_remove(bctoolbox_list_t * first, void *data);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_pop_front(bctoolbox_list_t *list, void **front_data);
BCTOOLBOX_PUBLIC int bctoolbox_list_size(const bctoolbox_list_t * first);
BCTOOLBOX_PUBLIC void bctoolbox_list_for_each(const bctoolbox_list_t * list, void (*func)(void *));
BCTOOLBOX_PUBLIC void bctoolbox_list_for_each2(const bctoolbox_list_t * list, void (*func)(void *, void *), void *user_data);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_remove_link(bctoolbox_list_t * list, bctoolbox_list_t * elem);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_delete_link(bctoolbox_list_t * list, bctoolbox_list_t * elem);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_find(bctoolbox_list_t * list, void *data);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_free(bctoolbox_list_t *list);
/*frees list elements and associated data, using the supplied function pointer*/
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_free_with_data(bctoolbox_list_t *list, void (*freefunc)(void*));

typedef  int (*bctoolbox_compare_func)(const void *, const void*);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_find_custom(const bctoolbox_list_t * list, bctoolbox_compare_func cmp, const void *user_data);
BCTOOLBOX_PUBLIC void * bctoolbox_list_nth_data(const bctoolbox_list_t * list, int index);
BCTOOLBOX_PUBLIC int bctoolbox_list_position(const bctoolbox_list_t * list, bctoolbox_list_t * elem);
BCTOOLBOX_PUBLIC int bctoolbox_list_index(const bctoolbox_list_t * list, void *data);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_insert_sorted(bctoolbox_list_t * list, void *data, bctoolbox_compare_func cmp);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_insert(bctoolbox_list_t * list, bctoolbox_list_t * before, void *data);
BCTOOLBOX_PUBLIC bctoolbox_list_t * bctoolbox_list_copy(const bctoolbox_list_t * list);
/*copy list elements and associated data, using the supplied function pointer*/
BCTOOLBOX_PUBLIC bctoolbox_list_t* bctoolbox_list_copy_with_data(const bctoolbox_list_t* list, void* (*copyfunc)(void*));

BCTOOLBOX_PUBLIC bctoolbox_list_t* bctoolbox_list_next(const bctoolbox_list_t *elem);
BCTOOLBOX_PUBLIC void* bctoolbox_list_get_data(const bctoolbox_list_t *elem);
	
#ifdef __cplusplus
}
#endif

#endif /* BCTOOLBOX_LIST_H_ */
