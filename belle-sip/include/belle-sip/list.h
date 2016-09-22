/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

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

#ifndef BELLE_SIP_LIST_H_
#define BELLE_SIP_LIST_H_

#include "bctoolbox/list.h"

typedef struct _bctbx_list belle_sip_list_t;

#define belle_sip_list_new bctbx_list_new
#define belle_sip_list_append bctbx_list_append
#define belle_sip_list_prepend  bctbx_list_prepend
#define belle_sip_list_prepend_link bctbx_list_prepend_link
#define belle_sip_list_last_elem bctbx_list_last_elem
#define belle_sip_list_free bctbx_list_free
#define belle_sip_list_concat bctbx_list_concat
#define belle_sip_list_remove bctbx_list_remove
#define belle_sip_list_pop_front bctbx_list_pop_front
#define belle_sip_list_size bctbx_list_size
#define belle_sip_list_for_each bctbx_list_for_each
#define belle_sip_list_for_each2 bctbx_list_for_each2
#define belle_sip_list_remove_link bctbx_list_unlink
#define belle_sip_list_delete_link bctbx_list_erase_link
#define belle_sip_list_find bctbx_list_find
#define belle_sip_list_free bctbx_list_free
#define belle_sip_list_free_with_data bctbx_list_free_with_data

#define belle_sip_compare_func bctbx_compare_func
#define belle_sip_list_find_custom bctbx_list_find_custom
#define belle_sip_list_nth_data bctbx_list_nth_data
#define belle_sip_list_position bctbx_list_position
#define belle_sip_list_index bctbx_list_index
#define belle_sip_list_insert_sorted bctbx_list_insert_sorted
#define belle_sip_list_insert bctbx_list_insert
#define belle_sip_list_copy bctbx_list_copy
#define belle_sip_list_copy_with_data bctbx_list_copy_with_data

#endif /* BELLE_SIP_LIST_H_ */
