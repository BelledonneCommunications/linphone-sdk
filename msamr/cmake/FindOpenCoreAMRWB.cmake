############################################################################
# FindOpenCoreAMRWB.cmake
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
# Find the opencore-amrwb library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  opencore-amrwb - If the opencore-amrwb library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  OpenCoreAMRWB_FOUND - The opencore-amrwb library has been found
#  OpenCoreAMRWB_TARGET - The name of the CMake target for the opencore-amrwb library
#
# This module may set the following variable:
#
#  OpenCoreAMRWB_USE_BUILD_INTERFACE - If the opencore-amrwb library is used from its build directory


include(FindPackageHandleStandardArgs)

set(_OpenCoreAMRWB_REQUIRED_VARS OpenCoreAMRWB_TARGET)
set(_OpenCoreAMRWB_CACHE_VARS ${_OpenCoreAMRWB_REQUIRED_VARS})

if(TARGET opencore-amrwb)

	set(OpenCoreAMRWB_TARGET opencore-amrwb)
	set(OpenCoreAMRWB_USE_BUILD_INTERFACE TRUE)

else()

	set(_OpenCoreAMRWB_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

	find_path(_OpenCoreAMRWB_INCLUDE_DIRS
		NAMES opencore-amrwb/dec_if.h
		HINTS ${_OpenCoreAMRWB_ROOT_PATHS}
		PATH_SUFFIXES include
	)

	find_library(_OpenCoreAMRWB_LIBRARY
		NAMES opencore-amrwb
		HINTS ${_OpenCoreAMRWB_ROOT_PATHS}
		PATH_SUFFIXES bin lib
	)

	if(_OpenCoreAMRWB_INCLUDE_DIRS AND _OpenCoreAMRWB_LIBRARY)
		add_library(opencore-amrwb UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(opencore-amrwb PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_OpenCoreAMRWB_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_OpenCoreAMRWB_LIBRARY}"
			)
		else()
			set_target_properties(opencore-amrwb PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_OpenCoreAMRWB_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_OpenCoreAMRWB_LIBRARY}"
			)
		endif()

		set(OpenCoreAMRWB_TARGET opencore-amrwb)
	endif()

endif()

find_package_handle_standard_args(OpenCoreAMRWB
	REQUIRED_VARS ${_OpenCoreAMRWB_REQUIRED_VARS}
)
mark_as_advanced(${_OpenCoreAMRWB_CACHE_VARS})
