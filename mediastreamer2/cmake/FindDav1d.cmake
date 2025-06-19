############################################################################
# FindDav1d.txt
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
# This file is part of mediastreamer2.
############################################################################
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#
############################################################################
#
# Find the dav1d library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  dav1d - If the dav1d library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Dav1d_FOUND - The dav1d library has been found
#  Dav1d_TARGET - The name of the CMake target for the dav1d library


include(FindPackageHandleStandardArgs)

set(_Dav1d_REQUIRED_VARS Dav1d_TARGET)
set(_Dav1d_CACHE_VARS ${_Dav1d_REQUIRED_VARS})

if(TARGET dav1d)

	set(Dav1d_TARGET libdav1d)

else()

	set(_Dav1d_ROOT_PATHS
		${CMAKE_INSTALL_PREFIX}
	)

	find_path(Dav1d_INCLUDE_DIRS
		NAMES dav1d/dav1d.h
		HINTS _Dav1d_ROOT_PATHS
		PATH_SUFFIXES include
	)

	find_library(Dav1d_LIBRARY
		NAMES dav1d
		HINTS _Dav1d_ROOT_PATHS
		PATH_SUFFIXES bin lib lib/Win32
	)

	if(_Dav1d_INCLUDE_DIRS AND _Dav1d_LIBRARY)
		add_library(libdav1d UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(libdav1d PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Dav1d_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_Dav1d_LIBRARY}"
			)
		else()
			set_target_properties(libdav1d PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Dav1d_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_Dav1d_LIBRARY}"
			)
		endif()

		set(Dav1d_TARGET libdav1d)
	endif()
endif()

find_package_handle_standard_args(Dav1d
	REQUIRED_VARS ${_Dav1d_REQUIRED_VARS}
)
mark_as_advanced(${_Dav1d_CACHE_VARS})
