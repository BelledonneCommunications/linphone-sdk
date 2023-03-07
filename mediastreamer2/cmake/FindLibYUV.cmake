############################################################################
# FindLibYUV.cmake
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
# This file is part of mediastreamer2.
############################################################################
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#
############################################################################
#
# - Find the LibYUV include file and library
#
#  LIBYUV_FOUND - system has LibYUV
#  LIBYUV_INCLUDE_DIRS - the LibYUV include directory
#  LIBYUV_LIBRARIES - The libraries needed to use LibYUV

if(TARGET yuv)

	set(LIBYUV_LIBRARIES yuv)
	get_target_property(LIBYUV_INCLUDE_DIRS yuv INTERFACE_INCLUDE_DIRECTORIES)
	set(HAVE_LIBYUV_H 1)

else()

	set(_LIBYUV_ROOT_PATHS
		${CMAKE_INSTALL_PREFIX}
	)

	find_path(LIBYUV_INCLUDE_DIRS
		NAMES libyuv.h
		HINTS _LIBYUV_ROOT_PATHS
		PATH_SUFFIXES include
	)
	if(LIBYUV_INCLUDE_DIRS)
		set(HAVE_LIBYUV_H 1)
	endif()

	find_library(LIBYUV_LIBRARIES
		NAMES yuv
		HINTS _LIBYUV_ROOT_PATHS
		PATH_SUFFIXES bin lib lib/Win32
	)

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibYUV
	DEFAULT_MSG
	LIBYUV_INCLUDE_DIRS LIBYUV_LIBRARIES HAVE_LIBYUV_H
)

mark_as_advanced(LIBYUV_INCLUDE_DIRS LIBYUV_LIBRARIES HAVE_LIBYUV_H)
