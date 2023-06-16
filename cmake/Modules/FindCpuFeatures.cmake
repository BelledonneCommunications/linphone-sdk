############################################################################
# FindCpuFeatures.cmake
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
# Find the Android cpufeatures library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  cpufeatures - If the cpufeatures library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  CpuFeatures_FOUND - The cpufeatures library has been found
#  CpuFeatures_TARGET - The name of the CMake target for the cpufeatures library

if(TARGET cpufeatures)

	set(CpuFeatures_TARGET cpufeatures)

else()

	find_path(_CpuFeatures_INCLUDE_DIRS
		NAMES cpu-features.h
		PATH_SUFFIXES include
	)
	find_library(_CpuFeatures_LIBRARY NAMES cpufeatures)

	if(_CpuFeatures_INCLUDE_DIRS AND _CpuFeatures_LIBRARY)
			add_library(cpufeatures UNKNOWN IMPORTED)
			set_target_properties(cpufeatures PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_CpuFeatures_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_CpuFeatures_LIBRARY}"
			)
			set(CpuFeatures_TARGET cpufeatures)
	endif()

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CpuFeatures REQUIRED_VARS CpuFeatures_TARGET)
mark_as_advanced(CpuFeatures_TARGET)
