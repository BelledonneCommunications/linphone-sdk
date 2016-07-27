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
# - Find the libcamapi include file and library
#
#  CAMAPI_FOUND - system has libcamapi
#  CAMAPI_INCLUDE_DIRS - the libcamapi include directory
#  CAMAPI_LIBRARIES - The libraries needed to use libcamapi

include(CheckSymbolExists)
include(CMakePushCheckState)

set(_CAMAPI_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(CAMAPI_INCLUDE_DIRS
	NAMES camera/camera_api.h
	HINTS _CAMAPI_ROOT_PATHS
	PATH_SUFFIXES include
)

find_library(CAMAPI_LIBRARIES
	NAMES camapi
	HINTS _CAMAPI_ROOT_PATHS
	PATH_SUFFIXES lib
)

if(CAMAPI_LIBRARIES)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${CAMAPI_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${CAMAPI_LIBRARIES})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CAMAPI
	DEFAULT_MSG
	CAMAPI_INCLUDE_DIRS CAMAPI_LIBRARIES
)

mark_as_advanced(CAMAPI_INCLUDE_DIRS CAMAPI_LIBRARIES)
