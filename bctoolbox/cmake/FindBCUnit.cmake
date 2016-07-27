############################################################################
# FindBCUnit.txt
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################
#
# - Find the BCUnit include file and library
#
#  BCUNIT_FOUND - system has BCUnit
#  BCUNIT_INCLUDE_DIRS - the BCUnit include directory
#  BCUNIT_LIBRARIES - The libraries needed to use BCUnit

include(CheckIncludeFile)
include(CheckLibraryExists)

find_path(BCUNIT_INCLUDE_DIRS
	NAMES BCUnit/BCUnit.h
	PATH_SUFFIXES include
)

if(BCUNIT_INCLUDE_DIRS)
	set(HAVE_BCUNIT_BCUNIT_H 1)
endif()

if(HAVE_BCUNIT_BCUNIT_H)
	if(BCUnit_FIND_VERSION)
		list(APPEND BCUNIT_REQUIRED_VARS BCUNIT_VERSION)
		file(STRINGS "${BCUNIT_INCLUDE_DIRS}/BCUnit/BCUnit.h" BCUNIT_VERSION_STR
			REGEX "^#define[\t ]+CU_VERSION[\t ]+\"([0-9.]+).*\"$")
		string(REGEX REPLACE "^.*CU_VERSION[\t ]+\"([0-9.]+).*\"$" "\\1" BCUNIT_VERSION "${BCUNIT_VERSION_STR}")
	endif()
endif()

find_library(BCUNIT_LIBRARIES
	NAMES bcunit
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)

if(BCUnit_FIND_VERSION)
	set(CHECK_VERSION_ARGS VERSION_VAR BCUNIT_VERSION)
endif()

find_package_handle_standard_args(BCUnit
	REQUIRED_VARS BCUNIT_INCLUDE_DIRS BCUNIT_LIBRARIES
	${CHECK_VERSION_ARGS}
)

mark_as_advanced(BCUNIT_INCLUDE_DIRS BCUNIT_LIBRARIES)

unset(CHECK_VERSION_ARGS)
