############################################################################
# FindPolarSSL.txt
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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
#  POLARSSL_INCLUDE_DIRS - the polarssl include directory
#  POLARSSL_LIBRARIES - The libraries needed to use polarssl

include(CMakePushCheckState)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckSymbolExists)

set(_POLARSSL_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(POLARSSL_INCLUDE_DIRS
	NAMES polarssl/ssl.h
	HINTS _POLARSSL_ROOT_PATHS
	PATH_SUFFIXES include
)
if(POLARSSL_INCLUDE_DIRS)
	set(HAVE_POLARSSL_SSL_H 1)
endif()

find_library(POLARSSL_LIBRARIES
	NAMES polarssl mbedtls
	HINTS _POLARSSL_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

if(POLARSSL_LIBRARIES)
	cmake_push_check_state(RESET)
	set(CMAKE_REQUIRED_INCLUDES ${POLARSSL_INCLUDE_DIRS})
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
	check_symbol_exists(ssl_get_dtls_srtp_protection_profile "polarssl/ssl.h" HAVE_SSL_GET_DTLS_SRTP_PROTECTION_PROFILE)
	cmake_pop_check_state()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PolarSSL
	DEFAULT_MSG
	POLARSSL_INCLUDE_DIRS POLARSSL_LIBRARIES HAVE_POLARSSL_SSL_H
)

mark_as_advanced(POLARSSL_INCLUDE_DIRS POLARSSL_LIBRARIES HAVE_POLARSSL_SSL_H X509PARSE_CRTPATH_OK HAVE_SSL_GET_DTLS_SRTP_PROTECTION_PROFILE)
