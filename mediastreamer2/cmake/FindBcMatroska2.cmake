############################################################################
# FindBcMatroska2.cmake
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
# - Find the bcmatroska2 include files and library
#
#  BCMATROSKA2_FOUND - system has lib bcmatroska2
#  BCMATROSKA2_INCLUDE_DIRS - the bcmatroska2 include directory
#  BCMATROSKA2_LIBRARIES - The library needed to use bcmatroska2

if(TARGET bcmatroska2)

	set(BCMATROSKA2_LIBRARIES bcmatroska2)
	get_target_property(BCMATROSKA2_INCLUDE_DIRS bcmatroska2 INTERFACE_INCLUDE_DIRECTORIES)


	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(BcMatroska2
		DEFAULT_MSG
		BCMATROSKA2_INCLUDE_DIRS BCMATROSKA2_LIBRARIES
	)

	mark_as_advanced(BCMATROSKA2_INCLUDE_DIRS BCMATROSKA2_LIBRARIES)

endif()
