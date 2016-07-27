############################################################################
# FindPulseAudio.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
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
# - Find the pulseaudio include file and library
#
#  PULSEAUDIO_FOUND - system has pulseaudio
#  PULSEAUDIO_INCLUDE_DIRS - the pulseaudio include directory
#  PULSEAUDIO_LIBRARIES - The libraries needed to use pulseaudio

include(CheckSymbolExists)
include(CMakePushCheckState)

set(_PULSEAUDIO_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(PULSEAUDIO_INCLUDE_DIRS
	NAMES pulse/pulseaudio.h
	HINTS _PULSEAUDIO_ROOT_PATHS
	PATH_SUFFIXES include
)
if(PULSEAUDIO_INCLUDE_DIRS)
	set(HAVE_PULSE_PULSEAUDIO_H 1)
endif()

find_library(PULSEAUDIO_LIBRARIES
	NAMES pulse
	HINTS _PULSEAUDIO_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

if(PULSEAUDIO_LIBRARIES)
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${PULSEAUDIO_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${PULSEAUDIO_LIBRARIES})
	check_symbol_exists(pa_mainloop_new "pulse/pulseaudio.h" HAVE_PA_MAINLOOP_NEW)
	cmake_pop_check_state()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PulseAudio
	DEFAULT_MSG
	PULSEAUDIO_INCLUDE_DIRS PULSEAUDIO_LIBRARIES HAVE_PULSE_PULSEAUDIO_H HAVE_PA_MAINLOOP_NEW
)

mark_as_advanced(PULSEAUDIO_INCLUDE_DIRS PULSEAUDIO_LIBRARIES HAVE_PULSE_PULSEAUDIO_H HAVE_PA_MAINLOOP_NEW)
