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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

cmake_minimum_required(VERSION 2.8)
project(BZRTP C)


option(ENABLE_STATIC "Build static library (default is shared library)." NO)
option(ENABLE_UNIT_TESTS "Enable compilation of unit tests." NO)


list(APPEND CMAKE_MODULE_PATH ${CMAKE_PREFIX_PATH}/share/cmake/Modules)
if(ENABLE_TESTS)
find_package(CUnit)
	if(CUNIT_FOUND)
		check_library_exists(${CUNIT_LIBRARIES} "CU_add_suite" "" HAVE_CU_ADD_SUITE)
		check_library_exists(${CUNIT_LIBRARIES} "CU_get_suite" "" HAVE_CU_GET_SUITE)
		check_library_exists(${CUNIT_LIBRARIES} "CU_curses_run_tests" "" HAVE_CU_CURSES)
	else()
		message(WARNING "Could not find cunit framework, tests will not be compiled.")
		set(ENABLE_TESTS OFF CACHE BOOL "Enable compilation of tests" FORCE)
	endif()
endif()
find_package(PolarSSL REQUIRED)
find_package(XML2)

if(XML2_FOUND)
	set(HAVE_LIBXML2 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/config.h PROPERTIES GENERATED ON)
add_definitions("-DHAVE_CONFIG_H")


include_directories(
	include
	${CMAKE_CURRENT_BINARY_DIR}
)


set(INCLUDES ${CUNIT_INCLUDE_DIRS} ${POLARSSL_INCLUDE_DIRS} ${XML2_INCLUDE_DIRS})
include_directories(${INCLUDES})
set(LIBS ${CUNIT_LIBRARIES} ${POLARSSL_LIBRARIES} ${XML2_LIBRARIES})


add_subdirectory(include)
add_subdirectory(src)
if(ENABLE_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()

install(FILES FindBZRTP.cmake
	DESTINATION share/cmake/Modules
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
