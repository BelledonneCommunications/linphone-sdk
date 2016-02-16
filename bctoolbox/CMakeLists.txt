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

cmake_minimum_required(VERSION 3.0)
project(BCTOOLBOX C CXX)

set(PACKAGE "bctoolbox")
set(PACKAGE_NAME ${PACKAGE})
set(PACKAGE_VERSION_MAJOR "0")
set(PACKAGE_VERSION_MINOR "0")
set(PACKAGE_VERSION_PATCH "1")
set(PACKAGE_VERSION "${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}.${PACKAGE_VERSION_PATCH}")
set(PACKAGE_STRING "${PACKAGE_NAME}-${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "jehan.monnier@linphone.org")
set(PACKAGE_TARNAME "bctoolbox")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")

option(ENABLE_STATIC "Build static library (default is shared library)." NO)
option(ENABLE_POLARSSL "Enable polarssl support" ON)
option(ENABLE_MBEDTLS "Enable mabedtls support" ON)

include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CMakePushCheckState)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(ENABLE_MBEDTLS)
	find_package(MbedTLS)
	if(MBEDTLS_FOUND)
		message("Use mbedTLS")
	endif()
endif()
if(ENABLE_POLARSSL AND NOT MBEDTLS_FOUND)
	find_package(PolarSSL REQUIRED)
	if(POLARSSL_FOUND)
		message("Use polarSSL")
	endif()
endif()

if(HAVE_SSL_GET_DTLS_SRTP_PROTECTION_PROFILE)
	message("DTLS SRTP available")
	set(HAVE_DTLS_SRTP 1)
else()
	message("DTLS SRTP not available")
endif()

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${prefix}/bin)
set(libdir ${prefix}/lib)
set(includedir ${prefix}/include)

if(MBEDTLS_FOUND)
	get_filename_component(mbedtls_library_path "${MBEDTLS_LIBRARIES}" PATH)
	set(LIBS_PRIVATE "${LIBS_PRIVATE} -L${mbedlts_library_path}")
endif()
if(POLARSSL_FOUND)
	get_filename_component(polarssl_library_path "${POLARSSL_LIBRARIES}" PATH)
	set(LIBS_PRIVATE "${LIBS_PRIVATE} -L${polarssl_library_path} -lpolarssl")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/bctoolbox.pc.in ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bctoolbox.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)


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
if(WIN32)
	add_definitions(
		-DBCTOOLBOX_EXPORTS
		-DBCTOOLBOX_INTERNAL_EXPORTS
	)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone")
	add_definitions(
		-DHAVE_COMPILER_TLS
	)
endif()


if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	set(EXPORT_TARGETS_NAME "LinphoneBuilder")
else()
	set(EXPORT_TARGETS_NAME "BcToolbox")
endif()

add_compile_options(${STRICT_OPTIONS_CPP} ${STRICT_OPTIONS_C})


add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(build)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/BcToolboxConfigVersion.cmake"
	VERSION ${PACKAGE_VERSION}
	COMPATIBILITY AnyNewerVersion
)
export(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/BcToolboxTargets.cmake"
)
configure_file(cmake/BcToolboxConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/BcToolboxConfig.cmake"
	@ONLY
)

set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/BcToolbox)
install(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE BcToolboxTargets.cmake
	DESTINATION ${ConfigPackageLocation}
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/BcToolboxConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/BcToolboxConfigVersion.cmake"
	DESTINATION ${ConfigPackageLocation}
)

