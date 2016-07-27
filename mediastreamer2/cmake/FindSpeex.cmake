############################################################################
# FindSpeex.txt
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
# - Find the speex include file and library
#
#  SPEEX_FOUND - system has speex
#  SPEEX_INCLUDE_DIRS - the speex include directory
#  SPEEX_LIBRARIES - The libraries needed to use speex

find_path(SPEEX_INCLUDE_DIRS
	NAMES speex/speex.h
	PATH_SUFFIXES include
)
if(SPEEX_INCLUDE_DIRS)
	set(HAVE_SPEEX_SPEEX_H 1)
endif()

find_library(SPEEX_LIBRARIES
	NAMES speex
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Speex
	DEFAULT_MSG
	SPEEX_INCLUDE_DIRS SPEEX_LIBRARIES HAVE_SPEEX_SPEEX_H
)

mark_as_advanced(SPEEX_INCLUDE_DIRS SPEEX_LIBRARIES HAVE_SPEEX_SPEEX_H)
