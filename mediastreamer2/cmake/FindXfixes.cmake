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
# - Find the Xfixes include file and library
#     XFIXES_FOUND        - 1 if XFIXES_INCLUDE_DIR & XFIXES_LIBRARY are found, 0 otherwise
#     XFIXES_INCLUDE_DIR  - where to find Xlib.h, etc.
#     XFIXES_LIBRARY      - the X11 library
#

find_path( XFIXES_INCLUDE_DIR
           NAMES X11/extensions/Xfixes.h
           PATH_SUFFIXES X11/extensions
           DOC "The XFixes include directory" )

find_library( XFIXES_LIBRARY
              NAMES Xfixes
              PATHS /usr/lib /lib
              DOC "The XFixes library" )

if( XFIXES_INCLUDE_DIR AND XFIXES_LIBRARY )
    set( XFIXES_FOUND 1 )
else()
    set( XFIXES_FOUND 0 )
endif()

mark_as_advanced( XFIXES_INCLUDE_DIR XFIXES_LIBRARY )
