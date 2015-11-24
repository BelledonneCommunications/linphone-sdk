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

cmake_minimum_required(VERSION 2.8.12)
project(CUNIT C)


set(PACKAGE "CUnit")
set(PACKAGE_NAME "${PACKAGE}")
set(VERSION "2.1")
set(RELEASE "2")
set(PACKAGE_VERSION "${VERSION}-${RELEASE}")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "")
set(PACKAGE_TARNAME "cunit")
set(PACKAGE_URL "")


option(ENABLE_AUTOMATED "Compile CUnit automated interface" ON)
option(ENABLE_BASIC "Compile CUnit basic interface" ON)
option(ENABLE_CONSOLE "Compile CUnit console interface" ON)
option(ENABLE_CURSES "Compile CUnit curses interface" OFF)
option(ENABLE_EXAMPLES "Compile CUnit example programs" OFF)
option(ENABLE_TEST "Compile CUnit internal test program" OFF)
option(ENABLE_MEMTRACE "Enable CUnit internal memory tracking" OFF)
option(ENABLE_DEPRECATED "Enable use of deprecated v1.1 names" OFF)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)


set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${prefix}/bin)
set(libdir ${prefix}/lib)
set(includedir ${prefix}/include)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cunit.pc.in ${CMAKE_CURRENT_BINARY_DIR}/cunit.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cunit.pc DESTINATION lib/pkgconfig)


if(MSVC)
	add_definitions("/W3")
else()
	add_definitions("-Wall -W -pedantic -Wshadow -ansi -std=c99")
endif()

if(ENABLE_MEMTRACE)
	add_definitions(-DMEMTRACE)
endif()
if(ENABLE_DEPRECATED)
	add_definitions(-DUSE_DEPRECATED_CUNIT_NAMES)
endif()
if(ENABLE_CURSES)
	set(CURSES_NEED_NCURSES 1)
	find_package(Curses)
	if(NOT CURSES_FOUND)
		message("Disabling curses as it has not been found!")
		set(ENABLE_CURSES 0)
	endif()
endif()

include_directories(
	CUnit/Headers
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/CUnit/Headers
)


add_subdirectory(CUnit)
add_subdirectory(doc)
add_subdirectory(Man)
add_subdirectory(Share)
if(ENABLE_EXAMPLES)
	add_subdirectory(Examples)
endif()
