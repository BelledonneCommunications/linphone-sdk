############################################################################
# FindBroadVoice16.txt
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
# - Find the bv16 include file and library
#
#  BV16_FOUND - system has bv16
#  BV16_INCLUDE_DIRS - the bv16 include directory
#  BV16_LIBRARIES - The libraries needed to use bv16

find_path(BV16_INCLUDE_DIRS
	NAMES bv16-floatingpoint/bv16/bv16.h
	PATH_SUFFIXES include
)
if(BV16_INCLUDE_DIRS)
	set(HAVE_BV16_BV16_H 1)
endif()

find_library(BV16_LIBRARIES NAMES bv16)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(bv16
	DEFAULT_MSG
	BV16_INCLUDE_DIRS BV16_LIBRARIES HAVE_BV16_BV16_H
)

mark_as_advanced(BV16_INCLUDE_DIRS BV16_LIBRARIES HAVE_BV16_BV16_H)
