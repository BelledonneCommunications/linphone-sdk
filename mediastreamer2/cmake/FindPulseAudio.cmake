############################################################################
# FindPulseAudio.cmake
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
# Find the pulseaudio library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  pulseaudio - If the pulse library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  PulseAudio_FOUND - The pulse library has been found
#  PulseAudio_TARGET - The name of the CMake target for the pulse library

set(_PulseAudio_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

find_path(_PulseAudio_INCLUDE_DIRS
	NAMES pulse/pulseaudio.h
	HINTS ${_PulseAudio_ROOT_PATHS}
	PATH_SUFFIXES include
)
find_library(_PulseAudio_LIBRARY
	NAMES pulse
	HINTS ${_PulseAudio_ROOT_PATHS}
	PATH_SUFFIXES bin lib
)

if(_PulseAudio_INCLUDE_DIRS AND _PulseAudio_LIBRARY)
		add_library(pulseaudio UNKNOWN IMPORTED)
		set_target_properties(pulseaudio PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${_PulseAudio_INCLUDE_DIRS}"
			IMPORTED_LOCATION "${_PulseAudio_LIBRARY}"
		)

		set(PulseAudio_TARGET pulseaudio)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PulseAudio REQUIRED_VARS PulseAudio_TARGET)
mark_as_advanced(PulseAudio_TARGET)
