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

cmake_minimum_required(VERSION 3.0)
project(belle-sip VERSION 1.5.0 LANGUAGES C CXX)


set(PACKAGE "${PROJECT_NAME}")
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "jehan.monnier@linphone.org")
set(PACKAGE_TARNAME "${PROJECT_NAME}")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")


option(ENABLE_SHARED "Build shared library." YES)
option(ENABLE_STATIC "Build static library." YES)
option(ENABLE_RTP_MAP_ALWAYS_IN_SDP "Always include rtpmap in SDP." OFF)
option(ENABLE_STRICT "Build with strict compile options." YES)
option(ENABLE_TUNNEL "Enable tunnel support" OFF)
option(ENABLE_TESTS "Enable compilation of tests" ON)


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


include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CMakePushCheckState)
include(GNUInstallDirs)

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

set(MSVC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/MSVC")
if(MSVC)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${MSVC_INCLUDE_DIR})
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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

find_package(Zlib)
find_package(Antlr3 REQUIRED)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES ${ANTLR3C_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${ANTLR3C_LIBRARIES})
check_symbol_exists("antlr3StringStreamNew" "antlr3.h" HAVE_ANTLR_STRING_STREAM_NEW)
cmake_pop_check_state()

if(ENABLE_RTP_MAP_ALWAYS_IN_SDP)
	set(BELLE_SDP_FORCE_RTP_MAP 1)
endif()

if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	set(BcToolbox_FIND_COMPONENTS tester)
	include("${EP_bctoolbox_CONFIG_DIR}/BcToolboxConfig.cmake")
else()
	find_package(BcToolbox 0.5.0 REQUIRED OPTIONAL_COMPONENTS tester)
endif()

if(ENABLE_TUNNEL)
	if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
		include("${EP_tunnel_CONFIG_DIR}/TunnelConfig.cmake")
	else()
		find_package(Tunnel)
	endif()
	if(TUNNEL_FOUND)
		set(HAVE_TUNNEL 1)
	else()
		message(WARNING "Could not find the tunnel library!")
		set(ENABLE_TUNNEL OFF CACHE BOOL "Enable tunnel support" FORCE)
	endif()
endif()
if(ZLIB_FOUND)
	set(HAVE_ZLIB 1)
endif()


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)


set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${prefix}/bin)
set(libdir ${prefix}/lib)
set(includedir ${prefix}/include)
get_filename_component(antlr3c_library_path "${ANTLR3C_LIBRARIES}" PATH)
set(LIBS_PRIVATE "-L${antlr3c_library_path} -lantlr3c")
get_filename_component(bctoolbox_library_path "${BCTOOLBOX_CORE_LIBRARIES}" PATH)
set(LIBS_PRIVATE "${LIBS_PRIVATE} -L${bctoolbox_library_path} -lbctoolbox")
if(ZLIB_FOUND)
	set(REQUIRES_PRIVATE "${REQUIRES_PRIVATE} z")
endif()
if(HAVE_LIBDL)
	set(LIBS_PRIVATE "${LIBS_PRIVATE} -ldl")
endif()
if(HAVE_LIBRT)
	set(LIBS_PRIVATE "${LIBS_PRIVATE} -lrt")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/belle-sip.pc.in ${CMAKE_CURRENT_BINARY_DIR}/belle-sip.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/belle-sip.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")


include_directories(
	include
	src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/src
	${BCTOOLBOX_CORE_INCLUDE_DIRS}
)
if(TUNNEL_FOUND)
	include_directories(${TUNNEL_INCLUDE_DIRS})
endif()
if(ZLIB_FOUND)
	include_directories(${ZLIB_INCLUDE_DIRS})
endif()
if(MSVC)
	include_directories(${MSVC_INCLUDE_DIR})
endif()

add_definitions("-DHAVE_CONFIG_H")
set(STRICT_OPTIONS_CPP )
set(STRICT_OPTIONS_C )
set(STRICT_OPTIONS_OBJC "-fmodules")
if(MSVC)
	list(APPEND STRICT_OPTIONS_CPP "/wd4068") # Disable "unknown pragma" warnings from antlr
	list(APPEND STRICT_OPTIONS_CPP "/wd4129") # Disable "unrecognized character escape sequence" warnings from antlr
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "/WX")
	endif()
else()
	list(APPEND STRICT_OPTIONS_CPP "-Wall" "-Wuninitialized" "-Wno-error=deprecated-declarations")
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=unknown-warning-option" "-Qunused-arguments" "-Wno-tautological-compare" "-Wno-builtin-requires-header" "-Wno-unused-function" "-Wno-gnu-designator" "-Wno-array-bounds")
	elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=pragmas")
	endif()
	if(APPLE)
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=unknown-warning-option" "-Qunused-arguments" "-Wno-tautological-compare" "-Wno-unused-function" "-Wno-array-bounds")
	endif()
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "-Werror" "-Wextra" "-Wno-unused-parameter" "-Wno-error=unknown-pragmas" "-Wuninitialized" "-fno-strict-aliasing")
		list(APPEND STRICT_OPTIONS_C "-Wdeclaration-after-statement" "-Wstrict-prototypes -Wno-missing-field-initializers")
	endif()
	# this warning is generated by antlr so ignore it for now
	list(APPEND STRICT_OPTIONS_C "-Wno-strict-prototypes" "-Wno-error=sign-compare")
endif()
if(STRICT_OPTIONS_CPP)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_CPP)
endif()
if(STRICT_OPTIONS_C)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_C)
endif()
set(LINK_FLAGS )
if(APPLE)
	list(APPEND LINK_FLAGS "-framework Foundation")
	if(IOS)
		list(APPEND LINK_FLAGS "-framework CoreFoundation" "-framework CFNetwork" "-framework UIKit")
	endif()
endif()
string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
if(WIN32)
	add_definitions(
		-DBELLESIP_EXPORTS
		-DBELLESIP_INTERNAL_EXPORTS
	)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" OR CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	add_definitions(
		-DHAVE_COMPILER_TLS
		-DUSE_FIXED_NAMESERVERS
		-DUSE_GETADDRINFO_FALLBACK
	)
endif()


if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	set(EXPORT_TARGETS_NAME "LinphoneBuilder")
else()
	set(EXPORT_TARGETS_NAME "BelleSIP")
endif()


add_subdirectory(include)
add_subdirectory(src)


if(ENABLE_TESTS AND BCTOOLBOX_TESTER_FOUND)
	enable_testing()
	add_subdirectory(tester)
endif()


include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/BelleSIPConfigVersion.cmake"
	VERSION ${PACKAGE_VERSION}
	COMPATIBILITY AnyNewerVersion
)
export(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/BelleSIPTargets.cmake"
)
configure_file(cmake/BelleSIPConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/BelleSIPConfig.cmake"
	@ONLY
)

set(CONFIG_PACKAGE_LOCATION "${CMAKE_INSTALL_DATADIR}/BelleSIP/cmake")
install(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE BelleSIPTargets.cmake
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/BelleSIPConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/BelleSIPConfigVersion.cmake"
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)
