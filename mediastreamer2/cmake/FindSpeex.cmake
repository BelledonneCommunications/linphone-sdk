############################################################################
# FindSpeex.txt
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################
#
# - Find the speex include file and library
#
#  SPEEX_FOUND - system has speex
#  SPEEX_INCLUDE_DIRS - the speex include directory
#  SPEEX_LIBRARIES - The libraries needed to use speex

set(_SPEEX_ROOT_PATHS
	${WITH_SPEEX}
	${CMAKE_INSTALL_PREFIX}
)

find_path(SPEEX_INCLUDE_DIRS
	NAMES speex/speex.h
	HINTS _SPEEX_ROOT_PATHS
	PATH_SUFFIXES include
)
if(SPEEX_INCLUDE_DIRS)
	set(HAVE_SPEEX_SPEEX_H 1)
endif()

find_library(SPEEX_LIBRARIES
	NAMES speex
	HINTS _SPEEX_ROOT_PATHS
	PATH_SUFFIXES bin lib
)
find_library(SPEEXDSP_LIBRARIES
	NAMES speexdsp
	HINTS _SPEEX_ROOT_PATHS
	PATH_SUFFIXES bin lib
)
list(APPEND SPEEX_LIBRARIES ${SPEEXDSP_LIBRARIES})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Speex
	DEFAULT_MSG
	SPEEX_INCLUDE_DIRS SPEEX_LIBRARIES SPEEXDSP_LIBRARIES
)

mark_as_advanced(SPEEX_INCLUDE_DIRS SPEEX_LIBRARIES)
