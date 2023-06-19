############################################################################
# FindGSM.cmake
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
# Find the gsm library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  gsm - If the gsm library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  GSM_FOUND - The gsm library has been found
#  GSM_TARGET - The name of the CMake target for the gsm library
#
# This module may set the following variable:
#
#  GSM_USE_BUILD_INTERFACE - If the gsm library is used from its build directory


include(FindPackageHandleStandardArgs)

set(_GSM_REQUIRED_VARS GSM_TARGET)
set(_GSM_CACHE_VARS ${_GSM_REQUIRED_VARS})

if(TARGET gsm)

	set(GSM_TARGET gsm)
	set(GSM_USE_BUILD_INTERFACE TRUE)

else()

	set(_GSM_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

	find_path(_GSM_INCLUDE_DIRS
		NAMES gsm/gsm.h
		HINTS ${_GSM_ROOT_PATHS}
		PATH_SUFFIXES include
	)

	find_library(_GSM_LIBRARY
		NAMES gsm
		HINTS ${_GSM_ROOT_PATHS}
		PATH_SUFFIXES bin lib
	)

	if(_GSM_INCLUDE_DIRS AND _GSM_LIBRARY)
		add_library(gsm UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(gsm PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_GSM_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_GSM_LIBRARY}"
			)
		else()
			set_target_properties(gsm PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_GSM_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_GSM_LIBRARY}"
			)
		endif()

		set(GSM_TARGET gsm)
	endif()

endif()

find_package_handle_standard_args(GSM
	REQUIRED_VARS ${_GSM_REQUIRED_VARS}
)
mark_as_advanced(${_GSM_CACHE_VARS})
