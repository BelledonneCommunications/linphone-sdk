############################################################################
# FindBCG729.cmake
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
# Find the bcg729 library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  bcg729 - If the bcg729 library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  BCG729_FOUND - The bcg729 library has been found
#  BCG729_TARGET - The name of the CMake target for the bcg729 library


if(TARGET bcg729)

	include(FindPackageHandleStandardArgs)
	set(BCG729_TARGET bcg729)
	set(_BCG729_REQUIRED_VARS BCG729_TARGET)
	set(_BCG729_CACHE_VARS ${_BCG729_REQUIRED_VARS})
	find_package_handle_standard_args(BCG729
		REQUIRED_VARS ${_BCG729_REQUIRED_VARS}
	)
	mark_as_advanced(${_BCG729_CACHE_VARS})

else()

	set(_OPTIONS CONFIG)
	if(BCG729_FIND_REQUIRED)
		list(APPEND _OPTIONS REQUIRED)
	endif()
	if(BCG729_FIND_QUIETLY)
		list(APPEND _OPTIONS QUIET)
	endif()
	if(BCG729_FIND_VERSION)
		list(PREPEND _OPTIONS "${BCG729_FIND_VERSION}")
	endif()
	if(BCG729_FIND_EXACT)
		list(APPEND _OPTIONS EXACT)
	endif()

	find_package(BCG729 ${_OPTIONS})

endif()
