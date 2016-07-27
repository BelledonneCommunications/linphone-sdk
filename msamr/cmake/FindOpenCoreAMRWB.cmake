############################################################################
# FindOpenCoreAMRWB.cmake
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
# - Find the opencoreamrwb include file and library
#
#  OPENCOREAMRWB_FOUND - system has opencoreamrwb
#  OPENCOREAMRWB_INCLUDE_DIRS - the opencoreamrwb include directory
#  OPENCOREAMRWB_LIBRARIES - The libraries needed to use opencoreamrwb

set(_OPENCOREAMRWB_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(OPENCOREAMRWB_INCLUDE_DIRS
	NAMES opencore-amrwb/dec_if.h
	HINTS _OPENCOREAMRWB_ROOT_PATHS
	PATH_SUFFIXES include
)
if(OPENCOREAMRWB_INCLUDE_DIRS)
	set(HAVE_OPENCOREAMRWB_DEC_IF_H 1)
endif()

find_library(OPENCOREAMRWB_LIBRARIES
	NAMES opencore-amrwb
	HINTS _OPENCOREAMRWB_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCoreAMRWB
	DEFAULT_MSG
	OPENCOREAMRWB_INCLUDE_DIRS OPENCOREAMRWB_LIBRARIES HAVE_OPENCOREAMRWB_DEC_IF_H
)

mark_as_advanced(OPENCOREAMRWB_INCLUDE_DIRS OPENCOREAMRWB_LIBRARIES HAVE_OPENCOREAMRWB_DEC_IF_H)
