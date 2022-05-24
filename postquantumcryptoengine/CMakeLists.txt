############################################################################
# CMakeLists.txt
# Copyright (C) 2022  Belledonne Communications, Grenoble France
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

cmake_minimum_required(VERSION 3.2)

# CMP0077 policy is required by Flexisip build. Remove it once the CMake required
# version is higer or equal to 3.13.
if(NOT CMAKE_VERSION VERSION_LESS 3.13)
	cmake_policy(SET CMP0077 NEW)
endif()

project(postquantumcryptoengine VERSION 5.2.0)

set(POSTQUANTUMCRYPTOENGINE_VERSION "${PROJECT_VERSION}")
set(POSTQUANTUMCRYPTOENGINE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(POSTQUANTUMCRYPTOENGINE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(POSTQUANTUMCRYPTOENGINE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(POSTQUANTUMCRYPTOENGINE_SO_VERSION 1)

option(ENABLE_SHARED "Build shared library." ON)
option(ENABLE_STATIC "Build static library." OFF)
option(ENABLE_STRICT "Pass strict flags to the compiler" ON)
option(ENABLE_TESTS "Enable compilation of tests" ON)
option(ENABLE_PACKAGE_SOURCE "Create 'package_source' target for source archive making (CMake >= 3.11)" OFF)

# Hidden non-cache options:
# * DISABLE_BC_PACKAGE_SEARCH: skip find_package() for every BC package

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS NO)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CPACK_GENERATOR AND NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

if(NOT DISABLE_BC_PACKAGE_SEARCH)
	find_package(bctoolbox 0.0.3 REQUIRED OPTIONAL_COMPONENTS tester CONFIG)
endif()

find_package(liboqs REQUIRED CONFIG)

include_directories(
	include
	src
	${CMAKE_CURRENT_BINARY_DIR}
)

set(POSTQUANTUMCRYPTOENGINE_CPPFLAGS ${BCTOOLBOX_CPPFLAGS})
if(ENABLE_STATIC)
	set(BCTBX_STATIC 1)
endif()
if(POSTQUANTUMCRYPTOENGINE_CPPFLAGS)
	list(REMOVE_DUPLICATES POSTQUANTUMCRYPTOENGINE_CPPFLAGS)
	add_definitions(${POSTQUANTUMCRYPTOENGINE_CPPFLAGS})
endif()


bc_init_compilation_flags(STRICT_OPTIONS_CPP STRICT_OPTIONS_C STRICT_OPTIONS_CXX ENABLE_STRICT)
if(CMAKE_VERSION VERSION_LESS 3.1 AND NOT APPLE AND NOT MSVC)
	list(APPEND STRICT_OPTIONS_CXX "-std=c++11")
endif()

set(EXPORT_TARGETS_NAME "postquantumcryptoengine")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/config.h PROPERTIES GENERATED ON)
add_definitions("-DHAVE_CONFIG_H")

add_subdirectory(include)
add_subdirectory(src)
if(ENABLE_TESTS)
	add_subdirectory(tester)
endif()
if(ENABLE_PACKAGE_SOURCE)
	add_subdirectory(build)
endif()

include(CMakePackageConfigHelpers)
set(CONFIG_PACKAGE_LOCATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake")

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}ConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)
configure_package_config_file(cmake/postquantumcryptoengineConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}Config.cmake"
	INSTALL_DESTINATION ${CONFIG_PACKAGE_LOCATION}
	NO_SET_AND_CHECK_MACRO
)

install(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE "${EXPORT_TARGETS_NAME}Targets.cmake"
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)

install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}Config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}ConfigVersion.cmake"
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)
