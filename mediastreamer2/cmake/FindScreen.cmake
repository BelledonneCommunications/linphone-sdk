############################################################################
# FindScreen.cmake
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
# Find the screen library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  screen - If the screen library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Screen_FOUND - The screen library has been found
#  Screen_TARGET - The name of the CMake target for the screen library

set(_Screen_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

find_path(_Screen_INCLUDE_DIRS
	NAMES screen/screen.h
	HINTS ${_Screen_ROOT_PATHS}
	PATH_SUFFIXES include
)
find_library(_Screen_LIBRARY
	NAMES screen
	HINTS ${_Screen_ROOT_PATHS}
	PATH_SUFFIXES lib
)

if(_Screen_INCLUDE_DIRS AND _Screen_LIBRARY)
		add_library(screen UNKNOWN IMPORTED)
		set_target_properties(screen PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${_Screen_INCLUDE_DIRS}"
			IMPORTED_LOCATION "${_Screen_LIBRARY}"
		)
		set(Screen_TARGET screen)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Screen REQUIRED_VARS Screen_TARGET)
mark_as_advanced(Screen_TARGET)
