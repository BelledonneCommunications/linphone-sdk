############################################################################
# FindPortAudio.txt
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
# - Find the portaudio include file and library
#
#  PORTAUDIO_FOUND - system has portaudio
#  PORTAUDIO_INCLUDE_DIRS - the portaudio include directory
#  PORTAUDIO_LIBRARIES - The libraries needed to use portaudio

include(CheckSymbolExists)
include(CMakePushCheckState)

set(_PORTAUDIO_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(PORTAUDIO_INCLUDE_DIRS
	NAMES portaudio.h
	HINTS _PORTAUDIO_ROOT_PATHS
	PATH_SUFFIXES include
)
if(PORTAUDIO_INCLUDE_DIRS)
	set(HAVE_PORTAUDIO_H 1)
endif()

find_library(PORTAUDIO_LIBRARIES
	NAMES portaudio
	HINTS _PORTAUDIO_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

if(PORTAUDIO_LIBRARIES)
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${PORTAUDIO_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${PORTAUDIO_LIBRARIES})
	check_symbol_exists(Pa_Initialize portaudio.h HAVE_PA_INITIALIZE)
	cmake_pop_check_state()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PortAudio
	DEFAULT_MSG
	PORTAUDIO_INCLUDE_DIRS PORTAUDIO_LIBRARIES HAVE_PORTAUDIO_H HAVE_PA_INITIALIZE
)

mark_as_advanced(PORTAUDIO_INCLUDE_DIRS PORTAUDIO_LIBRARIES HAVE_PORTAUDIO_H HAVE_PA_INITIALIZE)
