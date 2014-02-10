############################################################################
# FindTunnel.txt
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
# - Find the tunnel include file and library
#
#  TUNNEL_FOUND - system has tunnel
#  TUNNEL_INCLUDE_DIRS - the tunnel include directory
#  TUNNEL_LIBRARIES - The libraries needed to use tunnel

if(UNIX)
	find_package(PkgConfig)
	pkg_check_modules(_TUNNEL QUIET tunnel)
endif(UNIX)

if(WIN32)
	set(_TUNNEL_ROOT_PATHS "${CMAKE_INSTALL_PREFIX}")
endif(WIN32)

find_path(TUNNEL_INCLUDE_DIR
	NAMES tunnel/common.hh
	HINTS _TUNNEL_ROOT_PATHS
	PATH_SUFFIXES include
)

if(WIN32)
	find_library(TUNNEL_LIBRARIES
		NAMES tunnel
		HINTS ${_TUNNEL_ROOT_PATHS}
		PATH_SUFFIXES bin lib
	)
else(WIN32)
	find_library(TUNNEL_LIBRARIES
		NAMES tunnel
		HINTS ${_TUNNEL_LIBDIR}
		PATH_SUFFIXES lib
	)
endif(WIN32)

if(TUNNEL_INCLUDE_DIR AND TUNNEL_LIBRARIES)
	include(CheckIncludeFile)
	check_include_file("tunnel/common.hh" HAVE_TUNNEL_COMMON_HH)
endif(TUNNEL_INCLUDE_DIR AND TUNNEL_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(tunnel
	REQUIRED_VARS
		TUNNEL_LIBRARIES
		TUNNEL_INCLUDE_DIR
	FAIL_MESSAGE
		"Could NOT find tunnel"
)

mark_as_advanced(TUNNEL_INCLUDE_DIR TUNNEL_LIBRARIES)
