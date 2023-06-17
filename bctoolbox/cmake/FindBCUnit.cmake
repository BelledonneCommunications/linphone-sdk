############################################################################
# FindBCUnit.cmake
# Copyright (C) 2017-2023  Belledonne Communications, Grenoble France
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
#
# Find the bcunit library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  bcunit - If the bcunit library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  BCUnit_FOUND - The bcunit library has been found
#  BCUnit_TARGET - The name of the CMake target for the bcunit library
#  HAVE_CU_GET_SUITE - If the bcunit library includes the CU_get_suite symbol
#  HAVE_CU_CURSES - If the bcunit library includes the CU_curses_run_tests symbol
#  HAVE_CU_SET_TRACE_HANDLER - If the bcunit library includes the CU_set_trace_handler symbol


if(TARGET bcunit)

	include(FindPackageHandleStandardArgs)
	set(BCUnit_TARGET bcunit)
	set(HAVE_CU_GET_SUITE TRUE)
	set(HAVE_CU_CURSES ${ENABLE_BCUNIT_CURSES})
	set(HAVE_CU_SET_TRACE_HANDLER ${ENABLE_BCUNIT_BASIC})
	set(_BCUnit_REQUIRED_VARS BCUnit_TARGET HAVE_CU_GET_SUITE)
	set(_BCUnit_CACHE_VARS
		${_BCUnit_REQUIRED_VARS} HAVE_CU_CURSES HAVE_CU_SET_TRACE_HANDLER
	)
	find_package_handle_standard_args(BCUnit
		REQUIRED_VARS ${_BCUnit_REQUIRED_VARS}
	)
	mark_as_advanced(${_BCUnit_CACHE_VARS})

else()

	set(_OPTIONS CONFIG)
	if(BCUnit_FIND_REQUIRED)
		list(APPEND _OPTIONS REQUIRED)
	endif()
	if(BCUnit_FIND_QUIETLY)
		list(APPEND _OPTIONS QUIET)
	endif()
	if(BCUnit_FIND_VERSION)
		list(PREPEND _OPTIONS "${BCUnit_FIND_VERSION}")
	endif()
	if(BCUnit_FIND_EXACT)
		list(APPEND _OPTIONS EXACT)
	endif()

	find_package(BCUnit ${_OPTIONS})

	cmake_push_check_state(RESET)
	get_target_property(BCUnit_INCLUDE_DIRS "${BCUnit_TARGET}" INTERFACE_INCLUDE_DIRECTORIES)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${BCUnit_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES "${BCUnit_TARGET}")
	check_symbol_exists("CU_get_suite" "BCUnit/BCUnit.h" HAVE_CU_GET_SUITE)
	check_symbol_exists("CU_curses_run_tests" "BCUnit/BCUnit.h" HAVE_CU_CURSES)
	check_symbol_exists("CU_set_trace_handler" "BCUnit/Util.h" HAVE_CU_SET_TRACE_HANDLER)
	cmake_pop_check_state()

endif()
