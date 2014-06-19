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
project(BELLESIP C)
enable_language(CXX)


set(PACKAGE "belle-sip")
set(PACKAGE_NAME "${PACKAGE}")
set(PACKAGE_VERSION "1.3.1")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "jehan.monnier@linphone.org")
set(PACKAGE_TARNAME "belle-sip")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")


option(ENABLE_SERVER_SOCKETS "Enable server sockets" ON)
option(ENABLE_STATIC "Build static library (default is shared library)." OFF)
option(ENABLE_TLS "Enable TLS support" ON)
option(ENABLE_TUNNEL "Enable tunnel support" OFF)
option(ENABLE_TESTS "Enable compilation of tests" ON)
set(WITH_ANTLR "${CMAKE_INSTALL_PREFIX}" CACHE STRING "Set prefix where libantlr3c can be found (ex:/usr or /usr/local)")
set(WITH_CUNIT "${CMAKE_INSTALL_PREFIX}" CACHE STRING "Set prefix where libcunit can be found (ex:/usr or /usr/local)")
set(WITH_POLARSSL "${CMAKE_INSTALL_PREFIX}" CACHE STRING "Set prefix where polarssl can be found (ex:/usr or /usr/local)")


include(CheckIncludeFile)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

check_include_file("dlfcn.h" HAVE_DLFCN_H)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("memory.h" HAVE_MEMORY_H)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("strings.h" HAVE_STRINGS_H)
check_include_file("string.h" HAVE_STRING_H)
check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("unistd.h" HAVE_UNISTD_H)

check_library_exists("dl" "dlopen" "" HAVE_LIBDL)
check_library_exists("rt" "clock_gettime" "" HAVE_LIBRT)

check_symbol_exists("res_ndestroy" "resolv.h" HAVE_RES_NDESTROY)
set(CMAKE_REQUIRED_LIBRARIES resolv)
check_c_source_compiles("#include <resolv.h>
int main(int argc, char *argv[]) {
res_getservers(NULL,NULL,0);
return 0;
}"
	HAVE_RES_GETSERVERS)
if(NOT "${HAVE_RES_NDESTROY}" STREQUAL "" AND NOT "${HAVE_RES_GETSERVERS}" STREQUAL "")
	set(HAVE_RESINIT 1)
endif(NOT "${HAVE_RES_NDESTROY}" STREQUAL "" AND NOT "${HAVE_RES_GETSERVERS}" STREQUAL "")

find_package(Threads)

find_package(Antlr3)

if(${ENABLE_TLS})
	find_package(PolarSSL)
	if(POLARSSL_FOUND)
		set(HAVE_POLARSSL 1)
	endif(POLARSSL_FOUND)
endif(${ENABLE_TLS})
if(${ENABLE_TESTS})
	find_package(CUnit)
	if(CUNIT_FOUND)
		check_library_exists(${CUNIT_LIBRARIES} "CU_add_suite" "" HAVE_CU_ADD_SUITE)
		check_library_exists(${CUNIT_LIBRARIES} "CU_get_suite" "" HAVE_CU_GET_SUITE)
		check_library_exists(${CUNIT_LIBRARIES} "CU_curses_run_tests" "" HAVE_CU_CURSES)
	else(CUNIT_FOUND)
		message(WARNING "Could not find cunit framework, tests will not be compiled.")
		set(ENABLE_TESTS OFF CACHE BOOL "Enable compilation of tests" FORCE)
	endif(CUNIT_FOUND)
endif(${ENABLE_TESTS})
find_package(Tunnel)
if(TUNNEL_FOUND)
	set(HAVE_TUNNEL 1)
endif(TUNNEL_FOUND)


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)


set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${prefix}/bin)
set(libdir ${prefix}/lib)
set(includedir ${prefix}/include)
get_filename_component(antlr3c_library_path "${ANTLR3C_LIBRARIES}" PATH)
set(LIBS_PRIVATE "-L${antlr3c_library_path} -lantlr3c")
if(CUNIT_FOUND)
	set(REQUIRES_PRIVATE "${REQUIRES_PRIVATE} cunit")
endif(CUNIT_FOUND)
if(HAVE_POLARSSL)
	get_filename_component(polarssl_library_path "${POLARSSL_LIBRARIES}" PATH)
	set(LIBS_PRIVATE "${LIBS_PRIVATE} -L${polarssl_library_path} -lpolarssl")
endif(HAVE_POLARSSL)
if(HAVE_LIBDL)
	set(LIBS_PRIVATE "${LIBS_PRIVATE} -ldl")
endif()
if(HAVE_LIBRT)
	set(LIBS_PRIVATE "${LIBS_PRIVATE} -lrt")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/belle-sip.pc.in ${CMAKE_CURRENT_BINARY_DIR}/belle-sip.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/belle-sip.pc DESTINATION lib/pkgconfig)


include_directories(
	include
 	src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/src
)

add_definitions(-DHAVE_CONFIG_H)
if(NOT MSVC)
	add_definitions(-Wall -Wno-error=unknown-pragmas)
	if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
		add_definitions(-Werror -Wno-error=unknown-warning-option)
	elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
		add_definitions(-Werror -Wno-error=pragmas)
	endif()
endif(NOT MSVC)
if("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
	set(LINK_FLAGS "${LINK_FLAGS} -framework CoreFoundation -framework CFNetwork")
endif("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
if(WIN32)
	add_definitions(-DBELLESIP_EXPORTS)
endif(WIN32)

add_subdirectory(include)
add_subdirectory(src)

if(${ENABLE_TESTS})
	enable_testing()
	add_subdirectory(tester)
endif(${ENABLE_TESTS})
