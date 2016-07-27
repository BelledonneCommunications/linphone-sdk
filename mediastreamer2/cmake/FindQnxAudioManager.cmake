############################################################################
# FindQnxAudioManager.txt
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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
# - Find the Qnx AudioManager include file and library
#
#  QNXAUDIOMANAGER_FOUND - system has QNX Audio Manager
#  QNXAUDIOMANAGER_INCLUDE_DIRS - the Audio Manager include directory
#  QNXAUDIOMANAGER_LIBRARIES - The libraries needed to use Audio Manager

include(CheckSymbolExists)
include(CMakePushCheckState)

set(_QNXAUDIOMANAGER_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(QNXAUDIOMANAGER_INCLUDE_DIRS
	NAMES audio/audio_manager_routing.h
	HINTS _QNXAUDIOMANAGER_ROOT_PATHS
	PATH_SUFFIXES include
)
if(QNXAUDIOMANAGER_INCLUDE_DIRS)
	set(HAVE_QNX_AUDIOMANAGER_H 1)
endif()

find_library(QNXAUDIOMANAGER_LIBRARIES
	NAMES audio_manager
	HINTS _QNXAUDIOMANAGER_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

if(QNXAUDIOMANAGER_LIBRARIES)
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${QNXAUDIOMANAGER_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${QNXAUDIOMANAGER_LIBRARIES})
	check_symbol_exists(audio_manager_snd_pcm_open_name "audio/audio_manager_routing.h" HAVE_AUDIO_MANAGER_SND_PCM_OPEN)
	cmake_pop_check_state()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QNXAUDIOMANAGER
	DEFAULT_MSG
	QNXAUDIOMANAGER_INCLUDE_DIRS QNXAUDIOMANAGER_LIBRARIES HAVE_QNX_AUDIOMANAGER_H HAVE_AUDIO_MANAGER_SND_PCM_OPEN
)

mark_as_advanced(QNXAUDIOMANAGER_INCLUDE_DIRS QNXAUDIOMANAGER_LIBRARIES HAVE_QNX_AUDIOMANAGER_H HAVE_AUDIO_MANAGER_SND_PCM_OPEN)
