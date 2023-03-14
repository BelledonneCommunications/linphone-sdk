############################################################################
# FindLibOqs.cmake
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
# - Find the liboqs include files and library
#
#  LIBOQS_FOUND - system has lib liboqs
#  LIBOQS_INCLUDE_DIRS - the liboqs include directory
#  LIBOQS_LIBRARIES - The library needed to use liboqs

if(TARGET oqs)

	set(LIBOQS_LIBRARIES oqs)
	get_target_property(LIBOQS_INCLUDE_DIRS oqs INTERFACE_INCLUDE_DIRECTORIES)
	set(LIBOQS_USE_BUILD_INTERFACE ON)


	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(LibOqs
		DEFAULT_MSG
		LIBOQS_INCLUDE_DIRS LIBOQS_LIBRARIES
	)

	mark_as_advanced(LIBOQS_INCLUDE_DIRS LIBOQS_LIBRARIES)

endif()
