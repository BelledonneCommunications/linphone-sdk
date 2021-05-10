################################################################################
#
#  Copyright (c) 2021 Belledonne Communications SARL.
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
################################################################################

list(APPEND CMAKE_MODULE_PATH "${LINPHONESDK_DIR}/cmake")
include(LinphoneSdkUtils)


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_MACOS_ARCHS}" _archs)


# Create the apple-macos directory that will contain the merged content of all architectures
execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "linphone-sdk/apple-macos"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)

execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "linphone-sdk/apple-macos"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)


# Copy and merge content of all architectures in the apple-macos directory
list(GET _archs 0 _first_arch)

execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "linphone-sdk/mac-${_first_arch}/" "linphone-sdk/apple-macos/"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)

#this function will merge architectures on a file in a path
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

#Get all files that need to be changed : frameworks, bin and lib
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

#When done, remove architectures folders and keep only apple-macos
foreach(_arch ${_archs})
	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "linphone-sdk/mac-${_arch}/"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	)
endforeach()

