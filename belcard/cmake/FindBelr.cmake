############################################################################
# FindBelr.cmake
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################
#
# - Find the belr include file and library
#
#  BELR_FOUND - system has belr
#  BELR_INCLUDE_DIRS - the belr include directory
#  BELR_LIBRARIES - The libraries needed to use belr

set(_BELR_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(BELR_INCLUDE_DIRS
	NAMES belr/belr.hh
	HINTS _BELR_ROOT_PATHS
	PATH_SUFFIXES include
)

if(BELR_INCLUDE_DIRS)
	set(HAVE_BELR_H 1)
endif()

find_library(BELR_LIBRARIES
	NAMES belr
	HINTS ${_BELR_ROOT_PATHS}
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Belr
	DEFAULT_MSG
	BELR_INCLUDE_DIRS BELR_LIBRARIES HAVE_BELR_H
)

mark_as_advanced(BELR_INCLUDE_DIRS BELR_LIBRARIES HAVE_BELR_H)
