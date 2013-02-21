/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2013 Belledonne Communications, Grenoble

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef _MEDIASTREAMER2_TESTER_H
#define _MEDIASTREAMER2_TESTER_H


typedef void (*test_function_t)(void);
typedef int (*test_suite_function_t)(const char *name);

typedef struct {
	const char *name;
	test_function_t func;
} test_t;

typedef struct {
	const char *name;
	unsigned int nb_tests;
	test_t *tests;
} test_suite_t;


extern test_suite_t dtmfgen_test_suite;


unsigned int mediastreamer2_tester_nb_test_suites(void);
unsigned int mediastreamer2_tester_nb_tests(const char *suite_name);
const char * mediastreamer2_tester_test_suite_name(unsigned int suite_index);
const char * mediastreamer2_tester_test_name(unsigned int suite_index, unsigned int test_index);
int mediastreamer2_tester_run_tests(const char *suite_name, const char *test_name);


#endif /* _MEDIASTREAMER2_TESTER_H */
