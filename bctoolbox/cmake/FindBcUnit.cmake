############################################################################
# FindBcUnit.cmake
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
# - Find the bcunit include files and library
#
#  BCUNIT_FOUND - system has lib bcunit
#  BCUNIT_INCLUDE_DIRS - the bcunit include directory
#  BCUNIT_LIBRARIES - The library needed to use bcunit

if(TARGET bcunit)

	set(BCUNIT_LIBRARIES bcunit)
	get_target_property(BCUNIT_INCLUDE_DIRS bcunit INTERFACE_INCLUDE_DIRECTORIES)


	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(BcUnit
		DEFAULT_MSG
		BCUNIT_INCLUDE_DIRS BCUNIT_LIBRARIES
	)

	mark_as_advanced(BCUNIT_INCLUDE_DIRS BCUNIT_LIBRARIES)

endif()
