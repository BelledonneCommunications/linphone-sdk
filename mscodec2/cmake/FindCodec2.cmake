############################################################################
# FindCodec2.cmake
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
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
# Find the codec2 library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  codec2 - If the codec2 library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Codec2_FOUND - The codec2 library has been found
#  Codec2_TARGET - The name of the CMake target for the codec2 library
#
# This module may set the following variable:
#
#  Codec2_USE_BUILD_INTERFACE - If the codec2 library is used from its build directory


include(FindPackageHandleStandardArgs)

set(_Codec2_REQUIRED_VARS Codec2_TARGET)
set(_Codec2_CACHE_VARS ${_Codec2_REQUIRED_VARS})

if(TARGET codec2)

	set(Codec2_TARGET codec2)
	set(Codec2_USE_BUILD_INTERFACE TRUE)

else()

	find_path(_Codec2_INCLUDE_DIRS
		NAMES codec2/codec2.h
		PATH_SUFFIXES include
	)

	find_library(_Codec2_LIBRARY NAMES codec2)

	if(_Codec2_INCLUDE_DIRS AND _Codec2_LIBRARY)
	add_library(codec2 UNKNOWN IMPORTED)
		if(WIN32)
		set_target_properties(codec2 PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Codec2_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_Codec2_LIBRARY}"
			)
		else()
		set_target_properties(codec2 PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Codec2_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_Codec2_LIBRARY}"
			)
		endif()

	set(Codec2_TARGET codec2)
	endif()

endif()

find_package_handle_standard_args(Codec2
	REQUIRED_VARS ${_Codec2_REQUIRED_VARS}
)
mark_as_advanced(${_Codec2_CACHE_VARS})
