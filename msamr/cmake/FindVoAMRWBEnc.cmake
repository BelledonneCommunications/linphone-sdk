############################################################################
# FindVoAMRWBEnc.cmake
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
# - Find the vo-amrwbenc include file and library
#
#  VOAMRWBENC_FOUND - system has vo-amrwbenc
#  VOAMRWBENC_INCLUDE_DIRS - the vo-amrwbenc include directory
#  VOAMRWBENC_LIBRARIES - The libraries needed to use vo-amrwbenc

set(_VOAMRWBENC_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(VOAMRWBENC_INCLUDE_DIRS
	NAMES vo-amrwbenc/enc_if.h
	HINTS _VOAMRWBENC_ROOT_PATHS
	PATH_SUFFIXES include
)
if(VOAMRWBENC_INCLUDE_DIRS)
	set(HAVE_VOAMRWBENC_ENC_IF_H 1)
endif()

find_library(VOAMRWBENC_LIBRARIES
	NAMES vo-amrwbenc
	HINTS _VOAMRWBENC_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VoAMRWBEnc
	DEFAULT_MSG
	VOAMRWBENC_INCLUDE_DIRS VOAMRWBENC_LIBRARIES HAVE_VOAMRWBENC_ENC_IF_H
)

mark_as_advanced(VOAMRWBENC_INCLUDE_DIRS VOAMRWBENC_LIBRARIES HAVE_VOAMRWBENC_ENC_IF_H)
