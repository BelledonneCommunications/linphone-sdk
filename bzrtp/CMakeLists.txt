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

project(bzrtp VERSION 1.0.6 LANGUAGES C CXX)


option(ENABLE_SHARED "Build shared library." YES)
option(ENABLE_STATIC "Build static library." YES)
option(ENABLE_ZIDCACHE "Turn on compilation of ZID cache, request sqlite" YES)
option(ENABLE_STRICT "Build with strict compile options." YES)
option(ENABLE_TESTS "Enable compilation of unit tests." NO)


if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(MSVC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/MSVC")
if(MSVC)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${MSVC_INCLUDE_DIR})
endif()

include(GNUInstallDirs)
include(CheckLibraryExists)
check_library_exists("m" "sqrt" "" HAVE_SQRT)

if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	set(BcToolbox_FIND_COMPONENTS tester)
	include("${EP_bctoolbox_CONFIG_DIR}/BcToolboxConfig.cmake")
else()
	find_package(BcToolbox 0.0.3 REQUIRED OPTIONAL_COMPONENTS tester)
endif()

if(ENABLE_ZIDCACHE)
	find_package(Sqlite3 REQUIRED)
	# Also check if we have libxml2, as we need it for migration purpose
	find_package(XML2)
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
		list(APPEND STRICT_OPTIONS_CPP "-Werror" "-Wextra" "-Wno-unused-parameter" "-Wno-missing-field-initializers")
	endif()
endif()
if(STRICT_OPTIONS_CPP)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_CPP)
	string(REPLACE ";" " " STRICT_OPTIONS_CPP "${STRICT_OPTIONS_CPP}")
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


if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	set(EXPORT_TARGETS_NAME "LinphoneBuilder")
else()
	set(EXPORT_TARGETS_NAME "BZRTP")
endif()


add_subdirectory(include)
add_subdirectory(src)
if(ENABLE_TESTS AND BCTOOLBOX_TESTER_FOUND)
	enable_testing()
	add_subdirectory(test)
endif()


include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/BZRTPConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)
export(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/BZRTPTargets.cmake"
)
configure_file(cmake/BZRTPConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/BZRTPConfig.cmake"
	@ONLY
)

set(CONFIG_PACKAGE_LOCATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake")
install(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE BZRTPTargets.cmake
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/BZRTPConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/BZRTPConfigVersion.cmake"
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)


# CPack settings
set(CPACK_PACKAGE_NAME "bzrtp")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
set(CPACK_SOURCE_IGNORE_FILES
        "^${CMAKE_BINARY_DIR}"
        "/\\\\..+"
)

include(CPack)
