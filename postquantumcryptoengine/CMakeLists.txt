############################################################################
# CMakeLists.txt
# Copyright (C) 2022-2023  Belledonne Communications, Grenoble France
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

project(PostQuantumCryptoEngine VERSION 5.5.0)

set(POSTQUANTUMCRYPTOENGINE_VERSION "${PROJECT_VERSION}")
set(POSTQUANTUMCRYPTOENGINE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(POSTQUANTUMCRYPTOENGINE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(POSTQUANTUMCRYPTOENGINE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(POSTQUANTUMCRYPTOENGINE_SO_VERSION 1)

option(ENABLE_STRICT "Pass strict flags to the compiler" ON)
option(ENABLE_UNIT_TESTS "Enable compilation of tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS NO)

include(GNUInstallDirs)

if(NOT CPACK_GENERATOR AND NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

find_package(BCToolbox 5.3.0 REQUIRED OPTIONAL_COMPONENTS tester)
bc_find_package(liboqs OQS::oqs oqs 0.7.2 REQUIRED)
if(TARGET oqs)
	set(liboqs_USE_BUILD_INTERFACE TRUE)
endif()

include_directories(
	include
	src
	${CMAKE_CURRENT_BINARY_DIR}
)

if(NOT BUILD_SHARED_LIBS)
	set(PQCE_STATIC 1)
endif()

bc_init_compilation_flags(STRICT_OPTIONS_CPP STRICT_OPTIONS_C STRICT_OPTIONS_CXX ENABLE_STRICT)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/config.h PROPERTIES GENERATED ON)
add_definitions("-DHAVE_CONFIG_H")

add_subdirectory(include)
add_subdirectory(src)

if(ENABLE_UNIT_TESTS AND NOT ANDROID AND NOT IOS)
	enable_testing()
	add_subdirectory(tester)
endif()

include(CMakePackageConfigHelpers)
set(CMAKE_MODULES_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake")
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
