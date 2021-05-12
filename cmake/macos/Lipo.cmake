############################################################################
# Lipo.cmake
# Copyright (C) 2010-2018 Belledonne Communications, Grenoble France
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

list(APPEND CMAKE_MODULE_PATH "${LINPHONESDK_DIR}/cmake")
include(LinphoneSdkUtils)


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_MACOS_ARCHS}" _archs)


# Create the apple-darwin directory that will contain the merged content of all architectures
execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "linphone-sdk/apple-macos"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)

execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "linphone-sdk/apple-macos"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)


# Copy and merge content of all architectures in the apple-darwin directory
list(GET _archs 0 _first_arch)

# When using the generator Xcode (Xcode 10.3 and cmake 3.15) to build the SDK, it has too much processing on Info.plist :
#	 (for armv7 and x86_64) builtin-infoPlistUtility ... -expandbuildsettings -format binary -platform iphoneos
#  (for arm64) builtin-infoPlistUtility ... -expandbuildsettings -format binary -platform iphoneos -requiredArchitecture arm64
#
# This causes the Info.plist parameter UIRequiredDeviceCapabilities to be set to -requiredArchitecture for arm64.
# Lipo target select the first Info.plist with is most of the time arm64.
# Consequence is that even the multi arch framework will have Info.plist parameter UIRequiredDeviceCapabilities set to arm64.
# Last consequence is that Apple store will remove framework masked as arm64 from amv7 binaries leading to a crash at startup.
# Try to fix it in the future. Today, please use the Info.plist of armv7 whenever possible.
foreach(_arch ${_archs})
	if(_arch STREQUAL "armv7")
		set(_first_arch "armv7")
	endif()
endforeach()

execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "linphone-sdk/mac-${_first_arch}/" "linphone-sdk/apple-macos/"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)

function(merge_file _file_name _path _archs)
	set(_all_arch_files)
	foreach(_arch ${_archs})
		list(APPEND _all_arch_files "linphone-sdk/mac-${_arch}/${_path}/${_file_name}")
	endforeach()
	string(REPLACE ";" " " _arch_string "${_archs}")
	get_filename_component(_extension "${_file_name}" LAST_EXT)
	message (STATUS "Mixing ${_file_name} for archs [${_arch_string}]")
	if(NOT ${_file_name} MATCHES "(cmake|pkgconfig|objects-${CMAKE_BUILD_TYPE})" AND NOT _extension MATCHES "la")
		execute_process(
			COMMAND "lipo" "-create" "-output" "linphone-sdk/apple-macos/${_path}/${_file_name}" ${_all_arch_files}
			WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
		)
	endif()
endfunction()

file(GLOB _frameworks "linphone-sdk/mac-${_first_arch}/Frameworks/*.framework")
foreach(_framework ${_frameworks})
	get_filename_component(_framework_name "${_framework}" NAME_WE)
	merge_file(${_framework_name} "Frameworks/${_framework_name}.framework" "${_archs}")
endforeach()

file(GLOB _binaries "linphone-sdk/mac-${_first_arch}/bin/*")
foreach(_file ${_binaries})
	get_filename_component(_file_name "${_file}" NAME)
	merge_file(${_file_name} "bin" "${_archs}")
endforeach()

file(GLOB _libraries "linphone-sdk/mac-${_first_arch}/lib/*")
foreach(_file ${_libraries})
	get_filename_component(_file_name "${_file}" NAME)
	merge_file(${_file_name} "lib" "${_archs}")
endforeach()

foreach(_arch ${_archs})
	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "linphone-sdk/mac-${_arch}/"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	)
endforeach()













