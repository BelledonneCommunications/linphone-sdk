############################################################################
# FindGL.txt
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
# - Find the GL include file and library
#
#  GL_FOUND - system has GL
#  GL_INCLUDE_DIRS - the GL include directory
#  GL_LIBRARIES - The libraries needed to use GL

include(CheckIncludeFile)
include(CheckSymbolExists)

find_path(GL_INCLUDE_DIRS
	NAMES GL/gl.h
	PATH_SUFFIXES include
)
if(GL_INCLUDE_DIRS)
	if(WIN32)
		check_include_file(GL/gl.h HAVE_GL_GL_H "-include windows.h")
	else()
		check_include_file(GL/gl.h HAVE_GL_GL_H)
	endif()
endif()

find_library(GL_LIBRARY
	NAMES GL
	PATH_SUFFIXES bin lib
)

if(GL_LIBRARIES)
	# TODO: check symbols in gl
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GL
	DEFAULT_MSG
	GL_INCLUDE_DIRS HAVE_GL_GL_H GL_LIBRARIES
)

mark_as_advanced(GL_INCLUDE_DIRS HAVE_GL_GL_H GL_LIBRARIES)
