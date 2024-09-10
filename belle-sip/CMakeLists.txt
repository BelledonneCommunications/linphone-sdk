# CMakeLists.txt
# Copyright (C) 2010-2023  Belledonne Communications, Grenoble France
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


cmake_minimum_required(VERSION 3.22)

project(BelleSIP VERSION 5.4.0 LANGUAGES C CXX)

set(PACKAGE "${PROJECT_NAME}")
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "jehan.monnier@linphone.org")
set(PACKAGE_TARNAME "${PROJECT_NAME}")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")
set(BELLESIP_SO_VERSION 1) # set for 4.4.0 release.


option(ENABLE_RTP_MAP_ALWAYS_IN_SDP "Always include rtpmap in SDP." OFF)
option(ENABLE_SIP_PARSER_BENCHMARK "Enable build of SIP parser benchmark." OFF)
option(ENABLE_STRICT "Build with strict compile options." YES)
option(ENABLE_TUNNEL "Enable tunnel support" OFF)
option(ENABLE_UNIT_TESTS "Enable compilation of tests" ON)
option(ENABLE_MDNS "Enable multicast DNS" OFF)
option(ENABLE_PACKAGE_SOURCE "Create 'package_source' target for source archive making" OFF)


include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CMakePushCheckState)
include(GNUInstallDirs)
include(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(ENABLE_DNS_SERVICE "Enable Apple DNS Service. Available on Mac and iOS. Incompatible with ENABLE_MDNS." ON "APPLE" OFF)

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

check_library_exists("dl" "dlopen" "" HAVE_LIBDL)
check_library_exists("rt" "clock_gettime" "" HAVE_LIBRT)

cmake_push_check_state(RESET)
check_symbol_exists("res_ndestroy" "resolv.h" HAVE_RES_NDESTROY)
set(CMAKE_REQUIRED_LIBRARIES resolv)
check_c_source_compiles("#include <resolv.h>
int main(int argc, char *argv[]) {
res_getservers(NULL,NULL,0);
return 0;
}"
	HAVE_RES_GETSERVERS)
if(HAVE_RES_NDESTROY AND HAVE_RES_GETSERVERS)
	set(HAVE_RESINIT 1)
endif()
cmake_pop_check_state()

find_package(Threads)

find_package(Belr 5.3.0 REQUIRED)
find_package(BCToolbox 5.3.0 REQUIRED OPTIONAL_COMPONENTS tester)
#Mandatory to init default compilation flags
bc_init_compilation_flags(STRICT_OPTIONS_CPP STRICT_OPTIONS_C STRICT_OPTIONS_CXX ENABLE_STRICT)

bc_find_package(ZLIB ZLIB::ZLIB zlib)
if(ZLIB_FOUND)
	set(HAVE_ZLIB 1)
endif()

if(ENABLE_RTP_MAP_ALWAYS_IN_SDP)
	set(BELLE_SDP_FORCE_RTP_MAP 1)
endif()

if(ENABLE_TUNNEL)
	find_package(Tunnel 0.7.0)
	if(Tunnel_FOUND)
		set(HAVE_TUNNEL 1)
	else()
		message(WARNING "Could not find the tunnel library!")
		set(ENABLE_TUNNEL OFF CACHE BOOL "Enable tunnel support" FORCE)
	endif()
endif()

if (ENABLE_DNS_SERVICE)
	if(ENABLE_MDNS)
		message(FATAL_ERROR "ENABLE_MDNS and ENABLE_DNS_SERVICE are incompatible options")
	endif()
	if (NOT APPLE)
		message(FATAL_ERROR "ENABLE_DNS_SERVICE is available on APPLE platforms only")
	endif()
	message("Using Apple DNSService")
	set(HAVE_DNS_SERVICE 1)
endif()

if(ENABLE_MDNS)
	find_package(DNSSD REQUIRED)
	if(DNSSD_FOUND)
		set(HAVE_MDNS 1)
	endif()
endif()


configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake" "${CMAKE_CURRENT_BINARY_DIR}/config.h")


set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${prefix}/${CMAKE_INSTALL_BINDIR})
set(libdir ${prefix}/${CMAKE_INSTALL_LIBDIR})
set(includedir ${prefix}/include)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/belle-sip.pc.in ${CMAKE_CURRENT_BINARY_DIR}/belle-sip.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/belle-sip.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

