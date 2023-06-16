############################################################################
# FindBelCard.cmake
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
# Find the belcard library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  belcard - If the belcard library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  BelCard_FOUND - The belcard library has been found
#  BelCard_TARGET - The name of the CMake target for the belcard library


set(_BelCard_REQUIRED_VARS BelCard_TARGET)
set(_BelCard_CACHE_VARS ${_BelCard_REQUIRED_VARS})

if(TARGET belcard)
	set(BelCard_TARGET belcard)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BelCard
	REQUIRED_VARS ${_BelCard_REQUIRED_VARS}
	HANDLE_COMPONENTS
)
mark_as_advanced(${_BelCard_CACHE_VARS})
