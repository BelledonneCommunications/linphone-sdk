############################################################################
# FindQSA.txt
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
# - Find the QSA include file and library
#
#  QSA_FOUND - system has QSA
#  QSA_INCLUDE_DIRS - the QSA include directory
#  QSA_LIBRARIES - The libraries needed to use QSA

include(CheckSymbolExists)
include(CMakePushCheckState)

set(_QSA_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(QSA_INCLUDE_DIRS
	NAMES sys/asoundlib.h
	HINTS _QSA_ROOT_PATHS
	PATH_SUFFIXES include
)
if(QSA_INCLUDE_DIRS)
	set(HAVE_SYS_ASOUNDLIB_H 1)
endif()

find_library(QSA_LIBRARIES
	NAMES asound
	HINTS _QSA_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

if(QSA_LIBRARIES)
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${QSA_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${QSA_LIBRARIES})
	check_symbol_exists(snd_pcm_open "sys/asoundlib.h" HAVE_SND_PCM_OPEN)
	cmake_pop_check_state()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QSA
	DEFAULT_MSG
	QSA_INCLUDE_DIRS QSA_LIBRARIES HAVE_SYS_ASOUNDLIB_H HAVE_SND_PCM_OPEN
)

mark_as_advanced(QSA_INCLUDE_DIRS QSA_LIBRARIES HAVE_SYS_ASOUNDLIB_H HAVE_SND_PCM_OPEN)
