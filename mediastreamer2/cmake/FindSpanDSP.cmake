############################################################################
# FindSpanDSP.cmake
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
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
# Find the spandsp library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  spandsp - If the spandsp library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  SpanDSP_FOUND - The spandsp library has been found
#  SpanDSP_TARGET - The name of the CMake target for the spandsp library

find_path(_SpanDSP_INCLUDE_DIRS
	NAMES spandsp.h
	PATH_SUFFIXES include
)
find_library(_SpanDSP_LIBRARY
	NAMES spandsp
	PATH_SUFFIXES bin lib
)

if(_SpanDSP_INCLUDE_DIRS AND _SpanDSP_LIBRARY)
		add_library(spandsp UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(spandsp PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_SpanDSP_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_SpanDSP_LIBRARY}"
			)
		else()
			set_target_properties(spandsp PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_SpanDSP_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_SpanDSP_LIBRARY}"
			)
		endif()

		set(SpanDSP_TARGET spandsp)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SpanDSP REQUIRED_VARS SpanDSP_TARGET)
mark_as_advanced(SpanDSP_TARGET)
