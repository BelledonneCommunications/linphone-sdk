############################################################################
# FindBelr.cmake
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
# Find the belr library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  belr - If the belr library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Belr_FOUND - The belr library has been found
#  Belr_TARGET - The name of the CMake target for the belr library


set(_Belr_REQUIRED_VARS Belr_TARGET)
set(_Belr_CACHE_VARS ${_Belr_REQUIRED_VARS})

if(TARGET belr)
	set(Belr_TARGET belr)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Belr
	REQUIRED_VARS ${_Belr_REQUIRED_VARS}
	HANDLE_COMPONENTS
)
mark_as_advanced(${_Belr_CACHE_VARS})
