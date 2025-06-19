############################################################################
# FindBCMatroska2.cmake
# Copyright (C) 2023  Belledonne Communications, Grenoble France
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
# Find the bcmatroska2 library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  bcmatroska2 - If the bcmatroska2 library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  BCMatroska2_FOUND - The bcmatroska2 library has been found
#  BCMatroska2_TARGET - The name of the CMake target for the bcmatroska2 library


if(TARGET bcmatroska2)

	include(FindPackageHandleStandardArgs)
	set(BCMatroska2_TARGET bcmatroska2)
	set(_BCMatroska2_REQUIRED_VARS BCMatroska2_TARGET)
	set(_BCMatroska2_CACHE_VARS ${_BCMatroska2_REQUIRED_VARS})
	find_package_handle_standard_args(BCMatroska2
		REQUIRED_VARS ${_BCMatroska2_REQUIRED_VARS}
	)
	mark_as_advanced(${_BCMatroska2_CACHE_VARS})

else()

	set(_OPTIONS CONFIG)
	if(BCMatroska2_FIND_REQUIRED)
		list(APPEND _OPTIONS REQUIRED)
	endif()
	if(BCMatroska2_FIND_QUIETLY)
		list(APPEND _OPTIONS QUIET)
	endif()
	if(BCMatroska2_FIND_VERSION)
		list(PREPEND _OPTIONS "${BCMatroska2_FIND_VERSION}")
	endif()
	if(BCMatroska2_FIND_EXACT)
		list(APPEND _OPTIONS EXACT)
	endif()

	find_package(BCMatroska2 ${_OPTIONS})

endif()
