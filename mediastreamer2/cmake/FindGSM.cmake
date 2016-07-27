############################################################################
# FindGSM.txt
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
# - Find the gsm include file and library
#
#  GSM_FOUND - system has gsm
#  GSM_INCLUDE_DIRS - the gsm include directory
#  GSM_LIBRARIES - The libraries needed to use gsm

set(_GSM_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(GSM_INCLUDE_DIRS
	NAMES gsm/gsm.h
	HINTS _GSM_ROOT_PATHS
	PATH_SUFFIXES include
)
if(GSM_INCLUDE_DIRS)
	set(HAVE_GSM_GSM_H 1)
endif()

find_library(GSM_LIBRARIES
	NAMES gsm
	HINTS _GSM_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GSM
	DEFAULT_MSG
	GSM_INCLUDE_DIRS GSM_LIBRARIES HAVE_GSM_GSM_H
)

mark_as_advanced(GSM_INCLUDE_DIRS GSM_LIBRARIES HAVE_GSM_GSM_H)
