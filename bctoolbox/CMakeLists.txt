############################################################################
# CMakeLists.txt
# Copyright (C) 2010-2021  Belledonne Communications, Grenoble France
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

project(BCToolbox VERSION 5.4.0)

set(BCTOOLBOX_VERSION "${PROJECT_VERSION}")
set(BCTOOLBOX_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(BCTOOLBOX_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(BCTOOLBOX_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(BCTOOLBOX_SO_VERSION 1)
set(BCTOOLBOXTESTER_SO_VERSION 1)


option(ENABLE_DEBUG_LOGS "Turn on or off debug level logs." NO)
option(ENABLE_MBEDTLS "Enable mbedtls support" ON)
option(ENABLE_OPENSSL "Enable openssl support" OFF)
option(ENABLE_DECAF "Enable Elliptic Curve Cryptography support" ON)
option(ENABLE_STRICT "Pass strict flags to the compiler" ON)
option(ENABLE_TESTS_COMPONENT "Enable compilation of tests helper library" ON)
option(ENABLE_UNIT_TESTS "Enable compilation of tests" ON)
option(ENABLE_PACKAGE_SOURCE "Create 'package_source' target for source archive making" OFF)
option(ENABLE_DEFAULT_LOG_HANDLER "A default log handler will be initialized, if OFF no logging will be done before you initialize one." ON)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS NO)

include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CMakePushCheckState)
include(GNUInstallDirs)
include("cmake/BCToolboxCMakeUtils.cmake")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if(ENABLE_MBEDTLS AND ENABLE_OPENSSL)
	message(FATAL_ERROR "Cannot build bctoolbox with both openssl and mbedtls enabled")
endif()

if(NOT CPACK_GENERATOR AND NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

if(APPLE)
	find_package(Iconv REQUIRED)
endif()

if(ENABLE_MBEDTLS)
	message(STATUS "MBEDTLS Enabled")
	find_package(MbedTLS)
	if(MbedTLS_FOUND)
		message(STATUS "Found mbedTLS v${MbedTLS_VERSION}")
		if(MbedTLS_VERSION LESS 3)
			message(FATAL_ERROR "Mbedtls version incompatible (requires v3)")
		endif()
		set(HAVE_MBEDTLS 1)
	endif()
endif()

if(ENABLE_OPENSSL)
	message(STATUS "OPENSSL Enabled")
	find_package(OpenSSL)
	if(OPENSSL_FOUND)
		message(STATUS "Using OPENSSL v. ${OPENSSL_VERSION}")
		set(HAVE_OPENSSL 1)
		set(DTLS_SRTP_AVAILABLE TRUE)
	endif()
endif()

if(ENABLE_DECAF)
	find_package(Decaf)
	if(Decaf_FOUND)
		message(STATUS "Using Decaf")
		set(HAVE_DECAF 1)
	endif()
endif()

if(DTLS_SRTP_AVAILABLE)
	message(STATUS "DTLS SRTP available")
	set(HAVE_DTLS_SRTP 1)
else()
	message(STATUS "DTLS SRTP not available")
endif()

if(ENABLE_TESTS_COMPONENT)
	find_package(BCUnit 3.0.2 REQUIRED)
	set(TESTER_REQUIRES_PRIVATE "${BCUnit_TARGET}")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/bctoolbox.pc.in ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(ENABLE_TESTS_COMPONENT)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/bctoolbox-tester.pc.in ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox-tester.pc)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox-tester.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

find_package(Threads REQUIRED)

include_directories(
	include
	src
	${CMAKE_CURRENT_BINARY_DIR}
)

set(BCTOOLBOX_CPPFLAGS )
if(NOT BUILD_SHARED_LIBS)
	set(BCTBX_STATIC 1)
	list(APPEND BCTOOLBOX_CPPFLAGS "-DBCTBX_STATIC")
endif()
if(ENABLE_DEBUG_LOGS)
	add_definitions("-DBCTBX_DEBUG_MODE")
endif()

bc_init_compilation_flags(STRICT_OPTIONS_CPP STRICT_OPTIONS_C STRICT_OPTIONS_CXX ENABLE_STRICT)

check_library_exists("rt" "clock_gettime" "" HAVE_LIBRT)
check_library_exists("dl" "dladdr" "" HAVE_LIBDL)

if(ANDROID)
	set(HAVE_EXECINFO 0)
else()
	check_include_file("execinfo.h" HAVE_EXECINFO)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake" "${CMAKE_CURRENT_BINARY_DIR}/config.h")
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/config.h" PROPERTIES GENERATED ON)
add_definitions("-DHAVE_CONFIG_H")

add_subdirectory(include)
add_subdirectory(src)
if(ENABLE_UNIT_TESTS AND ENABLE_TESTS_COMPONENT)
	add_subdirectory(tester)
endif()
if(ENABLE_PACKAGE_SOURCE)
	add_subdirectory(build)
endif()

include(CMakePackageConfigHelpers)
set(CMAKE_MODULES_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake")
configure_package_config_file("cmake/${PROJECT_NAME}Config.cmake.in" "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	INSTALL_DESTINATION "${CMAKE_MODULES_INSTALL_DIR}"
	PATH_VARS CMAKE_MODULES_INSTALL_DIR
)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)

configure_file("cmake/BCToolboxCMakeUtils.cmake" "${CMAKE_CURRENT_BINARY_DIR}/BCToolboxCMakeUtils.cmake" COPYONLY)
configure_file("cmake/BCGitVersion.cmake" "${CMAKE_CURRENT_BINARY_DIR}/BCGitVersion.cmake" COPYONLY)
configure_file("cmake/gitversion.h.in" "${CMAKE_CURRENT_BINARY_DIR}/gitversion.h.in" COPYONLY)

install(EXPORT ${PROJECT_NAME}Targets
	FILE "${PROJECT_NAME}Targets.cmake"
	DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
)

install(FILES
	"cmake/MakeArchive.cmake"
	"cmake/ConfigureSpecfile.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/BCToolboxCMakeUtils.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/BCGitVersion.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/gitversion.h.in"
	DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
)

if(MbedTLS_FOUND)
	install(FILES "cmake/FindMbedTLS.cmake" DESTINATION ${CMAKE_MODULES_INSTALL_DIR})
endif()
if(Decaf_FOUND)
	install(FILES "cmake/FindDecaf.cmake" DESTINATION ${CMAKE_MODULES_INSTALL_DIR})
endif()
