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
#include "bctoolbox/logging.h"
#include "bctoolbox/map.h"
#include <map>
#include <typeinfo> 

#define LOG_DOMAIN "bctoolbox"

typedef std::multimap<unsigned long long, void*> mmap_ullong_t;
typedef mmap_ullong_t::value_type pair_ullong_t;


extern "C" bctbx_map_t *bctbx_mmap_ullong_new(void) {
	return (bctbx_map_t *) new mmap_ullong_t();
}
extern "C" void bctbx_mmap_ullong_delete(bctbx_map_t *mmap) {
	delete (mmap_ullong_t *)mmap;
}
static bctbx_iterator_t * bctbx_map_insert_base(bctbx_map_t *map,const bctbx_pair_t *pair,bool_t returns_it) {
	mmap_ullong_t::iterator it;
	if (typeid((pair_ullong_t*)pair) != typeid(pair_ullong_t*)) {
		BCTBX_SLOGE(LOG_DOMAIN) << "Cannot insert pair ["<< pair << "] on map [" << map <<"] , wrong type";
		return NULL;
	} else {
		it = ((mmap_ullong_t *)map)->insert(*((pair_ullong_t*)pair));
	}
	if (returns_it) {
		return (bctbx_iterator_t *) new mmap_ullong_t::iterator(it);
	} else
		return NULL;
}

extern "C" void bctbx_map_insert(bctbx_map_t *map,const bctbx_pair_t *pair) {
	bctbx_map_insert_base(map,pair,FALSE);
}

extern "C" void bctbx_map_insert_and_delete(bctbx_map_t *map, bctbx_pair_t *pair) {
	bctbx_map_insert(map,pair);
	bctbx_pair_delete(pair);
}

extern "C" bctbx_iterator_t * bctbx_map_insert_and_delete_with_returned_it(bctbx_map_t *map, bctbx_pair_t *pair) {
	bctbx_iterator_t * it = bctbx_map_insert_base(map,pair,TRUE);
	bctbx_pair_delete(pair);
	return it;
}

extern "C" bctbx_iterator_t *bctbx_map_erase(bctbx_map_t *map,bctbx_iterator_t *it) {
	//bctbx_iterator_t *  next = (bctbx_iterator_t *) new mmap_ullong_t::iterator((*(mmap_ullong_t::iterator*)it));
	//next = bctbx_iterator_get_next(next);
	((mmap_ullong_t *)map)->erase((*(mmap_ullong_t::iterator*)it)++);
	//bctbx_iterator_delete(it);
	return it;
}
extern "C" bctbx_iterator_t *bctbx_map_begin(const bctbx_map_t *map) {
	return (bctbx_iterator_t *) new mmap_ullong_t::iterator(((mmap_ullong_t *)map)->begin());
}
extern "C"  bctbx_iterator_t * bctbx_map_end(const bctbx_map_t *map) {
	return (bctbx_iterator_t *) new mmap_ullong_t::iterator(((mmap_ullong_t *)map)->end());
}
/*iterator*/
extern "C" bctbx_pair_t *bctbx_iterator_get_pair(const bctbx_iterator_t *it) {
	return (bctbx_pair_t *)&(**((mmap_ullong_t::iterator*)it));
}
extern "C" bctbx_iterator_t *bctbx_iterator_get_next(bctbx_iterator_t *it) {
	((mmap_ullong_t::iterator*)it)->operator++();
	return it;
}
extern "C"  bctbx_iterator_t *bctbx_iterator_get_next_and_delete(bctbx_iterator_t *it) {
	bctbx_iterator_t * next = bctbx_iterator_get_next(it);
	bctbx_iterator_delete(it);
	return next;
}
extern "C" bool_t bctbx_iterator_equals(const bctbx_iterator_t *a,const bctbx_iterator_t *b) {
	return *(mmap_ullong_t::iterator*)a == *(mmap_ullong_t::iterator*)b;
}
extern "C" void bctbx_iterator_delete(bctbx_iterator_t *it) {
	delete ((mmap_ullong_t::iterator*)it);
}

/*pair*/	
extern "C" bctbx_pair_ullong_t * bctbx_pair_ullong_new(unsigned long long key,void *value) {
	return (bctbx_pair_ullong_t *) new pair_ullong_t(key,value);
	
}
extern "C" const unsigned long long bctbx_pair_ullong_get_first(const bctbx_pair_ullong_t  * pair) {
	return ((pair_ullong_t*)pair)->first;
}
extern "C" void* bctbx_pair_get_second(const bctbx_pair_t * pair) {
	return ((pair_ullong_t*)pair)->second;
}
extern "C" void bctbx_pair_delete(bctbx_pair_t * pair) {
	delete ((pair_ullong_t*)pair);
}

extern "C" bctbx_iterator_t * bctbx_map_find_custom(bctbx_map_t *map, bctbx_compare_func compare_func, const void *user_data) {
	bctbx_iterator_t * end = bctbx_map_end(map);
	
	for(bctbx_iterator_t * it = bctbx_map_begin(map);!bctbx_iterator_equals(it,end);) {
		if (compare_func(bctbx_pair_get_second(bctbx_iterator_get_pair(it)),user_data)==0) {
			bctbx_iterator_delete(end);
			return it;
		} else {
			it = bctbx_iterator_get_next(it);
		}
		
	}
	bctbx_iterator_delete(end);
	return NULL;
	
}
extern "C" size_t bctbx_map_size(const bctbx_map_t *map) {
	return ((mmap_ullong_t *)map)->size();
}
