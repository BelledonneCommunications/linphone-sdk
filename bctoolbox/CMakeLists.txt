############################################################################
# CMakeLists.txt
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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


project(bctoolbox)

set(BCTOOLBOX_VERSION_MAJOR 0)
set(BCTOOLBOX_VERSION_MINOR 0)
set(BCTOOLBOX_VERSION_PATCH 2)
set(BCTOOLBOX_VERSION "${BCTOOLBOX_VERSION_MAJOR}.${BCTOOLBOX_VERSION_MINOR}.${BCTOOLBOX_VERSION_PATCH}")
set(PROJECT_VERSION_MAJOR ${BCTOOLBOX_VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${BCTOOLBOX_VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${BCTOOLBOX_VERSION_PATCH})
set(PROJECT_VERSION "${BCTOOLBOX_VERSION}")

set(BCTOOLBOX_SO_VERSION 0)
set(BCTOOLBOXTESTER_SO_VERSION 0)

option(ENABLE_SHARED "Build shared library." YES)
option(ENABLE_STATIC "Build static library." YES)
option(ENABLE_POLARSSL "Enable polarssl support" ON)
option(ENABLE_MBEDTLS "Enable mabedtls support" ON)
option(ENABLE_TESTS "Enable compilation of tests helper library" ON)

include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CMakePushCheckState)
include(GNUInstallDirs)

add_subdirectory(build)

# Note for Gnu/Unix systems:
# The library install director should ideally be set, according to GNUInstallDirs,
# to ${CMAKE_INSTALL_LIBDIR} for libraries, which are OS specific.
# HOWEVER autotools does not care about these specificities and ALWAYS sets them
# to $(exec_prefix)/lib (cf https://www.gnu.org/prep/standards/html_node/Directory-Variables.html)
# When using a custom prefix, we should follow the autotools guidelines because any project using pkg-config to find our library
# will expect it to be in $(exec_prefix)/lib, and NOT in $(exec_prefix)/${CMAKE_INSTALL_LIBDIR} since
# pkg-config does not let us introduce an -rpath settings to tell it where our library is located.
# Conversely, when NOT using a custom prefix, we can properly install our library in ${CMAKE_INSTALL_LIBDIR}
# because systems add extra locations for pkg-config to search in.
if("${CPACK_GENERATOR}" STREQUAL "RPM" OR "${CPACK_GENERATOR}" STREQUAL "DEB")
	# do nothing, we are using the "officials" install prefix
else()
	# force them to the default autotools values
	message("Setting CMAKE_INSTALL_LIBDIR to ${CMAKE_INSTALL_PREFIX}/lib for non-package build")
	set(CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()
set(CMAKE_MACOSX_RPATH ON)

if(ENABLE_MBEDTLS)
	find_package(MbedTLS)
	if(MBEDTLS_FOUND)
		message(STATUS "Using mbedTLS")
	endif()
endif()
if(ENABLE_POLARSSL AND NOT MBEDTLS_FOUND)
	find_package(PolarSSL REQUIRED)
	if(POLARSSL_FOUND)
		message(STATUS "Using polarSSL")
		if(CTR_DRBG_FREE)
			set(HAVE_CTR_DRGB_FREE 1)
		endif()
	endif()
endif()

if(HAVE_SSL_GET_DTLS_SRTP_PROTECTION_PROFILE)
	message(STATUS "DTLS SRTP available")
	set(HAVE_DTLS_SRTP 1)
else()
	message(STATUS "DTLS SRTP not available")
endif()

if(MBEDTLS_FOUND)
	get_filename_component(mbedtls_library_path "${MBEDTLS_LIBRARIES}" PATH)
	set(LIBS_PRIVATE "${LIBS_PRIVATE} -L${mbedlts_library_path}")
endif()
if(POLARSSL_FOUND)
	get_filename_component(polarssl_library_path "${POLARSSL_LIBRARIES}" PATH)
	set(LIBS_PRIVATE "${LIBS_PRIVATE} -L${polarssl_library_path} -lpolarssl")
endif()

if(ENABLE_TESTS)
	find_package(CUnit)
	if(CUNIT_FOUND)
		cmake_push_check_state(RESET)
		list(APPEND CMAKE_REQUIRED_INCLUDES ${CUNIT_INCLUDE_DIRS})
		list(APPEND CMAKE_REQUIRED_LIBRARIES ${CUNIT_LIBRARIES})
		check_symbol_exists("CU_get_suite" "CUnit/CUnit.h" HAVE_CU_GET_SUITE)
		check_symbol_exists("CU_curses_run_tests" "CUnit/CUnit.h" HAVE_CU_CURSES)
		check_symbol_exists("CU_set_trace_handler" "CUnit/Util.h" HAVE_CU_SET_TRACE_HANDLER)
		cmake_pop_check_state()

		set(TESTER_REQUIRES_PRIVATE "cunit")
	else()
		message(WARNING "Could not find the cunit library!")
	endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/bctoolbox.pc.in ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(ENABLE_TESTS AND CUNIT_FOUND)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/bctoolbox-tester.pc.in ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox-tester.pc)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox-tester.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

find_package(Threads REQUIRED)

include_directories(
	include
	src
	${CMAKE_CURRENT_BINARY_DIR}
)

if(MBEDTLS_FOUND)
	include_directories(${MBEDTLS_INCLUDE_DIRS})
endif()
if(POLARSSL_FOUND)
	include_directories(${POLARSSL_INCLUDE_DIRS})
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/config.h PROPERTIES GENERATED ON)
add_definitions("-DHAVE_CONFIG_H")

set(STRICT_OPTIONS_CPP )
set(STRICT_OPTIONS_C )
if(NOT MSVC)

	list(APPEND STRICT_OPTIONS_CPP "-Wall" "-Wuninitialized")
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=unknown-warning-option" "-Qunused-arguments" "-Wno-tautological-compare" "-Wno-builtin-requires-header" "-Wno-unused-function" "-Wno-gnu-designator" "-Wno-array-bounds")
	elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=pragmas")
	endif()
	if(APPLE)
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=unknown-warning-option" "-Qunused-arguments" "-Wno-tautological-compare" "-Wno-unused-function" "-Wno-array-bounds")
	endif()
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "-Werror" "-Wno-error=unknown-pragmas" "-Wuninitialized" "-fno-strict-aliasing")
		list(APPEND STRICT_OPTIONS_C "-Wdeclaration-after-statement" "-Wstrict-prototypes" "-Wno-error=strict-prototypes")
	endif()
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


if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	set(EXPORT_TARGETS_NAME "LinphoneBuilder")
else()
	set(EXPORT_TARGETS_NAME "BcToolbox")
endif()

add_compile_options(${STRICT_OPTIONS_CPP} ${STRICT_OPTIONS_C})
check_library_exists("rt" "clock_gettime" "" HAVE_LIBRT)

add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(tester)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/BcToolboxConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)
configure_file(cmake/BcToolboxConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/BcToolboxConfig.cmake"
	@ONLY
)

set(CONFIG_PACKAGE_LOCATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake")
install(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE BcToolboxTargets.cmake
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/BcToolboxConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/BcToolboxConfigVersion.cmake"
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)