include_directories(
	include
	src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/src
)
if(DNSSD_FOUND)
	include_directories(${DNSSD_INCLUDE_DIRS})
endif()

add_definitions("-DHAVE_CONFIG_H")
set(STRICT_OPTIONS_OBJC "-fmodules")
if(MSVC)
	list(APPEND STRICT_OPTIONS_CPP "/wd4996") # Disable POSIX functions warnings from bctbx.
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "/WX")
	endif()
else()
	list(APPEND STRICT_OPTIONS_CPP "-Wall" "-Wuninitialized" "-Wno-error=deprecated-declarations")
	if(CMAKE_C_COMPILER_ID MATCHES "Clang")
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=unknown-warning-option" "-Qunused-arguments" "-Wno-tautological-compare" "-Wno-builtin-requires-header" "-Wno-unused-function" "-Wno-gnu-designator" "-Wno-array-bounds")
	elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
		list(APPEND STRICT_OPTIONS_C "-Werror=vla") # Forbids usage of variable length arrays since Visual Studio's compiler does not support them.
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=pragmas")

		if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8)
			list(APPEND STRICT_OPTIONS_C "-Wno-error=cast-function-type" )
		endif()
	endif()
	if(APPLE)
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=unknown-warning-option" "-Qunused-arguments" "-Wno-tautological-compare" "-Wno-unused-function" "-Wno-array-bounds")
	endif()
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "-Werror" "-Wextra" "-Wno-unused-parameter" "-Wno-error=unknown-pragmas" "-Wuninitialized" "-fno-strict-aliasing")
		list(APPEND STRICT_OPTIONS_C " -Wno-missing-field-initializers" "-Wno-error=unused-result" "-Wno-cast-function-type")
	endif()
endif()
if(STRICT_OPTIONS_CPP)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_CPP)
endif()
if(STRICT_OPTIONS_C)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_C)
endif()
set(APPLE_FRAMEWORKS)
if(APPLE)
	list(APPEND APPLE_FRAMEWORKS "-framework Foundation")
	if(IOS)
		list(APPEND APPLE_FRAMEWORKS "-framework CoreFoundation" "-framework CFNetwork" "-framework UIKit")
	endif()
endif()

set(BELLESIP_CPPFLAGS )
if(NOT BUILD_SHARED_LIBS)
	list(APPEND BELLESIP_CPPFLAGS "-DBELLESIP_STATIC")
endif()
if(BELLESIP_CPPFLAGS)
	list(REMOVE_DUPLICATES BELLESIP_CPPFLAGS)
	add_definitions(${BELLESIP_CPPFLAGS})
endif()
add_definitions("-DBELLESIP_EXPORTS")

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone")
	add_definitions(
		-DHAVE_COMPILER_TLS
		-DUSE_FIXED_NAMESERVERS
		-DUSE_GETADDRINFO_FALLBACK
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	add_definitions(
		-DHAVE_COMPILER_TLS
	)
endif()


add_subdirectory(include)
add_subdirectory(src)


if(ENABLE_UNIT_TESTS)
	enable_testing()
	add_subdirectory(tester)
endif()


include(CMakePackageConfigHelpers)
set(CMAKE_MODULES_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
configure_package_config_file("cmake/${PROJECT_NAME}Config.cmake.in" "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	INSTALL_DESTINATION "${CMAKE_MODULES_INSTALL_DIR}"
	NO_SET_AND_CHECK_MACRO
)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)
install(FILES
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
)

install(EXPORT ${PROJECT_NAME}Targets
	FILE "${PROJECT_NAME}Targets.cmake"
	DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
)

if (ENABLE_PACKAGE_SOURCE)
	add_subdirectory(build)
endif()
