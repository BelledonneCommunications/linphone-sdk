############################################################################
# CMakeLists.txt
# Copyright (C) 2010-2019  Belledonne Communications, Grenoble France
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

cmake_minimum_required(VERSION 3.1)

# CMP0077 policy is required by Flexisip build. Remove it once the CMake required
# version is higer or equal to 3.13.
if(NOT CMAKE_VERSION VERSION_LESS 3.13)
    cmake_policy(SET CMP0077 NEW)
endif()

project(bzrtp VERSION 5.2.0 LANGUAGES C CXX)

option(ENABLE_SHARED "Build shared library." YES)
option(ENABLE_STATIC "Build static library." YES)
option(ENABLE_ZIDCACHE "Turn on compilation of ZID cache, request sqlite" YES)
option(ENABLE_STRICT "Build with strict compile options." YES)
option(ENABLE_TESTS "Enable compilation of unit tests." NO)
option(ENABLE_DOC "Enable API documentation generation." NO)
option(ENABLE_EXPORTEDKEY_V1_0_RETROCOMPATIBILITY "Enable support for Limev1 with older version of bzrtp(before v1.06)" NO)
option(ENABLE_PACKAGE_SOURCE "Create 'package_source' target for source archive making (CMake >= 3.11)" OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS NO)

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(MSVC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/MSVC")
if(MSVC)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${MSVC_INCLUDE_DIR})
endif()

# Hidden non-cache options:
# * DISABLE_BC_PACKAGE_SEARCH: skip find_package() for every BC package (bctoolbox, ortp, etc.)

include(GNUInstallDirs)
include(CheckLibraryExists)
check_library_exists("m" "sqrt" "" HAVE_SQRT)

if(NOT DISABLE_BC_PACKAGE_SEARCH)
	find_package(bctoolbox 0.0.3 REQUIRED OPTIONAL_COMPONENTS tester CONFIG)
	find_package(postquantumcryptoengine CONFIG)
endif()

if(ENABLE_ZIDCACHE)
	find_package(Sqlite3 REQUIRED)
	# Also check if we have libxml2, as we need it for migration purpose
	find_package(XML2)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/config.h PROPERTIES GENERATED ON)
add_definitions("-DHAVE_CONFIG_H")

bc_init_compilation_flags(STRICT_OPTIONS_CPP STRICT_OPTIONS_C STRICT_OPTIONS_CXX ENABLE_STRICT)
if(CMAKE_VERSION VERSION_LESS 3.1 AND NOT APPLE AND NOT MSVC)
	list(APPEND STRICT_OPTIONS_CXX "-std=c++11")
endif()

set(BZRTP_CPPFLAGS ${BCTOOLBOX_CPPFLAGS})
if(ENABLE_STATIC)
	list(APPEND BZRTP_CPPFLAGS "-DBZRTP_STATIC")
endif()
if(BZRTP_CPPFLAGS)
	list(REMOVE_DUPLICATES BZRTP_CPPFLAGS)
	add_definitions(${BZRTP_CPPFLAGS})
endif()

include_directories(
	include
	${CMAKE_CURRENT_BINARY_DIR}
)

if(MSVC)
	include_directories(${MSVC_INCLUDE_DIR})
endif()

if(ENABLE_ZIDCACHE)
	add_definitions("-DZIDCACHE_ENABLED")
	if(XML2_FOUND)
		add_definitions("-DHAVE_LIBXML2")
	endif()
endif()

if (ENABLE_EXPORTEDKEY_V1_0_RETROCOMPATIBILITY)
	add_definitions("-DSUPPORT_EXPORTEDKEY_V010000")
endif()

if (postquantumcryptoengine_FOUND)
	add_definitions("-DHAVE_BCTBXPQ")
endif()

set(EXPORT_TARGETS_NAME "bzrtp")

add_subdirectory(include)
add_subdirectory(src)
if(ENABLE_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()


include(CMakePackageConfigHelpers)
set(CONFIG_PACKAGE_LOCATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake")

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}ConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)

if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	export(EXPORT ${EXPORT_TARGETS_NAME}Targets
		FILE "${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}Targets.cmake"
	)
endif()

configure_package_config_file(cmake/BZRTPConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}Config.cmake"
	INSTALL_DESTINATION ${CONFIG_PACKAGE_LOCATION}
	NO_SET_AND_CHECK_MACRO
)

install(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE ${EXPORT_TARGETS_NAME}Targets.cmake
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}Config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}ConfigVersion.cmake"
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)

if(ENABLE_DOC)
	# Doxygen
	find_package(Doxygen)
	if (DOXYGEN_FOUND)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
		add_custom_target(doc ALL
			${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			COMMENT "Generating API documentation with Doxygen" VERBATIM)
		install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc/html/"
			DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/bzrtp-${PROJECT_VERSION}")
	endif()
endif()

if (ENABLE_PACKAGE_SOURCE)
	add_subdirectory(build)
endif()
