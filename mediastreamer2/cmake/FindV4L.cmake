############################################################################
# FindV4L.txt
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
# - Find the V4L include file and library
#
#  V4L_FOUND - system has V4L
#  V4L_INCLUDE_DIRS - the V4L include directory
#  V4L_LIBRARIES - The libraries needed to use V4L

set(_V4L_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(V4L1_INCLUDE_DIRS
	NAMES linux/videodev.h
	HINTS _V4L_ROOT_PATHS
	PATH_SUFFIXES include
)
if(V4L1_INCLUDE_DIRS)
	set(HAVE_LINUX_VIDEODEV_H 1)
endif()
find_path(V4L2_INCLUDE_DIRS
	NAMES linux/videodev2.h
	HINTS _V4L_ROOT_PATHS
	PATH_SUFFIXES include
)
if(V4L2_INCLUDE_DIRS)
	set(HAVE_LINUX_VIDEODEV2_H 1)
endif()

find_library(V4L1_LIBRARIES
	NAMES v4l1
	HINTS _V4L_ROOT_PATHS
	PATH_SUFFIXES bin lib
)
find_library(V4L2_LIBRARIES
	NAMES v4l2
	HINTS _V4L_ROOT_PATHS
	PATH_SUFFIXES bin lib
)
find_library(V4LCONVERT_LIBRARIES
	NAMES v4lconvert
	HINTS _V4L_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

set(V4L_INCLUDE_DIRS )
if(V4L1_INCLUDE_DIRS)
	list(APPEND V4L_INCLUDE_DIRS ${V4L1_INCLUDE_DIRS})
endif()
if(V4L2_INCLUDE_DIRS)
	list(APPEND V4L_INCLUDE_DIRS ${V4L2_INCLUDE_DIRS})
endif()
set(V4L_LIBRARIES )
if(V4L1_LIBRARIES)
	list(APPEND V4L_LIBRARIES ${V4L1_LIBRARIES})
endif()
if(V4L2_LIBRARIES)
	list(APPEND V4L_LIBRARIES ${V4L2_LIBRARIES})
endif()
if(V4LCONVERT_LIBRARIES)
	list(APPEND V4L_LIBRARIES ${V4LCONVERT_LIBRARIES})
endif()
if(V4L_INCLUDE_DIRS)
	list(REMOVE_DUPLICATES V4L_INCLUDE_DIRS)
endif()
if(V4L_LIBRARIES)
	list(REMOVE_DUPLICATES V4L_LIBRARIES)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(V4L
	DEFAULT_MSG
	V4L_INCLUDE_DIRS V4L_LIBRARIES
)

mark_as_advanced(V4L_INCLUDE_DIRS V4L_LIBRARIES)
