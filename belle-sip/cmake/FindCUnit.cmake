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

if(UNIX)
	find_package(PkgConfig)
	pkg_check_modules(_CUNIT QUIET cunit)
endif(UNIX)

if(WIN32)
	set(_CUNIT_ROOT_PATHS "${CMAKE_INSTALL_PREFIX}")
endif(WIN32)

find_path(CUNIT_INCLUDE_DIR
	NAMES CUnit/CUnit.h
	HINTS _CUNIT_ROOT_PATHS
	PATH_SUFFIXES include
)

if(WIN32)
	find_library(CUNIT_LIBRARIES
		NAMES cunit
		HINTS ${_CUNIT_ROOT_PATHS}
		PATH_SUFFIXES bin lib
	)
else(WIN32)
	find_library(CUNIT_LIBRARIES
		NAMES cunit
		HINTS ${_CUNIT_LIBDIR}
		PATH_SUFFIXES lib
	)
endif(WIN32)

if(CUNIT_INCLUDE_DIR AND CUNIT_LIBRARIES)
	include(CheckIncludeFile)
	include(CheckLibraryExists)
	check_include_file("CUnit/CUnit.h" HAVE_CUNIT_CUNIT_H)
	check_library_exists("cunit" "CU_add_suite" "" HAVE_CU_ADD_SUITE)
endif(CUNIT_INCLUDE_DIR AND CUNIT_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CUnit
	REQUIRED_VARS
		CUNIT_LIBRARIES
		CUNIT_INCLUDE_DIR
	FAIL_MESSAGE
		"Could NOT find CUnit"
)

mark_as_advanced(CUNIT_INCLUDE_DIR CUNIT_LIBRARIES)
