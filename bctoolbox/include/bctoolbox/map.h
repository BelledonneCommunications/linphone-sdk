/*
    bctoolbox mmap
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

#ifndef BCTBX_MMAP_H_
#define BCTBX_MMAP_H_
#include "bctoolbox/list.h"
#include "bctoolbox/port.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct _bctbx_map_t bctbx_map_t;
typedef struct _bctbx_pair_t bctbx_pair_t;
typedef struct _bctbx_iterator_t bctbx_iterator_t;



typedef struct _bctbx_mmap_ullong_t bctbx_mmap_ullong_t;
/*map*/
BCTBX_PUBLIC bctbx_map_t *bctbx_mmap_ullong_new(void);
BCTBX_PUBLIC void bctbx_mmap_ullong_delete(bctbx_map_t *mmap);
BCTBX_PUBLIC void bctbx_map_insert(bctbx_map_t *map,const bctbx_pair_t *pair);
/*same as insert, but also deleting pair*/
BCTBX_PUBLIC void bctbx_map_insert_and_delete(bctbx_map_t *map,bctbx_pair_t *pair);
/*same as insert and deleting pair with a newly allocated it returned */
BCTBX_PUBLIC bctbx_iterator_t * bctbx_map_insert_and_delete_with_returned_it(bctbx_map_t *map,bctbx_pair_t *pair);

/*at return, it point to the next element*/
BCTBX_PUBLIC bctbx_iterator_t *bctbx_map_erase(bctbx_map_t *map,bctbx_iterator_t *it);
/*return a new allocated iterator*/
BCTBX_PUBLIC bctbx_iterator_t *bctbx_map_begin(const bctbx_map_t *map);
/*return a new allocated iterator*/
BCTBX_PUBLIC bctbx_iterator_t *bctbx_map_end(const bctbx_map_t *map);
/*return a new allocated iterator or null*/
BCTBX_PUBLIC bctbx_iterator_t * bctbx_map_find_custom(bctbx_map_t *map, bctbx_compare_func compare_func, const void *user_data);

BCTBX_PUBLIC size_t bctbx_map_size(const bctbx_map_t *map);

		
	
/*iterator*/
BCTBX_PUBLIC bctbx_pair_t *bctbx_iterator_get_pair(const bctbx_iterator_t *it);
/*return same pointer but pointing to next*/
BCTBX_PUBLIC bctbx_iterator_t *bctbx_iterator_get_next(bctbx_iterator_t *it);
BCTBX_PUBLIC  bool_t bctbx_iterator_equals(const bctbx_iterator_t *a,const bctbx_iterator_t *b);
	
	

BCTBX_PUBLIC void bctbx_iterator_delete(bctbx_iterator_t *it);

/*pair*/	
typedef struct _bctbx_pair_ullong_t bctbx_pair_ullong_t; /*inherite from bctbx_pair_t*/
BCTBX_PUBLIC bctbx_pair_ullong_t * bctbx_pair_ullong_new(unsigned long long key,void *value);

BCTBX_PUBLIC void* bctbx_pair_get_second(const bctbx_pair_t * pair);
BCTBX_PUBLIC const unsigned long long bctbx_pair_ullong_get_first(const bctbx_pair_ullong_t * pair);
BCTBX_PUBLIC void bctbx_pair_delete(bctbx_pair_t * pair);


#ifdef __cplusplus
}
#endif

#endif /* BCTBX_LIST_H_ */
