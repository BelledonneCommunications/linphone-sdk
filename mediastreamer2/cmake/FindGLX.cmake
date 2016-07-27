############################################################################
# FindGLX.txt
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
# - Find the GLX include file and library
#
#  GLX_FOUND - system has GLX
#  GLX_INCLUDE_DIRS - the GLX include directory
#  GLX_LIBRARIES - The libraries needed to use GLX

include(CheckIncludeFile)
include(CheckSymbolExists)

set(_GLX_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(GLX_INCLUDE_DIRS
	NAMES GL/gl.h
	HINTS _GLX_ROOT_PATHS
	PATH_SUFFIXES include
)
if(GLX_INCLUDE_DIRS)
	check_include_file(GL/gl.h HAVE_GL_GL_H)
	check_include_file(GL/glx.h HAVE_GL_GLX_H)
endif()

find_library(GL_LIBRARY
	NAMES GL
	HINTS _GLX_ROOT_PATHS
	PATH_SUFFIXES bin lib
)
find_library(GLEW_LIBRARY
	NAMES GLEW
	HINTS _GLX_ROOT_PATHS
	PATH_SUFFIXES bin lib
)
set(GLX_LIBRARIES ${GL_LIBRARY} ${GLEW_LIBRARY})

if(GLX_LIBRARIES)
	# TODO: check symbols in gl and glew
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLX
	DEFAULT_MSG
	GLX_INCLUDE_DIRS HAVE_GL_GL_H HAVE_GL_GLX_H GLX_LIBRARIES
)

mark_as_advanced(GLX_INCLUDE_DIRS HAVE_GL_GL_H HAVE_GL_GLX_H GLX_LIBRARIES)
