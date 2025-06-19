############################################################################
# FindQnxAudioManager.cmake
# Copyright (C) 2015-2023  Belledonne Communications, Grenoble France
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
# Find the audio_manager library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  qnxaudiomanager - If the audio_manager library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  QnxAudioManager_FOUND - The audio_manager library has been found
#  QnxAudioManager_TARGET - The name of the CMake target for the audio_manager library

set(_QnxAudioManager_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

find_path(_QnxAudioManager_INCLUDE_DIRS
	NAMES audio/audio_manager_routing.h
	HINTS ${_QnxAudioManager_ROOT_PATHS}
	PATH_SUFFIXES include
)
find_library(_QnxAudioManager_LIBRARY
	NAMES audio_manager
	HINTS ${_QnxAudioManager_ROOT_PATHS}
	PATH_SUFFIXES bin lib
)

if(_QnxAudioManager_INCLUDE_DIRS AND _QnxAudioManager_LIBRARY)
		add_library(qnxaudiomanager UNKNOWN IMPORTED)
		set_target_properties(qnxaudiomanager PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${_QnxAudioManager_INCLUDE_DIRS}"
			IMPORTED_LOCATION "${_QnxAudioManager_LIBRARY}"
		)

		set(QnxAudioManager_TARGET qnxaudiomanager)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QnxAudioManager REQUIRED_VARS QnxAudioManager_TARGET)
mark_as_advanced(QnxAudioManager_TARGET)
