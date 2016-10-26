############################################################################
# FindTheora.txt
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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
# - Find the theora include file and library
#
#  THEORA_FOUND - system has theora
#  THEORA_INCLUDE_DIRS - the theora include directory
#  THEORA_LIBRARIES - The libraries needed to use theora

find_path(THEORA_INCLUDE_DIRS
	NAMES theora/theora.h
	PATH_SUFFIXES include
)
if(THEORA_INCLUDE_DIRS)
	set(HAVE_THEORA_THEORA_H 1)
endif()

find_library(THEORA_LIBRARIES
	NAMES theora
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Theora
	DEFAULT_MSG
	THEORA_INCLUDE_DIRS THEORA_LIBRARIES HAVE_THEORA_THEORA_H
)

mark_as_advanced(THEORA_INCLUDE_DIRS THEORA_LIBRARIES HAVE_THEORA_THEORA_H)
