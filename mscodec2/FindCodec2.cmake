############################################################################
# FindCodec2.txt
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
# - Find the codec2 include file and library
#
#  CODEC2_FOUND - system has codec2
#  CODEC2_INCLUDE_DIRS - the codec2 include directory
#  CODEC2_LIBRARIES - The libraries needed to use codec2

include(CMakePushCheckState)

find_path(CODEC2_INCLUDE_DIRS
	NAMES codec2/codec2.h
	PATH_SUFFIXES include
)
if(CODEC2_INCLUDE_DIRS)
	set(HAVE_CODEC2_CODEC2_H 1)
endif()

find_library(CODEC2_LIBRARIES NAMES codec2)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Codec2
	DEFAULT_MSG
	CODEC2_INCLUDE_DIRS CODEC2_LIBRARIES HAVE_CODEC2_CODEC2_H
)

mark_as_advanced(CODEC2_INCLUDE_DIRS CODEC2_LIBRARIES HAVE_CODEC2_CODEC2_H)
