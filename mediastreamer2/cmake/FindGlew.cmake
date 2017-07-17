############################################################################
# FindGLEW.txt
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
#  GLEW_FOUND - system has glew
#  GLEW_INCLUDE_DIRS - the glew include directory 
#  GLEW_LIBRARIES - The libraries needed to use glew

find_path(GLEW_INCLUDE_DIRS
	NAMES GL/glew.h
	PATH_SUFFIXES include
)

if(GLEW_INCLUDE_DIRS)
        set(HAVE_GL_GLEW_H 1)
endif()

find_library(GLEW_LIBRARIES
	NAMES GLEW
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Glew
	DEFAULT_MSG
	GLEW_INCLUDE_DIRS GLEW_LIBRARIES HAVE_GL_GLEW_H
)

mark_as_advanced(GLEW_INCLUDE_DIRS GLEW_LIBRARIES HAVE_GL_GLEW_H)
