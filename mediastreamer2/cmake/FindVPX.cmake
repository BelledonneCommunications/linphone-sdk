############################################################################
# FindVPX.cmake
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
# Find the vpx library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  vpx - If the vpx library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  VPX_FOUND - The vpx library has been found
#  VPX_TARGET - The name of the CMake target for the vpx library


include(FindPackageHandleStandardArgs)

set(_VPX_REQUIRED_VARS VPX_TARGET)
set(_VPX_CACHE_VARS ${_VPX_REQUIRED_VARS})

if(TARGET vpx)

	set(VPX_TARGET libvpx)

else()

	set(_VPX_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

	find_path(_VPX_INCLUDE_DIRS
		NAMES vpx/vpx_encoder.h
		HINTS ${_VPX_ROOT_PATHS}
		PATH_SUFFIXES include
	)

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		find_library(_VPX_LIBRARY
			NAMES vpx vpxmd
			HINTS ${_VPX_ROOT_PATHS}
			PATH_SUFFIXES bin lib lib/x64
		)
	else()
		find_library(_VPX_LIBRARY
			NAMES vpx vpxmd
			HINTS ${_VPX_ROOT_PATHS}
			PATH_SUFFIXES bin lib lib/Win32
		)
	endif()

	if(_VPX_INCLUDE_DIRS AND _VPX_LIBRARY)
		add_library(vpx UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(vpx PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_VPX_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_VPX_LIBRARY}"
			)
		else()
			set_target_properties(vpx PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_VPX_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_VPX_LIBRARY}"
			)
		endif()

		set(VPX_TARGET vpx)
	endif()

endif()

find_package_handle_standard_args(VPX
	REQUIRED_VARS ${_VPX_REQUIRED_VARS}
)
mark_as_advanced(${_VPX_CACHE_VARS})
