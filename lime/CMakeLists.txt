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
include(GNUInstallDirs)
include(CheckSymbolExists)
include(CheckLibraryExists)
include(CMakePushCheckState)
include(CMakePackageConfigHelpers)

cmake_minimum_required(VERSION 3.1)

option(ENABLE_SHARED "Build shared library." YES)
option(ENABLE_STATIC "Build static library." YES)
option(ENABLE_STRICT "Build with strict compile options." YES)
option(ENABLE_CURVE25519 "Enable support of Curve 25519." YES)
option(ENABLE_CURVE448 "Enable support of Curve 448(goldilock)." YES)
option(ENABLE_UNIT_TESTS "Enable compilation of unit tests." YES)
option(ENABLE_PROFILING "Enable profiling, GCC only" NO)
option(ENABLE_C_INTERFACE "Enable support of C89 foreign function interface" NO)
option(ENABLE_JNI "Enable support of Java foreign function interface" NO)
option(ENABLE_PACKAGE_SOURCE "Create 'package_source' target for source archive making (CMake >= 3.11)" OFF)

set (LANGUAGES_LIST CXX)
if (ENABLE_C_INTERFACE)
	set (LANGUAGES_LIST ${LANGUAGES_LIST} C)
endif()
if (ENABLE_JNI)
	set (LANGUAGES_LIST ${LANGUAGES_LIST} Java)
endif()

project(lime VERSION 4.4.0 LANGUAGES ${LANGUAGES_LIST})

set(LIME_SO_VERSION "0")
set(LIME_VERSION ${PROJECT_VERSION})


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CPACK_GENERATOR AND NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

find_package(bctoolbox 0.5.1 REQUIRED OPTIONAL_COMPONENTS tester)

find_package(Soci REQUIRED)

include_directories(
	include/
	src/
	${CMAKE_CURRENT_BINARY_DIR}
)
if(MSVC)
	include_directories(${MSVC_INCLUDE_DIR})
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/config.h PROPERTIES GENERATED ON)

set(LIME_CPPFLAGS ${BCTOOLBOX_CPPFLAGS})
if(LIME_CPPFLAGS)
	list(REMOVE_DUPLICATES LIME_CPPFLAGS)
	add_definitions(${LIME_CPPFLAGS})
endif()
add_definitions("-DLIME_EXPORTS")

set(STRICT_OPTIONS_C)
set(STRICT_OPTIONS_CPP )
set(STRICT_OPTIONS_CXX )
set(STRICT_OPTIONS_OBJC )

if(ENABLE_JNI)
	set(CMAKE_CXX_STANDARD 14)
else()
	set(CMAKE_CXX_STANDARD 11)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 99)

if(MSVC)
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "/WX")
	endif()
	# avoid conflicts with std::min and std::max
	add_definitions("-DNOMINMAX")
else()
	if (ENABLE_PROFILING)
		list(APPEND STRICT_OPTIONS_CXX "-g -pg")
	endif()
        #list(APPEND STRICT_OPTIONS_CPP "-Wall" "-Wuninitialized" "-Wno-error=deprecated-declarations") # turn off deprecated-declaration warning to avoid being flooded by soci.h
	list(APPEND STRICT_OPTIONS_CPP "-Wall" "-Wuninitialized" "-Wno-deprecated-declarations" "-Wno-missing-field-initializers")
	
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
		list(APPEND STRICT_OPTIONS_CPP "-Qunused-arguments" "-Wno-array-bounds")
	endif()
	if(APPLE)
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=unknown-warning-option" "-Qunused-arguments" "-Wno-tautological-compare" "-Wno-unused-function" "-Wno-array-bounds")
	endif()
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "-Werror" "-Wextra" "-Wno-unused-parameter" "-fno-strict-aliasing")
	endif()
endif()
if(STRICT_OPTIONS_CPP)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_CPP)
endif()

set(EXPORT_TARGETS_NAME "lime")

if (ENABLE_CURVE25519)
	add_definitions("-DEC25519_ENABLED")
	message(STATUS "Support Curve 25519")
endif()

if (ENABLE_CURVE448)
	add_definitions("-DEC448_ENABLED")
	message(STATUS "Support Curve 448")
endif()

if(ENABLE_C_INTERFACE)
	add_definitions("-DFFI_ENABLED")
	message(STATUS "Provide C89 interface")
endif()

if(ENABLE_JNI)
	message(STATUS "Provide JNI interface")
	if (NOT ANDROID)
		find_package(JNI REQUIRED)

		if (JNI_FOUND)
			message (STATUS "JNI_INCLUDE_DIRS=${JNI_INCLUDE_DIRS}")
			message (STATUS "JNI_LIBRARIES=${JNI_LIBRARIES}")
		endif()
	endif()
endif()

add_subdirectory(include)
add_subdirectory(src)
if(ENABLE_UNIT_TESTS)
        enable_testing()
	add_subdirectory(tester)
endif()

set(ConfigPackageLocation "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake")

export(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}Targets.cmake"
)

configure_package_config_file(cmake/LimeConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}Config.cmake"
  	INSTALL_DESTINATION ${ConfigPackageLocation}
	NO_SET_AND_CHECK_MACRO
)

install(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE ${EXPORT_TARGETS_NAME}Targets.cmake
	DESTINATION ${ConfigPackageLocation}
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/${EXPORT_TARGETS_NAME}Config.cmake"
	DESTINATION ${ConfigPackageLocation}
)

# Doxygen
find_package(Doxygen)
if (DOXYGEN_FOUND)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
	add_custom_target(doc
		${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMENT "Generating API documentation with Doxygen" VERBATIM
)
endif()

if (ENABLE_PACKAGE_SOURCE)
	add_subdirectory(build)
endif()
