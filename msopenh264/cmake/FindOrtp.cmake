############################################################################
# FindOrtp.txt
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
# - Find the oRTP include file and library
#
#  ORTP_FOUND - system has oRTP
#  ORTP_INCLUDE_DIR - the oRTP include directory
#  ORTP_LIBRARIES - The libraries needed to use oRTP

if(UNIX)
	find_package(PkgConfig)
	pkg_check_modules(PC_ORTP QUIET ortp>=0.23.0)
endif()

set(_ORTP_ROOT_PATHS
	${WITH_ORTP}
	${CMAKE_INSTALL_PREFIX}
)

find_path(ORTP_INCLUDE_DIR
	NAMES ortp/ortp.h
	HINTS _ORTP_ROOT_PATHS
	PATH_SUFFIXES include
)

if(NOT "${ORTP_INCLUDE_DIR}" STREQUAL "")
	set(HAVE_ORTP_ORTP_H 1)

	find_library(ORTP_LIBRARIES
		NAMES ortp
		HINTS ${_ORTP_ROOT_PATHS}
		PATH_SUFFIXES bin lib
	)

	if(NOT "${ORTP_LIBRARIES}" STREQUAL "")
		set(ORTP_FOUND TRUE)
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ortp
	REQUIRED_VARS
		ORTP_LIBRARIES
		ORTP_INCLUDE_DIR
	FAIL_MESSAGE
		"Could NOT find oRTP"
)

mark_as_advanced(ORTP_INCLUDE_DIR ORTP_LIBRARIES)
