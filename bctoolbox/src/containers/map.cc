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

typedef std::multimap<long, void*> mmap_long_t;
typedef mmap_long_t::value_type pair_long_t;


extern "C" bctoolbox_map_t *bctoolbox_mmap_long_new(void) {
	return (bctoolbox_map_t *) new mmap_long_t();
}
extern "C" void bctoolbox_mmap_long_delete(bctoolbox_map_t *mmap) {
	delete (mmap_long_t *)mmap;
}
extern "C" void bctoolbox_map_insert(bctoolbox_map_t *map,const bctoolbox_pair_t *pair) {
	if (typeid((pair_long_t*)pair) != typeid(pair_long_t*)) {
		BCTOOLBOX_SLOGE(LOG_DOMAIN) << "Cannot insert pair ["<< pair << "] on map [" << map <<"] , wrong type";
	} else {
		((mmap_long_t *)map)->insert(*((pair_long_t*)pair));
	}
}
extern "C" void bctoolbox_map_insert_and_delete(bctoolbox_map_t *map, bctoolbox_pair_t *pair) {
	bctoolbox_map_insert(map,pair);
	bctoolbox_pair_delete(pair);
}
extern "C" void bctoolbox_map_erase(bctoolbox_map_t *map,bctoolbox_iterator_t *it) {
	((mmap_long_t *)map)->erase((*(mmap_long_t::iterator*)it));
}
extern "C" bctoolbox_iterator_t *bctoolbox_map_begin(const bctoolbox_map_t *map) {
	return (bctoolbox_iterator_t *) new mmap_long_t::iterator(((mmap_long_t *)map)->begin());
}
extern "C"  bctoolbox_iterator_t * bctoolbox_map_end(const bctoolbox_map_t *map) {
	return (bctoolbox_iterator_t *) new mmap_long_t::iterator(((mmap_long_t *)map)->end());
}
/*iterator*/
extern "C" bctoolbox_pair_t *bctoolbox_iterator_get_pair(const bctoolbox_iterator_t *it) {
	return (bctoolbox_pair_t *)&(**((mmap_long_t::iterator*)it));
}
extern "C" bctoolbox_iterator_t *bctoolbox_iterator_get_next(const bctoolbox_iterator_t *it) {
	mmap_long_t::iterator *next = new mmap_long_t::iterator(*(mmap_long_t::iterator*)it);
	next->operator++();
	return (bctoolbox_iterator_t *)next;
}
extern "C"  bctoolbox_iterator_t *bctoolbox_iterator_get_next_and_delete(bctoolbox_iterator_t *it) {
	bctoolbox_iterator_t * next = bctoolbox_iterator_get_next(it);
	bctoolbox_iterator_delete(it);
	return next;
}
extern "C" bool_t bctoolbox_iterator_equals(const bctoolbox_iterator_t *a,const bctoolbox_iterator_t *b) {
	return (mmap_long_t::iterator*)a == (mmap_long_t::iterator*)b;
}
extern "C" void bctoolbox_iterator_delete(bctoolbox_iterator_t *it) {
	delete ((mmap_long_t::iterator*)it);
}

/*pair*/	
extern "C" bctoolbox_pair_long_t * bctoolbox_pair_long_new(long key,void *value) {
	return (bctoolbox_pair_long_t *) new pair_long_t(key,value);
	
}
extern "C" const long bctoolbox_pair_long_get_first(const bctoolbox_pair_long_t  * pair) {
	return ((pair_long_t*)pair)->first;
}
extern "C" void* bctoolbox_pair_get_second(const bctoolbox_pair_t * pair) {
	return ((pair_long_t*)pair)->second;
}
extern "C" void bctoolbox_pair_delete(bctoolbox_pair_t * pair) {
	delete ((pair_long_t*)pair);
}


