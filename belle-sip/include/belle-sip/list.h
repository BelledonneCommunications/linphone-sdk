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

#ifndef BELLE_SIP_LIST_H_
#define BELLE_SIP_LIST_H_
struct _belle_sip_list {
	struct _belle_sip_list *next;
	struct _belle_sip_list *prev;
	void *data;
};
typedef struct _belle_sip_list belle_sip_list_t;

belle_sip_list_t * belle_sip_list_append(belle_sip_list_t * elem, void * data);
belle_sip_list_t * belle_sip_list_prepend(belle_sip_list_t * elem, void * data);
belle_sip_list_t * belle_sip_list_free(belle_sip_list_t * elem);
belle_sip_list_t * belle_sip_list_concat(belle_sip_list_t * first, belle_sip_list_t * second);
belle_sip_list_t * belle_sip_list_remove(belle_sip_list_t * first, void *data);
int belle_sip_list_size(const belle_sip_list_t * first);
void belle_sip_list_for_each(const belle_sip_list_t * list, void (*func)(void *));
void belle_sip_list_for_each2(const belle_sip_list_t * list, void (*func)(void *, void *), void *user_data);
belle_sip_list_t * belle_sip_list_remove_link(belle_sip_list_t * list, belle_sip_list_t * elem);
belle_sip_list_t * belle_sip_list_delete_link(belle_sip_list_t * list, belle_sip_list_t * elem);
belle_sip_list_t * belle_sip_list_find(belle_sip_list_t * list, void *data);
belle_sip_list_t * belle_sip_list_free(belle_sip_list_t *list);
/*frees list elements and associated data, using the supplied function pointer*/
belle_sip_list_t * belle_sip_list_free_with_data(belle_sip_list_t *list, void (*freefunc)(void*));

typedef  int (*belle_sip_compare_func)(const void *, const void*);
belle_sip_list_t * belle_sip_list_find_custom(belle_sip_list_t * list, belle_sip_compare_func cmp, const void *user_data);
void * belle_sip_list_nth_data(const belle_sip_list_t * list, int index);
int belle_sip_list_position(const belle_sip_list_t * list, belle_sip_list_t * elem);
int belle_sip_list_index(const belle_sip_list_t * list, void *data);
belle_sip_list_t * belle_sip_list_insert_sorted(belle_sip_list_t * list, void *data, belle_sip_compare_func cmp);
belle_sip_list_t * belle_sip_list_insert(belle_sip_list_t * list, belle_sip_list_t * before, void *data);
belle_sip_list_t * belle_sip_list_copy(const belle_sip_list_t * list);


#endif /* BELLE_SIP_LIST_H_ */
