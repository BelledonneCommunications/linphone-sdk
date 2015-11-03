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

cmake_minimum_required(VERSION 3.0)
project(BZRTP C)
set(BZRTP_VERSION 1.0.3)


option(ENABLE_STATIC "Build static library (default is shared library)." NO)
option(ENABLE_STRICT "Build with strict compile options." YES)
option(ENABLE_TESTS "Enable compilation of unit tests." NO)


macro(apply_compile_flags SOURCE_FILES)
	if(${SOURCE_FILES})
		set(options "")
		foreach(a ${ARGV})
			if(STRICT_OPTIONS_${a})
				string(REPLACE ";" " " options_${a} "${STRICT_OPTIONS_${a}}")
				set(options "${options} ${options_${a}}")
			endif()
		endforeach()
		if(options)
			set_source_files_properties(${${SOURCE_FILES}} PROPERTIES COMPILE_FLAGS "${options}")
		endif()
	endif()
endmacro()


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(MSVC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/MSVC")
if(MSVC)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${MSVC_INCLUDE_DIR})
endif()

include(CheckLibraryExists)
check_library_exists("m" "sqrt" "" HAVE_SQRT)
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

set(STRICT_OPTIONS_CPP )
if(NOT MSVC)
	list(APPEND STRICT_OPTIONS_CPP "-Wall")
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
		list(APPEND STRICT_OPTIONS_CPP "-Qunused-arguments")
	endif()
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "-Werror")
	endif()
endif()
if(STRICT_OPTIONS_CPP)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_CPP)
	string(REPLACE ";" " " STRICT_OPTIONS_CPP "${STRICT_OPTIONS_CPP}")
endif()


include_directories(
	include
	${CMAKE_CURRENT_BINARY_DIR}
	${POLARSSL_INCLUDE_DIRS}
)

set(LIBS ${POLARSSL_LIBRARIES})
if(CUNIT_FOUND)
	include_directories(${CUNIT_INCLUDE_DIRS})
	list(APPEND LIBS ${CUNIT_LIBRARIES})
endif()
if(XML2_FOUND)
	include_directories(${XML2_INCLUDE_DIRS})
	list(APPEND LIBS ${XML2_LIBRARIES})
endif()
if(MSVC)
	include_directories(${MSVC_INCLUDE_DIR})
endif()


if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	set(EXPORT_TARGETS_NAME "LinphoneBuilder")
else()
	set(EXPORT_TARGETS_NAME "BZRTP")
endif()


add_subdirectory(include)
add_subdirectory(src)
if(ENABLE_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()


include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/BZRTPConfigVersion.cmake"
	VERSION ${BZRTP_VERSION}
	COMPATIBILITY AnyNewerVersion
)
export(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/BZRTPTargets.cmake"
)
configure_file(cmake/BZRTPConfig.cmake
	"${CMAKE_CURRENT_BINARY_DIR}/BZRTPConfig.cmake"
	COPYONLY
)

set(ConfigPackageLocation lib/cmake/BZRTP)
install(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE BZRTPTargets.cmake
	DESTINATION ${ConfigPackageLocation}
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/BZRTPConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/BZRTPConfigVersion.cmake"
	DESTINATION ${ConfigPackageLocation}
)

