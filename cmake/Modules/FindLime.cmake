############################################################################
# FindLime.cmake
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
# Find the lime library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  lime - If the lime library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Lime_FOUND - The lime library has been found
#  Lime_TARGET - The name of the CMake target for the lime library


set(_Lime_REQUIRED_VARS Lime_TARGET)
set(_Lime_CACHE_VARS ${_Lime_REQUIRED_VARS})

if(TARGET lime)
	set(Lime_TARGET lime)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Lime
	REQUIRED_VARS ${_Lime_REQUIRED_VARS}
	HANDLE_COMPONENTS
)
mark_as_advanced(${_Lime_CACHE_VARS})
