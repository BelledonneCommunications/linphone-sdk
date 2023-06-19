############################################################################
# CMakeLists.txt
# Copyright (C) 2010-2023  Belledonne Communications, Grenoble France
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

project(BZRTP VERSION 5.3.0 LANGUAGES C CXX)

option(ENABLE_ZIDCACHE "Turn on compilation of ZID cache, request sqlite" YES)
option(ENABLE_STRICT "Build with strict compile options." YES)
option(ENABLE_UNIT_TESTS "Enable compilation of unit tests." NO)
option(ENABLE_DOC "Enable API documentation generation." NO)
option(ENABLE_PACKAGE_SOURCE "Create 'package_source' target for source archive making" OFF)
option(ENABLE_GOCLEAR "Enable the possibility to send and receive GoClear" YES)
option(ENABLE_PQCRYPTO "Enable Post Quantum Cryptography key agreements algorithms" NO)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS NO)

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(CheckLibraryExists)
check_library_exists("m" "sqrt" "" HAVE_SQRT)

find_package(BCToolbox 5.3.0 REQUIRED OPTIONAL_COMPONENTS tester)
if(ENABLE_PQCRYPTO)
	find_package(PostQuantumCryptoEngine 5.3.0 REQUIRED)
endif()
if(ENABLE_ZIDCACHE)
	bc_find_package(SQLite3 SQLite::SQLite3 sqlite3 REQUIRED)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/config.h PROPERTIES GENERATED ON)
add_definitions("-DHAVE_CONFIG_H")

bc_init_compilation_flags(STRICT_OPTIONS_CPP STRICT_OPTIONS_C STRICT_OPTIONS_CXX ENABLE_STRICT)

set(BZRTP_CPPFLAGS ${BCTOOLBOX_CPPFLAGS})
if(NOT BUILD_SHARED_LIBS)
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

if(ENABLE_ZIDCACHE)
	add_definitions("-DZIDCACHE_ENABLED")
endif()

if(ENABLE_GOCLEAR)
        add_definitions("-DGOCLEAR_ENABLED")
endif()

if(ENABLE_PQCRYPTO)
	add_definitions("-DHAVE_BCTBXPQ")
	message(STATUS "Building with Post Quantum Key Agreements")
endif()

add_subdirectory(include)
add_subdirectory(src)
if(ENABLE_UNIT_TESTS AND NOT ANDROID AND NOT IOS)
	enable_testing()
	add_subdirectory(test)
endif()

if(ENABLE_DOC)
	# Doxygen
	find_package(Doxygen)
	if (DOXYGEN_FOUND)
		set(DOXYGEN_INPUT "")
		file(GLOB DOC_INPUT_FILES
			"${PROJECT_SOURCE_DIR}/include/[^.]*.h"
			"${PROJECT_SOURCE_DIR}/src/[^.]*.c"
			"${PROJECT_SOURCE_DIR}/src/[^.]*.cc"
		)
		foreach (INPUT_FILE ${DOC_INPUT_FILES})
			string(CONCAT DOXYGEN_INPUT ${DOXYGEN_INPUT} " \"${INPUT_FILE}\"")
		endforeach ()
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
		add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/doc/html/index.html"
			COMMAND "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile"
			DEPENDS "${PROJECT_BINARY_DIR}/Doxyfile" ${DOC_INPUT_FILES}
		)
		add_custom_target(bzrtp-doc ALL
			DEPENDS "${PROJECT_BINARY_DIR}/doc/html/index.html"
			COMMENT "Generating API documentation with Doxygen" VERBATIM
		)
		install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc/html/"
			DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/bzrtp-${PROJECT_VERSION}")
	endif()
endif()

if(ENABLE_PACKAGE_SOURCE)
	add_subdirectory(build)
endif()

include(CMakePackageConfigHelpers)
set(CMAKE_MODULES_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake")
configure_package_config_file("cmake/${PROJECT_NAME}Config.cmake.in" "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
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
if(ENABLE_PQCRYPTO)
	install(FILES "cmake/FindPostQuantumCryptoEngine.cmake" DESTINATION ${CMAKE_MODULES_INSTALL_DIR})
endif()

install(EXPORT ${PROJECT_NAME}Targets
	FILE "${PROJECT_NAME}Targets.cmake"
	DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
)
