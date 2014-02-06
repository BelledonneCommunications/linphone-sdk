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
project(CUNIT C)


set(PACKAGE "CUnit")
set(PACKAGE_NAME "${PACKAGE}")
set(PACKAGE_VERSION "2.1-2")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "")
set(PACKAGE_TARNAME "cunit")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")


option(ENABLE_AUTOMATED "Compile CUnit automated interface" ON)
option(ENABLE_BASIC "Compile CUnit basic interface" ON)
option(ENABLE_CONSOLE "Compile CUnit console interface" ON)
option(ENABLE_CURSES "Compile CUnit curses interface" OFF)
option(ENABLE_EXAMPLES "Compile CUnit example programs" OFF)
option(ENABLE_TEST "Compile CUnit internal test program" OFF)
option(ENABLE_MEMTRACE "Enable CUnit internal memory tracking" OFF)
option(ENABLE_DEPRECATED "Enable use of deprecated v1.1 names" OFF)


include(CheckIncludeFile)
include(CheckFunctionExists)

check_include_file("assert.h" HAVE_ASSERT_H)
check_include_file("ctype.h" HAVE_CTYPE_H)
check_include_file("dlfcn.h" HAVE_DLFCN_H)
check_include_file("file.h" HAVE_FILE_H)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("math.h" HAVE_MATH_H)
check_include_file("memory.h" HAVE_MEMORY_H)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("stdio.h" HAVE_STDIO_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("strings.h" HAVE_STRINGS_H)
check_include_file("string.h" HAVE_STRING_H)
check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("unistd.h" HAVE_UNISTD_H)

macro(check_function_exists_fail FUNCTION)
	string(TOUPPER ${FUNCTION} UPPER_FUNCTION)
	check_function_exists("${FUNCTION}" HAVE_${UPPER_FUNCTION})
	if(NOT ${HAVE_${UPPER_FUNCTION}})
		message(FATAL_ERROR "${FUNCTION} not found")
	endif(NOT ${HAVE_${UPPER_FUNCTION}})
endmacro(check_function_exists_fail)
check_function_exists_fail("malloc")
check_function_exists_fail("free")
check_function_exists_fail("calloc")
check_function_exists_fail("realloc")
check_function_exists_fail("strcpy")
check_function_exists_fail("strerror")
check_function_exists_fail("fopen")
check_function_exists_fail("fclose")
check_function_exists_fail("fprintf")
check_function_exists_fail("snprintf")
check_function_exists_fail("setvbuf")
check_function_exists_fail("time")
check_function_exists_fail("ctime")


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)


set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${prefix}/bin)
set(libdir ${prefix}/lib)
set(includedir ${prefix}/include)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cunit.pc.in ${CMAKE_CURRENT_BINARY_DIR}/cunit.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cunit.pc DESTINATION lib/pkgconfig)


add_definitions(-Wall -W -pedantic -Wshadow -ansi -std=c99)
if(${ENABLE_MEMTRACE})
	add_definitions(-DMEMTRACE)
endif(${ENABLE_MEMTRACE})
if(${ENABLE_DEPRECATED})
	add_definitions(-DUSE_DEPRECATED_CUNIT_NAMES)
endif(${ENABLE_DEPRECATED})
if(${ENABLE_CURSES})
	set(CURSES_NEED_NCURSES 1)
	find_package(Curses)
	if(NOT ${CURSES_FOUND})
		message("Disabling curses as it has not been found!")
		set(ENABLE_CURSES 0)
	endif(NOT ${CURSES_FOUND})
endif(${ENABLE_CURSES})

include_directories(
	CUnit/Headers
	${CMAKE_CURRENT_BINARY_DIR}
)


add_subdirectory(CUnit)
add_subdirectory(doc)
add_subdirectory(Man)
add_subdirectory(Share)
if(${ENABLE_EXAMPLES})
	add_subdirectory(Examples)
endif(${ENABLE_EXAMPLES})
