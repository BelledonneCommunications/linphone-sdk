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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

cmake_minimum_required(VERSION 2.8.12)
set(CMAKE_MACOSX_RPATH ON) # Before cmake 3.0.0, MACOSX_RPATH was not set to ON by default - however this is no good reason to not enable it by default


project(bctoolbox)

set(BCTOOLBOX_VERSION_MAJOR 0)
set(BCTOOLBOX_VERSION_MINOR 5)
set(BCTOOLBOX_VERSION_PATCH 0)
set(BCTOOLBOX_SO_VERSION 0)
set(BCTOOLBOXTESTER_SO_VERSION 0)

set(BCTOOLBOX_VERSION "${BCTOOLBOX_VERSION_MAJOR}.${BCTOOLBOX_VERSION_MINOR}.${BCTOOLBOX_VERSION_PATCH}")
set(PROJECT_VERSION_MAJOR ${BCTOOLBOX_VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${BCTOOLBOX_VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${BCTOOLBOX_VERSION_PATCH})
set(PROJECT_VERSION "${BCTOOLBOX_VERSION}")


option(ENABLE_SHARED "Build shared library." ON)
option(ENABLE_STATIC "Build static library." ON)
option(ENABLE_POLARSSL "Enable polarssl support" ON)
option(ENABLE_MBEDTLS "Enable mabedtls support" ON)
option(ENABLE_STRICT "Pass strict flags to the compiler" ON)
option(ENABLE_TESTS_COMPONENT "Enable compilation of tests helper library" ON)
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

add_subdirectory(build)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CPACK_GENERATOR AND NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

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

if(ENABLE_TESTS_COMPONENT)
	find_package(BCUnit 3.0 REQUIRED)
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${BCUNIT_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${BCUNIT_LIBRARIES})
	check_symbol_exists("CU_get_suite" "BCUnit/BCUnit.h" HAVE_CU_GET_SUITE)
	check_symbol_exists("CU_curses_run_tests" "BCUnit/BCUnit.h" HAVE_CU_CURSES)
	check_symbol_exists("CU_set_trace_handler" "BCUnit/Util.h" HAVE_CU_SET_TRACE_HANDLER)
	cmake_pop_check_state()
	set(TESTER_REQUIRES_PRIVATE "bcunit")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/bctoolbox.pc.in ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(ENABLE_TESTS_COMPONENT AND BCUNIT_FOUND)
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
set(STRICT_OPTIONS_CXX )
if(MSVC)
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "/WX")
	endif()
else()
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
		list(APPEND STRICT_OPTIONS_CPP "-Werror" "-Wextra" "-Wno-unused-parameter" "-Wno-error=unknown-pragmas" "-Wuninitialized" "-Wno-missing-field-initializers" "-fno-strict-aliasing")
		list(APPEND STRICT_OPTIONS_C "-Werror" "-Wdeclaration-after-statement" "-Wstrict-prototypes")
	endif()
endif()
if(STRICT_OPTIONS_CPP)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_CPP)
	string(REPLACE ";" " " STRICT_OPTIONS_CPP "${STRICT_OPTIONS_CPP}")
endif()
if(STRICT_OPTIONS_C)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_C)
	string(REPLACE ";" " " STRICT_OPTIONS_C "${STRICT_OPTIONS_C}")
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

check_library_exists("rt" "clock_gettime" "" HAVE_LIBRT)
check_library_exists("dl" "dladdr" "" HAVE_LIBDL)
check_include_file("execinfo.h" HAVE_EXECINFO)

add_subdirectory(include)
add_subdirectory(src)
if(ENABLE_TESTS AND ENABLE_TESTS_COMPONENT AND BCUNIT_FOUND)
	add_subdirectory(tester)
endif()

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
