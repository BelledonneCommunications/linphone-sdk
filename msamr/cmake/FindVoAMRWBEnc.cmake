############################################################################
# FindVoAMRWBEnc.cmake
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
# Find the vo-amrwbenc library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  vo-amrwbenc - If the vo-amrwbenc library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  VoAMRWBEnc_FOUND - The vo-amrwbenc library has been found
#  VoAMRWBEnc_TARGET - The name of the CMake target for the vo-amrwbenc library
#
# This module may set the following variable:
#
#  VoAMRWBEnc_USE_BUILD_INTERFACE - If the vo-amrwbenc library is used from its build directory


include(FindPackageHandleStandardArgs)

set(_VoAMRWBEnc_REQUIRED_VARS VoAMRWBEnc_TARGET)
set(_VoAMRWBEnc_CACHE_VARS ${_VoAMRWBEnc_REQUIRED_VARS})

if(TARGET vo-amrwbenc)

	set(VoAMRWBEnc_TARGET vo-amrwbenc)
	set(VoAMRWBEnc_USE_BUILD_INTERFACE TRUE)

else()

	set(_VoAMRWBEnc_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

	find_path(_VoAMRWBEnc_INCLUDE_DIRS
	NAMES vo-amrwbenc/enc_if.h
		HINTS ${_VoAMRWBEnc_ROOT_PATHS}
		PATH_SUFFIXES include
	)

	find_library(_VoAMRWBEnc_LIBRARY
	NAMES vo-amrwbenc
		HINTS ${_VoAMRWBEnc_ROOT_PATHS}
		PATH_SUFFIXES bin lib
	)

	if(_VoAMRWBEnc_INCLUDE_DIRS AND _VoAMRWBEnc_LIBRARY)
	add_library(vo-amrwbenc UNKNOWN IMPORTED)
		if(WIN32)
		set_target_properties(vo-amrwbenc PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_VoAMRWBEnc_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_VoAMRWBEnc_LIBRARY}"
			)
		else()
		set_target_properties(vo-amrwbenc PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_VoAMRWBEnc_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_VoAMRWBEnc_LIBRARY}"
			)
		endif()

	set(VoAMRWBEnc_TARGET vo-amrwbenc)
	endif()

endif()

find_package_handle_standard_args(VoAMRWBEnc
	REQUIRED_VARS ${_VoAMRWBEnc_REQUIRED_VARS}
)
mark_as_advanced(${_VoAMRWBEnc_CACHE_VARS})
