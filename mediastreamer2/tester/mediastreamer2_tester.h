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


#include "CUnit/Basic.h"
#include <mediastreamer2/mediastream.h>



typedef void (*test_function_t)(void);
typedef int (*test_suite_function_t)(const char *name);

typedef struct {
	const char *name;
	test_function_t func;
} test_t;

typedef struct {
	const char *name;
	CU_InitializeFunc init_func;
	CU_CleanupFunc cleanup_func;
	int nb_tests;
	test_t *tests;
} test_suite_t;


#ifdef __cplusplus
extern "C" {
#endif

extern test_suite_t basic_audio_test_suite;
extern test_suite_t sound_card_test_suite;
extern test_suite_t adaptive_test_suite;
extern test_suite_t audio_stream_test_suite;
extern test_suite_t video_stream_test_suite;
extern test_suite_t framework_test_suite;
extern test_suite_t player_test_suite;
#ifdef __ARM_NEON__
extern test_suite_t neon_test_suite;
#endif

#define CU_ASSERT_IN_RANGE(value, inf, sup) \
		CU_ASSERT_TRUE(value >= inf); \
		CU_ASSERT_TRUE(value <= sup);

extern int mediastreamer2_tester_nb_test_suites(void);
extern int mediastreamer2_tester_nb_tests(const char *suite_name);
extern const char * mediastreamer2_tester_test_suite_name(int suite_index);
extern const char * mediastreamer2_tester_test_name(const char *suite_name, int test_index);
extern void mediastreamer2_tester_init(void);
extern void mediastreamer2_tester_uninit(void);
extern int mediastreamer2_tester_run_tests(const char *suite_name, const char *test_name);

extern const char* mediastreamer2_tester_get_file_root();
/* without / at the end */
extern void mediastreamer2_tester_set_file_root(const char* fileroot);
extern const char* mediastreamer2_tester_get_writable_dir();
extern void mediastreamer2_tester_set_writable_dir(const char* writable_dir);

#if TARGET_OS_MAC || TARGET_OS_IPHONE
    int apple_main(int argc, char *argv[]);
#endif


#ifdef __cplusplus
};
#endif


#endif /* _MEDIASTREAMER2_TESTER_H */
