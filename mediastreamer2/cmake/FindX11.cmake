############################################################################
# FindX11.txt
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
# - Find the X11 include file and library
#
#  X11_FOUND - system has X11
#  X11_INCLUDE_DIRS - the X11 include directory
#  X11_LIBRARIES - The libraries needed to use X11

set(_X11_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(X11_INCLUDE_DIRS
	NAMES X11/Xlib.h
	HINTS _X11_ROOT_PATHS
	PATH_SUFFIXES include
)
if(X11_INCLUDE_DIRS)
	set(HAVE_X11_XLIB_H 1)
endif()

find_library(X11_LIBRARIES
	NAMES X11
	HINTS _X11_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(X11
	DEFAULT_MSG
	X11_INCLUDE_DIRS X11_LIBRARIES HAVE_X11_XLIB_H
)

mark_as_advanced(X11_INCLUDE_DIRS X11_LIBRARIES HAVE_X11_XLIB_H)
