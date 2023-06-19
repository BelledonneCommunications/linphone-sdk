############################################################################
# FindPortAudio.cmake
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
# Find the portaudio library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  portaudio - If the portaudio library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  PortAudio_FOUND - The portaudio library has been found
#  PortAudio_TARGET - The name of the CMake target for the portaudio library

set(_PortAudio_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

find_path(_PortAudio_INCLUDE_DIRS
	NAMES portaudio.h
	HINTS ${_PortAudio_ROOT_PATHS}
	PATH_SUFFIXES include
)
find_library(_PortAudio_LIBRARY
	NAMES portaudio
	HINTS ${_PortAudio_ROOT_PATHS}
	PATH_SUFFIXES bin lib
)

if(_PortAudio_INCLUDE_DIRS AND _PortAudio_LIBRARY)
		add_library(portaudio UNKNOWN IMPORTED)
		set_target_properties(portaudio PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${_PortAudio_INCLUDE_DIRS}"
			IMPORTED_LOCATION "${_PortAudio_LIBRARY}"
		)

		set(PortAudio_TARGET portaudio)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PortAudio REQUIRED_VARS PortAudio_TARGET)

mark_as_advanced(PortAudio_TARGET)
