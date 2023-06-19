############################################################################
# FindSpeex.cmake
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
# Find the speex library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  speex - If the speex library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Speex_FOUND - The speex library has been found
#  Speex_TARGET - The name of the CMake target for the speex library


include(FindPackageHandleStandardArgs)

set(_Speex_REQUIRED_VARS Speex_TARGET)
set(_Speex_CACHE_VARS ${_Speex_REQUIRED_VARS})

if(TARGET speex)

	set(Speex_TARGET speex)

else()

	find_path(_Speex_INCLUDE_DIRS
		NAMES speex/speex.h
		PATH_SUFFIXES include
	)

	find_library(_Speex_LIBRARY NAMES speex)

	if(_Speex_INCLUDE_DIRS AND _Speex_LIBRARY)
		add_library(speex UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(speex PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Speex_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_Speex_LIBRARY}"
			)
		else()
			set_target_properties(speex PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Speex_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_Speex_LIBRARY}"
			)
		endif()

		set(Speex_TARGET speex)
	endif()

endif()

find_package_handle_standard_args(Speex
	REQUIRED_VARS ${_Speex_REQUIRED_VARS}
)
mark_as_advanced(${_Speex_CACHE_VARS})
