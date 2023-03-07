############################################################################
# FindBcg729.cmake
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
# - Find the bcg729 include file and library
#
#  BCG729_FOUND - system has bcg729
#  BCG729_INCLUDE_DIRS - the bcg729 include directory
#  BCG729_LIBRARIES - The libraries needed to use bcg729

if(TARGET bcg729)

	set(BCG729_LIBRARIES bcg729)
	get_target_property(BCG729_INCLUDE_DIRS bcg729 INTERFACE_INCLUDE_DIRECTORIES)

	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(Bcg729
		DEFAULT_MSG
		BCG729_INCLUDE_DIRS BCG729_LIBRARIES
	)

	mark_as_advanced(BCG729_INCLUDE_DIRS BCG729_LIBRARIES)

endif()
