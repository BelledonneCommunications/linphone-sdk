############################################################################
# FindSpeexDSP.cmake
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
# Find the speexdsp library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  speexdsp - If the speexdsp library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  SpeexDSP_FOUND - The speexdsp library has been found
#  SpeexDSP_TARGET - The name of the CMake target for the speexdsp library


include(FindPackageHandleStandardArgs)

set(_SpeexDSP_REQUIRED_VARS SpeexDSP_TARGET)
set(_SpeexDSP_CACHE_VARS ${_SpeexDSP_REQUIRED_VARS})

if(TARGET speexdsp)

	set(SpeexDSP_TARGET speexdsp)

else()

	find_path(_SpeexDSP_INCLUDE_DIRS
		NAMES speex/speex_resampler.h
		PATH_SUFFIXES include
	)

	find_library(_SpeexDSP_LIBRARY NAMES speexdsp)

	if(_SpeexDSP_INCLUDE_DIRS AND _SpeexDSP_LIBRARY)
		add_library(speexdsp UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(speexdsp PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_SpeexDSP_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_SpeexDSP_LIBRARY}"
			)
		else()
			set_target_properties(speexdsp PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_SpeexDSP_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_SpeexDSP_LIBRARY}"
			)
		endif()

		set(SpeexDSP_TARGET speexdsp)
	endif()

endif()

find_package_handle_standard_args(SpeexDSP
	REQUIRED_VARS ${_SpeexDSP_REQUIRED_VARS}
)
mark_as_advanced(${_SpeexDSP_CACHE_VARS})
