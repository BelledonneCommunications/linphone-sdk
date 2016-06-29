############################################################################
# FindCUnit.txt
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
# - Find the CUnit include file and library
#
#  CUNIT_FOUND - system has CUnit
#  CUNIT_INCLUDE_DIRS - the CUnit include directory
#  CUNIT_LIBRARIES - The libraries needed to use CUnit

include(CheckIncludeFile)
include(CheckLibraryExists)

set(CUNIT_REQUIRED_VARS CUNIT_INCLUDE_DIRS HAVE_CUNIT_CUNIT_H CUNIT_LIBRARIES)

find_path(CUNIT_INCLUDE_DIRS
	NAMES CUnit/CUnit.h
	PATH_SUFFIXES include
)

if(CUNIT_INCLUDE_DIRS)
	set(HAVE_CUNIT_CUNIT_H 1)
endif()

if(HAVE_CUNIT_CUNIT_H)
	file(STRINGS "${CUNIT_INCLUDE_DIRS}/CUnit/CUnit.h" CUNIT_VERSION_STR
		REGEX "^#define[\t ]+CU_VERSION[\t ]+\"([0-9.]+).*\"$")
	string(REGEX REPLACE "^.*CU_VERSION[\t ]+\"([0-9.]+).*\"$" "\\1" CUNIT_VERSION "${CUNIT_VERSION_STR}")
	if(CUNIT_VERSION VERSION_LESS "3.0")
		# Version too low to activate automated junit output
		set(HAVE_CU_FIXED_JUNIT FALSE)
	else()
		set(HAVE_CU_FIXED_JUNIT TRUE)
		list(APPEND CUNIT_REQUIRED_VARS HAVE_CU_FIXED_JUNIT)
	endif()
endif()

find_library(CUNIT_LIBRARIES
	NAMES cunit
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
include(FindPackageMessage)

find_package_handle_standard_args(CUnit
	REQUIRED_VARS ${CUNIT_REQUIRED_VARS}
)

if(HAVE_CUNIT_CUNIT_H)
	find_package_message(CUnit "Found fixed junit: ${HAVE_CU_FIXED_JUNIT}" "")
endif()

mark_as_advanced(${CUNIT_REQUIRED_VARS})

unset(CUNIT_REQUIRED_VARS)
