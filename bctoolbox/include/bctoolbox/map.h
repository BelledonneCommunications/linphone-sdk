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

#ifndef BCTOOLBOX_MMAP_H_
#define BCTOOLBOX_MMAP_H_
#include "bctoolbox/list.h"
#include "bctoolbox/port.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct _bctoolbox_map_t bctoolbox_map_t;
typedef struct _bctoolbox_pair_t bctoolbox_pair_t;
typedef struct _bctoolbox_iterator_t bctoolbox_iterator_t;



typedef struct _bctoolbox_mmap_long_t bctoolbox_mmap_long_t;
/*map*/
BCTOOLBOX_PUBLIC bctoolbox_map_t *bctoolbox_mmap_long_new(void);
BCTOOLBOX_PUBLIC void bctoolbox_mmap_long_delete(bctoolbox_map_t *mmap);
BCTOOLBOX_PUBLIC void bctoolbox_map_insert(bctoolbox_map_t *map,const bctoolbox_pair_t *pair);
/*same as insert, but also deleting pair*/
BCTOOLBOX_PUBLIC void bctoolbox_map_insert_and_delete(bctoolbox_map_t *map,bctoolbox_pair_t *pair);
/*same as insert and deleting pair with a newly allocated it returned */
BCTOOLBOX_PUBLIC bctoolbox_iterator_t * bctoolbox_map_insert_and_delete_with_returned_it(bctoolbox_map_t *map,bctoolbox_pair_t *pair);

/*at return, it point to the next element*/
BCTOOLBOX_PUBLIC bctoolbox_iterator_t *bctoolbox_map_erase(bctoolbox_map_t *map,bctoolbox_iterator_t *it);
/*return a new allocated iterator*/
BCTOOLBOX_PUBLIC bctoolbox_iterator_t *bctoolbox_map_begin(const bctoolbox_map_t *map);
/*return a new allocated iterator*/
BCTOOLBOX_PUBLIC bctoolbox_iterator_t *bctoolbox_map_end(const bctoolbox_map_t *map);
/*return a new allocated iterator or null*/
BCTOOLBOX_PUBLIC bctoolbox_iterator_t * bctoolbox_map_find_custom(bctoolbox_map_t *map, bctoolbox_compare_func compare_func, const void *user_data);

BCTOOLBOX_PUBLIC size_t bctoolbox_map_size(const bctoolbox_map_t *map);

		
	
/*iterator*/
BCTOOLBOX_PUBLIC bctoolbox_pair_t *bctoolbox_iterator_get_pair(const bctoolbox_iterator_t *it);
/*return same pointer but pointing to next*/
BCTOOLBOX_PUBLIC bctoolbox_iterator_t *bctoolbox_iterator_get_next(bctoolbox_iterator_t *it);
BCTOOLBOX_PUBLIC  bool_t bctoolbox_iterator_equals(const bctoolbox_iterator_t *a,const bctoolbox_iterator_t *b);
	
	

BCTOOLBOX_PUBLIC void bctoolbox_iterator_delete(bctoolbox_iterator_t *it);

/*pair*/	
typedef struct _bctoolbox_pair_long_t bctoolbox_pair_long_t; /*inherite from bctoolbox_pair_t*/
BCTOOLBOX_PUBLIC bctoolbox_pair_long_t * bctoolbox_pair_long_new(long key,void *value);

BCTOOLBOX_PUBLIC void* bctoolbox_pair_get_second(const bctoolbox_pair_t * pair);
BCTOOLBOX_PUBLIC const long bctoolbox_pair_long_get_first(const bctoolbox_pair_long_t * pair);
BCTOOLBOX_PUBLIC void bctoolbox_pair_delete(bctoolbox_pair_t * pair);


#ifdef __cplusplus
}
#endif

#endif /* BCTOOLBOX_LIST_H_ */
