############################################################################
# FindMbedTLS.cmake
# Copyright (C) 2015-2023  Belledonne Communications, Grenoble France
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
#
# Find the mbedtls library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  mbedtls - If the mbedtls library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  MbedTLS_FOUND - The mbedtls library has been found
#  MbedTLS_TARGET - The name of the CMake target for the mbedtls library
#  MbedX509_TARGET - The name of the CMake target for the mbedx509 library
#  MbedCrypto_TARGET - The name of the CMake target for the mbedcrypto library
#  MbedTLS_VERSION - The version of the medtls library
#  DTLS_SRTP_AVAILABLE - If the mbedtls library has DTLS-SRTP support


include(FindPackageHandleStandardArgs)

set(_MbedTLS_REQUIRED_VARS MbedTLS_TARGET MbedX509_TARGET MbedCrypto_TARGET MbedTLS_VERSION)
set(_MbedTLS_CACHE_VARS
	${_MbedTLS_REQUIRED_VARS} DTLS_SRTP_AVAILABLE
)

if(TARGET mbedtls)

	set(MbedTLS_TARGET mbedtls)
	set(MbedX509_TARGET mbedx509)
	set(MbedCrypto_TARGET mbedcrypto)

	get_target_property(MbedTLS_VERSION mbedtls VERSION)
	if (MbedTLS_VERSION EQUAL "MbedTLS_VERSION-NOTFOUND")
		set(MbedTLS_VERSION 3)
	else()
		string(REPLACE "." ";" MbedTLS_VERSION "${MbedTLS_VERSION}")
		list(GET MbedTLS_VERSION 0 MbedTLS_VERSION)
		# We are building mbedTLS, we have DTLS-SRTP support
		set(DTLS_SRTP_AVAILABLE ON)
	endif()

else()

	include(CMakePushCheckState)
	include(CheckSymbolExists)

	find_path(_MbedTLS_INCLUDE_DIRS
		NAMES mbedtls/ssl.h
		PATH_SUFFIXES include
	)

	# Find the three mbedtls libraries
	find_library(_MbedTLS_LIBRARY
		NAMES mbedtls
	)

	find_library(_MbedX509_LIBRARY
		NAMES mbedx509
	)

	find_library(_MbedCrypto_LIBRARY
		NAMES mbedcrypto
	)

	cmake_push_check_state(RESET)
	set(CMAKE_REQUIRED_INCLUDES ${_MbedTLS_INCLUDE_DIRS} ${CMAKE_REQUIRED_INCLUDES_${BUILD_TYPE}})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${_MbedTLS_LIBRARY} ${_MbedX509_LIBRARY} ${_MbedCrypto_LIBRARY})

	# Check that we have a mbedTLS version 2 or above (all functions are prefixed mbedtls_)
	if(_MbedTLS_LIBRARY AND _MbedX509_LIBRARY AND _MbedCrypto_LIBRARY)
		check_symbol_exists(mbedtls_ssl_init "mbedtls/ssl.h" _MbedTLS_V2)
	  if(NOT _MbedTLS_V2)
  	  message("MESSAGE: NO MbedTLS_V2")
    	message("MESSAGE: MbedTLS_LIBRARY=" ${_MbedTLS_LIBRARY})
	    message("MESSAGE: MbedX509_LIBRARY=" ${MbedX509_LIBRARY})
  	  message("MESSAGE: MbedCrypto_LIBRARY=" ${MbedCrypto_LIBRARY})
			set(MbedTLS_VERSION 1)
		else()
			# Are we mbdetls 2 or 3?
			# From version 3 and on, version number is given in include/mbedtls/build_info.h.
			# This file does not exist before version 3
			if(EXISTS "${_MbedTLS_INCLUDE_DIRS}/mbedtls/build_info.h")
				set(MbedTLS_VERSION 3)
			else()
				set(MbedTLS_VERSION 2)
			endif()
		endif()
	endif()

	check_symbol_exists(mbedtls_ssl_conf_dtls_srtp_protection_profiles "mbedtls/ssl.h" DTLS_SRTP_AVAILABLE)

	cmake_pop_check_state()

	# Define the imported target for the three mbedtls libraries
	foreach(_VARPREFIX "MbedTLS" "MbedX509" "MbedCrypto")
		string(TOLOWER ${_VARPREFIX} _TARGET)
		add_library(${_TARGET} UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(${_TARGET} PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_MbedTLS_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_${_VARPREFIX}_LIBRARY}"
			)
		else()
			set_target_properties(${_TARGET} PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_MbedTLS_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_${_VARPREFIX}_LIBRARY}"
			)
		endif()
	endforeach()

	set(MbedTLS_TARGET mbedtls)
	set(MbedX509_TARGET mbedx509)
	set(MbedCrypto_TARGET mbedcrypto)

endif()

find_package_handle_standard_args(MbedTLS
	REQUIRED_VARS ${_MbedTLS_REQUIRED_VARS}
)
mark_as_advanced(${_MbedTLS_CACHE_VARS})
