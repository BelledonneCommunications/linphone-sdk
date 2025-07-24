############################################################################
# FindRNNoise.txt
# Copyright (C) 2016-2025  Belledonne Communications, Grenoble France
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
# Find the rnnoise library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  rnnoise - If the rnnoise library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  RNNoise_FOUND - The rnnoise library has been found
#  RNNoise_TARGET - The name of the CMake target for the rnnoise library


include(FindPackageHandleStandardArgs)

set(_RNNoise_REQUIRED_VARS RNNoise_TARGET)
set(_RNNoise_CACHE_VARS ${_RNNoise_REQUIRED_VARS})

if(TARGET rnnoise)

	set(RNNoise_TARGET rnnoise)

else()

	find_path(_RNNoise_INCLUDE_DIRS
		NAMES rnnoise.h
		PATH_SUFFIXES include
	)

	find_library(_RNNoise_LIBRARY NAMES rnnoise)

	if(_RNNoise_INCLUDE_DIRS AND _RNNoise_LIBRARY)
		add_library(rnnoise UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(rnnoise PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_RNNoise_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_RNNoise_LIBRARY}"
			)
		else()
			set_target_properties(rnnoise PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_RNNoise_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_RNNoise_LIBRARY}"
			)
		endif()

		set(RNNoise_TARGET rnnoise)
	endif()
endif()

find_package_handle_standard_args(RNNoise
	REQUIRED_VARS ${_RNNoise_REQUIRED_VARS}
)
mark_as_advanced(${_RNNoise_CACHE_VARS})
