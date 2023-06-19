############################################################################
# FindBV16.cmake
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
# Find the bv16 library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  bv16 - If the bv16 library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  BV16_FOUND - The bv16 library has been found
#  BV16_TARGET - The name of the CMake target for the bv16 library
#
# This module may set the following variable:
#
#  BV16_USE_BUILD_INTERFACE - If the bv16 library is used from its build directory


include(FindPackageHandleStandardArgs)

set(_BV16_REQUIRED_VARS BV16_TARGET)
set(_BV16_CACHE_VARS ${_BV16_REQUIRED_VARS})

if(TARGET bv16)

	set(BV16_TARGET bv16)
	set(BV16_USE_BUILD_INTERFACE TRUE)

else()

	find_path(_BV16_INCLUDE_DIRS
		NAMES bv16-floatingpoint/bv16/bv16.h
		PATH_SUFFIXES include
	)

	find_library(_BV16_LIBRARY NAMES bv16)

	if(_BV16_INCLUDE_DIRS AND _BV16_LIBRARY)
		add_library(bv16 UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(bv16 PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_BV16_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_BV16_LIBRARY}"
			)
		else()
			set_target_properties(bv16 PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_BV16_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_BV16_LIBRARY}"
			)
		endif()

		set(BV16_TARGET bv16)
	endif()

endif()

find_package_handle_standard_args(BV16
	REQUIRED_VARS ${_BV16_REQUIRED_VARS}
)
mark_as_advanced(${_BV16_CACHE_VARS})
