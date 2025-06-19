############################################################################
# FindXv.txt
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
#  This module defines the following variables:
#     XINERAMA_FOUND        - true if XINERAMA_INCLUDE_DIR & XINERAMA_LIBRARY are found
#     XINERAMA_LIBRARIES    - Set when XINERAMA_LIBRARY is found
#     XINERAMA_INCLUDE_DIRS - Set when XINERAMA_INCLUDE_DIR is found
#
#     XINERAMA_INCLUDE_DIR  - where to find Xinerama.h, etc.
#     XINERAMA_LIBRARY      - the XINERAMA library

find_path(XINERAMA_INCLUDE_DIR NAMES X11/extensions/Xinerama.h
          PATHS /opt/X11/include
          PATH_SUFFIXES X11/extensions
          DOC "The Xinerama include directory"
)

find_library(XINERAMA_LIBRARY NAMES Xinerama
          PATHS /opt/X11/lib
          DOC "The Xinerama library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Xinerama DEFAULT_MSG XINERAMA_LIBRARY XINERAMA_INCLUDE_DIR)

if(XINERAMA_FOUND)
  set( XINERAMA_LIBRARIES ${XINERAMA_LIBRARY} )
  set( XINERAMA_INCLUDE_DIRS ${XINERAMA_INCLUDE_DIR} )
endif()

mark_as_advanced(XINERAMA_INCLUDE_DIR XINERAMA_LIBRARY)
