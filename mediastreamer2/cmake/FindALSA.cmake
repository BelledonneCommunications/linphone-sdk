############################################################################
# FindALSA.txt
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
# - Find the ALSA include file and library
#
#  ALSA_FOUND - system has ALSA
#  ALSA_INCLUDE_DIRS - the ALSA include directory
#  ALSA_LIBRARIES - The libraries needed to use ALSA

include(CheckSymbolExists)
include(CMakePushCheckState)

find_path(ALSA_INCLUDE_DIRS
	NAMES alsa/asoundlib.h
	PATH_SUFFIXES include
)
if(ALSA_INCLUDE_DIRS)
	set(HAVE_ALSA_ASOUNDLIB_H 1)
endif()

find_library(ALSA_LIBRARIES
	NAMES asound
	PATH_SUFFIXES lib
)

if(ALSA_LIBRARIES)
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${ALSA_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${ALSA_LIBRARIES})
	check_symbol_exists(snd_pcm_open "alsa/asoundlib.h" HAVE_SND_PCM_OPEN)
	cmake_pop_check_state()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ALSA
	DEFAULT_MSG
	ALSA_INCLUDE_DIRS ALSA_LIBRARIES HAVE_ALSA_ASOUNDLIB_H HAVE_SND_PCM_OPEN
)

mark_as_advanced(ALSA_INCLUDE_DIRS ALSA_LIBRARIES HAVE_ALSA_ASOUNDLIB_H HAVE_SND_PCM_OPEN)
