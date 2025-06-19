############################################################################
# FindArts.cmake
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
# Find the artsc library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  arts - If the artsc library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Arts_FOUND - The artsc library has been found
#  Arts_TARGET - The name of the CMake target for the artsc library


include(FindPackageHandleStandardArgs)

set(_Arts_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

find_path(_Arts_INCLUDE_DIRS
	NAMES kde/artsc/artsc.h
	HINTS ${_Arts_ROOT_PATHS}
	PATH_SUFFIXES include
)

find_library(_Arts_LIBRARY
	NAMES artsc
	HINTS ${_Arts_ROOT_PATHS}
	PATH_SUFFIXES lib
)

if(_Arts_INCLUDE_DIRS AND _Arts_LIBRARY)
	add_library(arts UNKNOWN IMPORTED)
	set_target_properties(arts PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${_Arts_INCLUDE_DIRS}"
		IMPORTED_LOCATION "${_Arts_LIBRARY}"
	)

	set(Arts_TARGET arts)
endif()

find_package_handle_standard_args(Arts REQUIRED_VARS Arts_TARGET)
mark_as_advanced(Arts_TARGET)
