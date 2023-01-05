############################################################################
# FindAom.txt
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
# This file is part of mediastreamer2.
############################################################################
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#
############################################################################
#
# - Find the aom include file and library
#
#  AOM_FOUND - system has aom
#  AOM_INCLUDE_DIRS - the aom include directory
#  AOM_LIBRARIES - The libraries needed to use aom

if(TARGET aom)

	set(AOM_LIBRARIES aom)
	get_target_property(AOM_INCLUDE_DIRS aom INTERFACE_INCLUDE_DIRECTORIES)

else()

	set(_AOM_ROOT_PATHS
		${CMAKE_INSTALL_PREFIX}
	)

	find_path(AOM_INCLUDE_DIRS
		NAMES aom/aomcx.h
		HINTS _AOM_ROOT_PATHS
		PATH_SUFFIXES include
	)

	find_library(AOM_LIBRARIES
		NAMES aom
		HINTS _AOM_ROOT_PATHS
		PATH_SUFFIXES bin lib lib/Win32
	)

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Aom
	DEFAULT_MSG
	AOM_INCLUDE_DIRS AOM_LIBRARIES
)

mark_as_advanced(AOM_INCLUDE_DIRS AOM_LIBRARIES)
