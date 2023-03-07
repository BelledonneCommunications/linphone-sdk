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

project(BELR VERSION 5.3.0 LANGUAGES C CXX)

set(BELR_SO_VERSION "1")

option(ENABLE_STRICT "Build with strict compile options." YES)
option(ENABLE_TOOLS "Turn on or off compilation of tools." YES)
option(ENABLE_UNIT_TESTS "Enable compilation of unit tests." YES)
option(ENABLE_PACKAGE_SOURCE "Create 'package_source' target for source archive making (CMake >= 3.11)" OFF)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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
find_package(BcToolbox)
if(NOT BCTOOLBOX_FOUND)
	find_package(bctoolbox 0.0.5 REQUIRED OPTIONAL_COMPONENTS tester)
endif()

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
if(MSVC)
	include_directories(${MSVC_INCLUDE_DIR})
endif()

set(STRICT_OPTIONS_CPP )
set(STRICT_OPTIONS_C )
set(STRICT_OPTIONS_CXX )
set(STRICT_OPTIONS_OBJC )
if(MSVC)
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "/WX")
	endif()
else()
	list(APPEND STRICT_OPTIONS_CPP "-Wall" "-Wuninitialized" "-Wno-error=deprecated-declarations")
	list(APPEND STRICT_OPTIONS_C "-Wdeclaration-after-statement" "-Wstrict-prototypes" "-Wno-error=strict-prototypes")
	if(CMAKE_C_COMPILER_ID MATCHES "Clang")
		list(APPEND STRICT_OPTIONS_CPP "-Qunused-arguments" "-Wno-array-bounds")
	endif()
	if(APPLE)
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=unknown-warning-option" "-Qunused-arguments" "-Wno-tautological-compare" "-Wno-unused-function" "-Wno-array-bounds")
	endif()
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "-Werror" "-Wextra" "-Wunused-parameter" "-fno-strict-aliasing")
		CHECK_CXX_COMPILER_FLAG("-Wsuggest-override" SUGGEST_OVERRIDE)
		if(SUGGEST_OVERRIDE)
			list(APPEND STRICT_OPTIONS_CPP "-Wsuggest-override -Werror=suggest-override")
		endif()
	endif()
endif()
if(STRICT_OPTIONS_CPP)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_CPP)
endif()
if(STRICT_OPTIONS_C)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_C)
endif()

set(BELR_CPPFLAGS ${BCTOOLBOX_CPPFLAGS})
if(NOT BUILD_SHARED_LIBS)
	list(APPEND BELR_CPPFLAGS "-DBELR_STATIC")
endif()
if(BELR_CPPFLAGS)
	list(REMOVE_DUPLICATES BELR_CPPFLAGS)
	add_definitions(${BELR_CPPFLAGS})
endif()
add_definitions("-DBELR_EXPORTS")

set(EXPORT_TARGET_NAME "belr")

set(BELR_GRAMMARS_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/${EXPORT_TARGET_NAME}/grammars")
set(BELR_GRAMMARS_RELATIVE_DIR "${CMAKE_INSTALL_DATADIR}/${EXPORT_TARGET_NAME}/grammars")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_subdirectory(include)
add_subdirectory(src)
if(ENABLE_UNIT_TESTS)
  add_subdirectory(tester)
endif()
if(ENABLE_TOOLS)
	add_subdirectory(tools)
endif()

include(CMakePackageConfigHelpers)
set(EXPORT_TARGET_NAME belr)
set(ConfigPackageLocation "${CMAKE_INSTALL_LIBDIR}/cmake/${EXPORT_TARGET_NAME}")

configure_package_config_file(cmake/BelrConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGET_NAME}Config.cmake"
	INSTALL_DESTINATION ${ConfigPackageLocation}
	NO_SET_AND_CHECK_MACRO
)

install(EXPORT ${EXPORT_TARGET_NAME}Targets
	FILE ${EXPORT_TARGET_NAME}Targets.cmake
	DESTINATION ${ConfigPackageLocation}
)

#Yes, an install directive is necessary. The INSTALL_DESTINATION parameter of configure_package_config_file() above does NOTHING.
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGET_NAME}Config.cmake"
	DESTINATION ${ConfigPackageLocation}
)

if (ENABLE_PACKAGE_SOURCE)
	add_subdirectory(build)
endif()
