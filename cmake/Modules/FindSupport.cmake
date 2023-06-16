############################################################################
# FindSupport.cmake
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
# Find the Android support library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  support - If the support library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Support_FOUND - The support library has been found
#  Support_TARGET - The name of the CMake target for the support library

if(TARGET support)

	set(Support_TARGET support)

else()

	find_library(_Support_LIBRARY NAMES support)

	if(_Support_LIBRARY)
			add_library(support UNKNOWN IMPORTED)
			set_target_properties(support PROPERTIES
				IMPORTED_LOCATION "${_Support_LIBRARY}"
			)
			set(Support_TARGET support)
	endif()

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Support REQUIRED_VARS Support_TARGET)
mark_as_advanced(Support_TARGET)
