############################################################################
# FindSRTP.cmake
# Copyright (C) 2014-2023  Belledonne Communications, Grenoble France
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
# Find the srtp library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  srtp2 - If the srtp library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  SRTP_FOUND - The srtp library has been found
#  SRTP_TARGET - The name of the CMake target for the srtp library
#  SRTP_VERSION - The vresion of the srtp library
#
# This module may set the following variables in your project:
#  SRTP_NOT_INSTALLED - If the srtp library has been installed and is used from its build dir


include(FindPackageHandleStandardArgs)

set(_SRTP_REQUIRED_VARS SRTP_TARGET SRTP_VERSION)
set(_SRTP_CACHE_VARS ${_SRTP_REQUIRED_VARS} SRTP_NOT_INSTALLED)

if(TARGET srtp2)

	if(SRTP_FIND_VERSION_MAJOR LESS_EQUAL 2)
		set(SRTP_TARGET srtp2)
		set(SRTP_VERSION 2)
		set(SRTP_NOT_INSTALLED TRUE)
	endif()

else()

	set(_SRTP_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

	find_path(_SRTP_INCLUDE_DIRS
		NAMES srtp2/srtp.h
		HINTS ${_SRTP_ROOT_PATHS}
		PATH_SUFFIXES include
	)

	if(_SRTP_INCLUDE_DIRS)
		set(SRTP_VERSION 2)
		find_library(_SRTP_LIBRARY
			NAMES srtp2
			HINTS ${_SRTP_ROOT_PATHS}
			PATH_SUFFIXES bin lib
		)
		set(_SRTP_TARGET_NAME srtp2)
	else()
		set(SRTP_VERSION 1)
		find_path(_SRTP_INCLUDE_DIRS
			NAMES srtp/srtp.h
			HINTS ${_SRTP_ROOT_PATHS}
			PATH_SUFFIXES include
		)
		find_library(_SRTP_LIBRARY
			NAMES srtp
			HINTS ${_SRTP_ROOT_PATHS}
			PATH_SUFFIXES bin lib
		)
		set(_SRTP_TARGET_NAME srtp)
	endif()

	if(NOT SRTP_FIND_VERSION_MAJOR OR SRTP_VERSION GREATER_EQUAL SRTP_FIND_VERSION_MAJOR)
		set(_SRTP_VERSION_OK TRUE)
	else()
		set(_SRTP_VERSION_OK FALSE)
	endif()

	if(_SRTP_INCLUDE_DIRS AND _SRTP_LIBRARY AND _SRTP_VERSION_OK)
		add_library(${_SRTP_TARGET_NAME} UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(${_SRTP_TARGET_NAME} PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_SRTP_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_SRTP_LIBRARY}"
			)
		else()
			set_target_properties(${_SRTP_TARGET_NAME} PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_SRTP_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_SRTP_LIBRARY}"
			)
		endif()

		set(SRTP_TARGET ${_SRTP_TARGET_NAME})
	endif()

endif()

find_package_handle_standard_args(SRTP
	REQUIRED_VARS ${_SRTP_REQUIRED_VARS}
)
mark_as_advanced(${_SRTP_CACHE_VARS})
