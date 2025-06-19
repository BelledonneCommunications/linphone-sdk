############################################################################
# FindPCAP.cmake
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
# Find the pcap library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  pcap - If the pcap library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  PCAP_FOUND - The pcap library has been found
#  PCAP_TARGET - The name of the CMake target for the pcap library

set(_PCAP_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

find_path(_PCAP_INCLUDE_DIRS
	NAMES pcap/pcap.h pcap.h
	HINTS ${_PCAP_ROOT_PATHS}
	PATH_SUFFIXES include
)
find_library(_PCAP_LIBRARY
	NAMES pcap
	HINTS ${_PCAP_ROOT_PATHS}
	PATH_SUFFIXES lib
)

if(_PCAP_INCLUDE_DIRS AND _PCAP_LIBRARY)
		add_library(pcap UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(pcap PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_PCAP_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_PCAP_LIBRARY}"
			)
		else()
			set_target_properties(pcap PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_PCAP_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_PCAP_LIBRARY}"
			)
		endif()

		set(PCAP_TARGET pcap)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCAP REQUIRED_VARS PCAP_TARGET)
mark_as_advanced(PCAP_TARGET)
