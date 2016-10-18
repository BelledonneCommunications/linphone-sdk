############################################################################
# CMakeLists.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

set(BASIC_SOURCE_FILES Basic/Basic.c)
set(AUTOMATED_SOURCE_FILES Automated/Automated.c)
set(CONSOLE_SOURCE_FILES Console/Console.c)
set(CURSES_SOURCE_FILES Curses/Curses.c)
set(FRAMEWORK_SOURCE_FILES
	Framework/CUError.c
	Framework/MyMem.c
	Framework/TestDB.c
	Framework/TestRun.c
	Framework/Util.c
)
set(FRAMEWORK_SYMBOLS "
	CU_add_suite
	CU_add_suite_with_setup_and_teardown
	CU_add_test
	CU_assertImplementation
	CU_cleanup_registry
	CU_clear_previous_results
	CU_compare_strings
	CU_create_new_registry
	CU_destroy_existing_registry
	CU_get_all_test_complete_handler
	CU_get_current_suite
	CU_get_current_test
	CU_get_elapsed_time
	CU_get_error
	CU_get_error_action
	CU_get_error_msg
	CU_get_fail_on_inactive
	CU_get_failure_list
	CU_get_number_of_asserts
	CU_get_number_of_failure_records
	CU_get_number_of_failures
	CU_get_number_of_successes
	CU_get_number_of_suites_failed
	CU_get_number_of_suites_run
	CU_get_number_of_tests_failed
	CU_get_number_of_tests_run
	CU_get_registry
	CU_get_run_results_string
	CU_get_run_summary
	CU_get_suite
	CU_get_suite_at_pos
	CU_get_suite_by_index
	CU_get_suite_by_name
	CU_get_suite_cleanup_failure_handler
	CU_get_suite_complete_handler
	CU_get_suite_init_failure_handler
	CU_get_suite_pos
	CU_get_suite_pos_by_name
	CU_get_suite_start_handler
	CU_get_test
	CU_get_test_at_pos
	CU_get_test_by_index
	CU_get_test_by_name
	CU_get_test_complete_handler
	CU_get_test_pos
	CU_get_test_pos_by_name
	CU_get_test_start_handler
	CU_initialize_registry
	CU_is_test_running
	CU_print_run_results
	CU_register_nsuites
	CU_register_suites
	CU_run_all_tests
	CU_run_suite
	CU_run_test
	CU_set_all_test_complete_handler
	CU_set_error
	CU_set_error_action
	CU_set_fail_on_inactive
	CU_set_registry
	CU_set_suite_active
	CU_set_suite_cleanup_failure_handler
	CU_set_suite_cleanupfunc
	CU_set_suite_complete_handler
	CU_set_suite_init_failure_handler
	CU_set_suite_initfunc
	CU_set_suite_name
	CU_set_suite_start_handler
	CU_set_test_active
	CU_set_test_complete_handler
	CU_set_test_func
	CU_set_test_name
	CU_set_test_start_handler
	CU_translate_special_characters
	CU_trim
	CU_trim_left
	CU_trim_right
")

set(SOURCE_FILES ${FRAMEWORK_SOURCE_FILES})
source_group(framework FILES ${FRAMEWORK_SOURCE_FILES})
if(ENABLE_BASIC)
	list(APPEND SOURCE_FILES ${BASIC_SOURCE_FILES})
	source_group(basic FILES ${BASIC_SOURCE_FILES})
	set(BASIC_SYMBOLS "
	CU_basic_get_mode
	CU_basic_run_suite
	CU_basic_run_test
	CU_basic_run_tests
	CU_basic_set_mode
	CU_basic_show_failures
	CU_set_trace_handler
	")
endif()
if(ENABLE_AUTOMATED)
	list(APPEND SOURCE_FILES ${AUTOMATED_SOURCE_FILES})
	source_group(automated FILES ${AUTOMATED_SOURCE_FILES})
	set(AUTOMATED_SYMBOLS "
	CU_automated_enable_junit_xml
	CU_automated_run_tests
	CU_list_tests_to_file
	CU_set_output_filename
	")
endif()
if(ENABLE_CONSOLE)
	list(APPEND SOURCE_FILES ${CONSOLE_SOURCE_FILES})
	source_group(console FILES ${CONSOLE_SOURCE_FILES})
	set(CONSOLE_SYMBOLS "
	CU_console_run_tests
	")
endif()
if(ENABLE_CURSES)
	list(APPEND SOURCE_FILES ${CURSES_SOURCE_FILES})
	source_group(curses FILES ${CURSES_SOURCE_FILES})
endif()
if(WIN32)
	configure_file(bcunit.def.in bcunit.def)
	list(APPEND SOURCE_FILES ${CMAKE_CURRENT_BINARY_DIR}/bcunit.def)
endif()


if(ENABLE_STATIC)
	add_library(bcunit STATIC ${SOURCE_FILES})
	target_link_libraries(bcunit ${CURSES_LIBRARIES})
else()
	add_library(bcunit SHARED ${SOURCE_FILES})
	set_target_properties(bcunit PROPERTIES VERSION 1.0.1)
	target_link_libraries(bcunit ${CURSES_LIBRARIES})
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bcunit.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
endif()
install(TARGETS bcunit
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)


if(ENABLE_TEST)
	add_library(bcunit_test STATIC ${FRAMEWORK_SOURCE_FILES})
	target_compile_definitions(bcunit_test PUBLIC MEMTRACE BCUNIT_BUILD_TESTS BCUNIT_DO_NOT_DEFINE_UNLESS_BUILDING_TESTS)
	target_include_directories(bcunit_test PUBLIC Test)

	add_subdirectory(Test)
endif()
