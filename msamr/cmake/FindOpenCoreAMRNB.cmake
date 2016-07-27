############################################################################
# FindOpenCoreAMRNB.cmake
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
# - Find the opencoreamrnb include file and library
#
#  OPENCOREAMRNB_FOUND - system has opencoreamrnb
#  OPENCOREAMRNB_INCLUDE_DIRS - the opencoreamrnb include directory
#  OPENCOREAMRNB_LIBRARIES - The libraries needed to use opencoreamrnb

set(_OPENCOREAMRNB_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(OPENCOREAMRNB_INCLUDE_DIRS
	NAMES opencore-amrnb/interf_dec.h
	HINTS _OPENCOREAMRNB_ROOT_PATHS
	PATH_SUFFIXES include
)
if(OPENCOREAMRNB_INCLUDE_DIRS)
	set(HAVE_OPENCOREAMRNB_INTERF_DEC_H 1)
endif()

find_library(OPENCOREAMRNB_LIBRARIES
	NAMES opencore-amrnb
	HINTS _OPENCOREAMRNB_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCoreAMRNB
	DEFAULT_MSG
	OPENCOREAMRNB_INCLUDE_DIRS OPENCOREAMRNB_LIBRARIES HAVE_OPENCOREAMRNB_INTERF_DEC_H
)

mark_as_advanced(OPENCOREAMRNB_INCLUDE_DIRS OPENCOREAMRNB_LIBRARIES HAVE_OPENCOREAMRNB_INTERF_DEC_H)
