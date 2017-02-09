############################################################################
# FindQtAngle.txt
# Copyright (C) 2017  Belledonne Communications, Grenoble France
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
# - Find the QtAngle include file and library
#
#  QTANGLE_FOUND - system has QtAngle
#  QTANGLE_INCLUDE_DIRS - the QtAngle include directory

find_path(QTANGLE_INCLUDE_DIRS
	NAMES QtANGLE/GLES3/gl3.h
	PATH_SUFFIXES include
)
if(QTANGLE_INCLUDE_DIRS)
	list(APPEND QTANGLE_INCLUDE_DIRS "${QTANGLE_INCLUDE_DIRS}/QtANGLE")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QTANGLE
	DEFAULT_MSG
	QTANGLE_INCLUDE_DIRS
)

mark_as_advanced(QTANGLE_INCLUDE_DIRS)
