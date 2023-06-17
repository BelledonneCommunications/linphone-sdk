############################################################################
# FindDecaf.cmake
# Copyright (C) 2017-2023  Belledonne Communications, Grenoble France
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
# Find the decaf library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  decaf - If the decaf library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Decaf_FOUND - The decaf library has been found
#  Decaf_TARGET - The name of the CMake target for the decaf library


if(TARGET decaf OR TARGET decaf-static)

	if(TARGET decaf-static)
		set(Decaf_TARGET decaf-static)
	elseif(TARGET decaf)
		set(Decaf_TARGET decaf)
	endif()

	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(Decaf
		DEFAULT_MSG
		Decaf_TARGET
	)
	mark_as_advanced(Decaf_TARGET)

else()

	set(_OPTIONS CONFIG)
	if(Decaf_FIND_REQUIRED)
		list(APPEND _OPTIONS REQUIRED)
	endif()
	if(Decaf_FIND_QUIETLY)
		list(APPEND _OPTIONS QUIET)
	endif()
	if(Decaf_FIND_VERSION)
		list(PREPEND _OPTIONS "${Decaf_FIND_VERSION}")
	endif()
	if(Decaf_FIND_EXACT)
		list(APPEND _OPTIONS EXACT)
	endif()

	find_package(Decaf ${_OPTIONS})

	if(TARGET decaf-static)
		set(Decaf_TARGET decaf-static)
	elseif(TARGET decaf)
		set(Decaf_TARGET decaf)
	endif()

endif()
