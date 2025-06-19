############################################################################
# FindAom.txt
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
# Find the aom library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  aom - If the aom library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Aom_FOUND - The aom library has been found
#  Aom_TARGET - The name of the CMake target for the aom library


include(FindPackageHandleStandardArgs)

set(_Aom_REQUIRED_VARS Aom_TARGET)
set(_Aom_CACHE_VARS ${_Aom_REQUIRED_VARS})

if(TARGET aom)

	set(Aom_TARGET aom)

else()

	set(_Aom_ROOT_PATHS
		${CMAKE_INSTALL_PREFIX}
	)

	find_path(Aom_INCLUDE_DIRS
		NAMES aom/aomcx.h
		HINTS _Aom_ROOT_PATHS
		PATH_SUFFIXES include
	)

	find_library(Aom_LIBRARY
		NAMES aom
		HINTS _Aom_ROOT_PATHS
		PATH_SUFFIXES bin lib lib/Win32
	)

	if(_Aom_INCLUDE_DIRS AND _Aom_LIBRARY)
		add_library(aom UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(aom PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Aom_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_Aom_LIBRARY}"
			)
		else()
			set_target_properties(aom PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Aom_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_Aom_LIBRARY}"
			)
		endif()

		set(Aom_TARGET aom)
	endif()
endif()

find_package_handle_standard_args(Aom
	REQUIRED_VARS ${_Aom_REQUIRED_VARS}
)
mark_as_advanced(${_Aom_CACHE_VARS})
