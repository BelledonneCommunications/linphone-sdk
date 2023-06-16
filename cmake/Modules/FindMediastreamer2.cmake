############################################################################
# FindMediastreamer2.cmake
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
# Find the mediastreamer2 library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  mediastreamer2 - If the mediastreamer2 library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Mediastreamer2_FOUND - The mediastreamer2 library has been found
#  Mediastreamer2_TARGET - The name of the CMake target for the mediastreamer2 library
#  Mediastreamer2_PLUGINS_DIR - The directory where to install mediastreamer2 plugins


set(_Mediastreamer2_REQUIRED_VARS Mediastreamer2_TARGET Mediastreamer2_PLUGINS_DIR)
set(_Mediastreamer2_CACHE_VARS ${_Mediastreamer2_REQUIRED_VARS})

if(TARGET mediastreamer2)
	set(Mediastreamer2_TARGET mediastreamer2)
	get_target_property(Mediastreamer2_PLUGINS_DIR ${Mediastreamer2_TARGET} MS2_PLUGINS_DIR)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Mediastreamer2
	REQUIRED_VARS ${_Mediastreamer2_REQUIRED_VARS}
	HANDLE_COMPONENTS
)
mark_as_advanced(${_Mediastreamer2_CACHE_VARS})
