############################################################################
# FindOpenCoreAMRNB.cmake
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
# Find the opencore-amrnb library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  opencore-amrnb - If the opencore-amrnb library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  OpenCoreAMRNB_FOUND - The opencore-amrnb library has been found
#  OpenCoreAMRNB_TARGET - The name of the CMake target for the opencore-amrnb library
#
# This module may set the following variable:
#
#  OpenCoreAMRNB_USE_BUILD_INTERFACE - If the opencore-amrnb library is used from its build directory


include(FindPackageHandleStandardArgs)

set(_OpenCoreAMRNB_REQUIRED_VARS OpenCoreAMRNB_TARGET)
set(_OpenCoreAMRNB_CACHE_VARS ${_OpenCoreAMRNB_REQUIRED_VARS})

if(TARGET opencore-amrnb)

	set(OpenCoreAMRNB_TARGET opencore-amrnb)
	set(OpenCoreAMRNB_USE_BUILD_INTERFACE TRUE)

else()

	set(_OpenCoreAMRNB_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

	find_path(_OpenCoreAMRNB_INCLUDE_DIRS
		NAMES opencore-amrnb/interf_dec.h
		HINTS ${_OpenCoreAMRNB_ROOT_PATHS}
		PATH_SUFFIXES include
	)

	find_library(_OpenCoreAMRNB_LIBRARY
		NAMES opencore-amrnb
		HINTS ${_OpenCoreAMRNB_ROOT_PATHS}
		PATH_SUFFIXES bin lib
	)

	if(_OpenCoreAMRNB_INCLUDE_DIRS AND _OpenCoreAMRNB_LIBRARY)
		add_library(opencore-amrnb UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(opencore-amrnb PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_OpenCoreAMRNB_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_OpenCoreAMRNB_LIBRARY}"
			)
		else()
			set_target_properties(opencore-amrnb PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_OpenCoreAMRNB_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_OpenCoreAMRNB_LIBRARY}"
			)
		endif()

		set(OpenCoreAMRNB_TARGET opencore-amrnb)
	endif()

endif()

find_package_handle_standard_args(OpenCoreAMRNB
	REQUIRED_VARS ${_OpenCoreAMRNB_REQUIRED_VARS}
)
mark_as_advanced(${_OpenCoreAMRNB_CACHE_VARS})
