############################################################################
# FindDav1d.txt
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
# - Find the dav1d include file and library
#
#  DAV1D_FOUND - system has dav1d
#  DAV1D_INCLUDE_DIRS - the dav1d include directory
#  DAV1D_LIBRARIES - The libraries needed to use dav1d

if(TARGET dav1d)

	set(DAV1D_LIBRARIES libdav1d)
	get_target_property(DAV1D_INCLUDE_DIRS libdav1d INTERFACE_INCLUDE_DIRECTORIES)

else()

	set(_DAV1D_ROOT_PATHS
		${CMAKE_INSTALL_PREFIX}
	)

	find_path(DAV1D_INCLUDE_DIRS
		NAMES dav1d/dav1d.h
		HINTS _DAV1D_ROOT_PATHS
		PATH_SUFFIXES include
	)

	find_library(DAV1D_LIBRARIES
		NAMES dav1d
		HINTS _DAV1D_ROOT_PATHS
		PATH_SUFFIXES bin lib lib/Win32
	)

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Dav1d
	DEFAULT_MSG
	DAV1D_INCLUDE_DIRS DAV1D_LIBRARIES
)

mark_as_advanced(DAV1D_INCLUDE_DIRS DAV1D_LIBRARIES)
