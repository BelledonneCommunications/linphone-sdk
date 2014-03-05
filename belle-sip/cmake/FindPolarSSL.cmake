############################################################################
# FindPolarSSL.txt
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################
#
# - Find the polarssl include file and library
#
#  POLARSSL_FOUND - system has polarssl
#  POLARSSL_INCLUDE_DIR - the polarssl include directory
#  POLARSSL_LIBRARIES - The libraries needed to use polarssl

if("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
	include(CMakePushCheckState)
endif("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
include(CheckIncludeFile)
include(CheckCSourceCompiles)

set(_POLARSSL_ROOT_PATHS
	${WITH_POLARSSL}
	${CMAKE_INSTALL_PREFIX}
)

find_path(POLARSSL_INCLUDE_DIR
	NAMES polarssl/ssl.h
	HINTS _POLARSSL_ROOT_PATHS
	PATH_SUFFIXES include
)
if(NOT "${POLARSSL_INCLUDE_DIR}" STREQUAL "")
	set(HAVE_POLARSSL_SSL_H 1)

	find_library(POLARSSL_LIBRARIES
		NAMES polarssl
		HINTS _POLARSSL_ROOT_PATHS
		PATH_SUFFIXES bin lib
	)

	if(NOT "${POLARSSL_LIBRARIES}" STREQUAL "")
		set(POLARSSL_FOUND TRUE)

		if("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
			cmake_push_check_state(RESET)
		else("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
			set(SAVE_CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES})
			set(SAVE_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
		endif("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
		set(CMAKE_REQUIRED_INCLUDES ${POLARSSL_INCLUDE_DIR})
		set(CMAKE_REQUIRED_LIBRARIES ${POLARSSL_LIBRARIES})
		check_c_source_compiles("#include <polarssl/version.h>
#include <polarssl/x509.h>
#if POLARSSL_VERSION_NUMBER >= 0x01030000
#include <polarssl/compat-1.2.h>
#endif
int main(int argc, char *argv[]) {
x509parse_crtpath(0,0);
return 0;
}"
			X509PARSE_CRTPATH_OK)
		if("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
			cmake_pop_check_state()
		else("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
			set(CMAKE_REQUIRED_INCLUDES ${SAVE_CMAKE_REQUIRED_INCLUDES})
			set(CMAKE_REQUIRED_LIBRARIES ${SAVE_CMAKE_REQUIRED_LIBRARIES})
		endif("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
	endif(NOT "${POLARSSL_LIBRARIES}" STREQUAL "")
endif(NOT "${POLARSSL_INCLUDE_DIR}" STREQUAL "")

mark_as_advanced(POLARSSL_INCLUDE_DIR POLARSSL_LIBRARIES)
