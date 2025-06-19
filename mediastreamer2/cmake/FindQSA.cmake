############################################################################
# FindQSA.cmake
# Copyright (C) 2014-2023  Belledonne Communications, Grenoble France
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
# Find the asound library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  qsa - If the asound library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  QSA_FOUND - The asound library has been found
#  QSA_TARGET - The name of the CMake target for the asound library

set(_QSA_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

find_path(_QSA_INCLUDE_DIRS
	NAMES sys/asoundlib.h
	HINTS ${_QSA_ROOT_PATHS}
	PATH_SUFFIXES include
)
find_library(_QSA_LIBRARY
	NAMES asound
	HINTS ${_QSA_ROOT_PATHS}
	PATH_SUFFIXES bin lib
)

if(_QSA_INCLUDE_DIRS AND _QSA_LIBRARY)
		add_library(qsa UNKNOWN IMPORTED)
		set_target_properties(qsa PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${_QSA_INCLUDE_DIRS}"
			IMPORTED_LOCATION "${_QSA_LIBRARY}"
		)

		set(QSA_TARGET qsa)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QSA REQUIRED_VARS QSA_TARGET)
mark_as_advanced(QSA_TARGET)
