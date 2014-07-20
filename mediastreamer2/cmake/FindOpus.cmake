############################################################################
# FindOpus.txt
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
# - Find the opus include file and library
#
#  OPUS_FOUND - system has opus
#  OPUS_INCLUDE_DIRS - the opus include directory
#  OPUS_LIBRARIES - The libraries needed to use opus

set(_OPUS_ROOT_PATHS
	${WITH_OPUS}
	${CMAKE_INSTALL_PREFIX}
)

find_path(OPUS_INCLUDE_DIRS
	NAMES opus/opus.h
	HINTS _OPUS_ROOT_PATHS
	PATH_SUFFIXES include
)
if(OPUS_INCLUDE_DIRS)
	set(HAVE_OPUS_OPUS_H 1)
endif()

find_library(OPUS_LIBRARIES
	NAMES opus
	HINTS _OPUS_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus
	DEFAULT_MSG
	OPUS_INCLUDE_DIRS OPUS_LIBRARIES
)

mark_as_advanced(OPUS_INCLUDE_DIRS OPUS_LIBRARIES)
