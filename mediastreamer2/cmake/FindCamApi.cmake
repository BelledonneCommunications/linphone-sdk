############################################################################
# FindCamApi.cmake
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
# Find the camapi library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  camapi - If the camapi library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  CamApi_FOUND - The camapi library has been found
#  CamApi_TARGET - The name of the CMake target for the camapi library

set(_CamApi_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

find_path(_CamApi_INCLUDE_DIRS
	NAMES camera/camera_api.h
	HINTS ${_CamApi_ROOT_PATHS}
	PATH_SUFFIXES include
)
find_library(_CamApi_LIBRARY
	NAMES camapi
	HINTS ${_CamApi_ROOT_PATHS}
	PATH_SUFFIXES lib
)

if(_CamApi_INCLUDE_DIRS AND _CamApi_LIBRARY)
		add_library(camapi UNKNOWN IMPORTED)
		set_target_properties(camapi PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${_CamApi_INCLUDE_DIRS}"
			IMPORTED_LOCATION "${_CamApi_LIBRARY}"
		)
		set(CamApi_TARGET camapi)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CamApi REQUIRED_VARS CamApi_TARGET)
mark_as_advanced(CamApi_TARGET)
