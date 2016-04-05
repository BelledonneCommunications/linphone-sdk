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

#include "bctoolbox_tester.h"
#include "bctoolbox/map.h"
#include "bctoolbox/list.h"

void multimap_insert(void) {
	bctoolbox_map_t *mmap = bctoolbox_mmap_long_new();
	bctoolbox_list_t *ref = NULL;
	bctoolbox_iterator_t *it;
	long i=0;
	int N = 100;
	
	for(i=0;i<N;i++) {
		bctoolbox_pair_t* pair = (bctoolbox_pair_t*)bctoolbox_pair_long_new(i, (void*)((long)i));
		ref = bctoolbox_list_append(ref, (void*)i);
		bctoolbox_map_insert(mmap, pair);
		bctoolbox_pair_delete(pair);
	}
	
	it = bctoolbox_map_begin(mmap);
	
	for(ref = bctoolbox_list_first_elem(ref);ref!=NULL;ref=bctoolbox_list_next(ref)) {
		BC_ASSERT_EQUAL((long)bctoolbox_list_get_data(ref)
						,(long)bctoolbox_pair_get_second(bctoolbox_iterator_get_pair(it))
		,long, "%lu");
		it = bctoolbox_iterator_get_next_and_delete(it);
	}
	bctoolbox_mmap_long_delete(mmap);
}

void multimap_erase(void) {
	bctoolbox_map_t *mmap = bctoolbox_mmap_long_new();
	bctoolbox_iterator_t *it;
	bctoolbox_iterator_t *end;
	long i=0;
	int N = 100;
	
	for(i=0;i<N;i++) {
		bctoolbox_map_insert_and_delete(mmap, (bctoolbox_pair_t*)bctoolbox_pair_long_new(i, (void*)((long)i)));
	}
	
	end= bctoolbox_map_end(mmap);
	
	for(it = bctoolbox_map_begin(mmap);!bctoolbox_iterator_equals(it,end);) {
		long value = (long)bctoolbox_pair_get_second(bctoolbox_iterator_get_pair(it));
		if (value < N/2) {
			bctoolbox_iterator_t *cur = it;
			it=bctoolbox_iterator_get_next(it);
			bctoolbox_map_erase(mmap, cur);
			bctoolbox_iterator_delete(cur);
		} else {
			break;
		}
	}
	it = bctoolbox_map_begin(mmap);
	BC_ASSERT_EQUAL((long)bctoolbox_pair_get_second(bctoolbox_iterator_get_pair(it))
					, N/2
					,long, "%lu");
					
	bctoolbox_mmap_long_delete(mmap);
	bctoolbox_iterator_delete(end);
	
}
static test_t container_tests[] = {
	TEST_NO_TAG("mmap", multimap_insert),
	TEST_NO_TAG("mmap", multimap_erase),
};

test_suite_t containers_test_suite = {"Containers", NULL, NULL, NULL, NULL,
							   sizeof(container_tests) / sizeof(container_tests[0]), container_tests};
