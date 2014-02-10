############################################################################
# FindCUnit.txt
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
# - Find the CUnit include file and library
#
#  CUNIT_FOUND - system has CUnit
#  CUNIT_INCLUDE_DIRS - the CUnit include directory
#  CUNIT_LIBRARIES - The libraries needed to use CUnit

include(CheckIncludeFile)
include(CheckLibraryExists)

if(UNIX)
	find_package(PkgConfig)
	pkg_check_modules(_CUNIT QUIET cunit)
endif(UNIX)

set(_CUNIT_ROOT_PATHS
	${WITH_CUNIT}
	${CMAKE_INSTALL_PREFIX}
)

find_path(CUNIT_INCLUDE_DIR
	NAMES CUnit/CUnit.h
	HINTS _CUNIT_ROOT_PATHS
	PATH_SUFFIXES include
)

if(NOT "${CUNIT_INCLUDE_DIR}" STREQUAL "")
	set(HAVE_CUNIT_CUNIT_H 1)

	find_library(CUNIT_LIBRARIES
		NAMES cunit
		HINTS ${_CUNIT_ROOT_PATHS}
		PATH_SUFFIXES bin lib
	)

	if(NOT "${CUNIT_LIBRARIES}" STREQUAL "")
		set(CUNIT_FOUND TRUE)
	endif(NOT "${CUNIT_LIBRARIES}" STREQUAL "")
endif(NOT "${CUNIT_INCLUDE_DIR}" STREQUAL "")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CUnit
	REQUIRED_VARS
		CUNIT_LIBRARIES
		CUNIT_INCLUDE_DIR
	FAIL_MESSAGE
		"Could NOT find CUnit"
)

mark_as_advanced(CUNIT_INCLUDE_DIR CUNIT_LIBRARIES)
