############################################################################
# CMakeLists.txt
# Copyright (C) 2010-2023 Belledonne Communications, Grenoble France
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

project(Belr VERSION 5.4.0 LANGUAGES C CXX)

set(BELR_SO_VERSION "1")

option(ENABLE_STRICT "Build with strict compile options." YES)
option(ENABLE_TOOLS "Turn on or off compilation of tools." YES)
option(ENABLE_UNIT_TESTS "Enable compilation of unit tests." YES)
option(ENABLE_PACKAGE_SOURCE "Create 'package_source' target for source archive making" OFF)

set(CMAKE_CXX_STANDARD 17)

if(NOT CPACK_GENERATOR AND NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

include(GNUInstallDirs)
include(CheckSymbolExists)
include(CMakePushCheckState)
include(CheckCXXCompilerFlag)

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D_SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING)
endif()

# find_package should be invoked here to check for libraries - however do NOT
# call include_directories here (see below)
find_package(BCToolbox 5.3.0 REQUIRED OPTIONAL_COMPONENTS tester)
#Mandatory to init default compilation flags
bc_init_compilation_flags(STRICT_OPTIONS_CPP STRICT_OPTIONS_C STRICT_OPTIONS_CXX ENABLE_STRICT)

set(LINK_FLAGS )

if(UNIX AND NOT APPLE)
	include(CheckIncludeFiles)
	check_include_files(libudev.h HAVE_LIBUDEV_H)
endif()

include_directories(
	include
	src
	${CMAKE_CURRENT_BINARY_DIR}
)

set(BELR_CPPFLAGS )
if(NOT BUILD_SHARED_LIBS)
	list(APPEND BELR_CPPFLAGS "-DBELR_STATIC")
endif()
if(BELR_CPPFLAGS)
	list(REMOVE_DUPLICATES BELR_CPPFLAGS)
	add_definitions(${BELR_CPPFLAGS})
endif()
add_definitions("-DBELR_EXPORTS")

set(BELR_GRAMMARS_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/belr/grammars")
set(BELR_GRAMMARS_RELATIVE_DIR "${CMAKE_INSTALL_DATADIR}/belr/grammars")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake" "${CMAKE_CURRENT_BINARY_DIR}/config.h")

add_subdirectory(include)
add_subdirectory(src)
if(ENABLE_UNIT_TESTS)
  add_subdirectory(tester)
endif()
if(ENABLE_TOOLS)
	add_subdirectory(tools)
endif()

include(CMakePackageConfigHelpers)
set(CMAKE_MODULES_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
configure_package_config_file("${PROJECT_NAME}Config.cmake.in" "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
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
