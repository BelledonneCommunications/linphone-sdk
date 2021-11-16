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


# Create the desktop directory that will contain the merged content of all architectures
execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "${CMAKE_INSTALL_PREFIX}"
)

execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "${CMAKE_INSTALL_PREFIX}"
)


# Copy and merge content of all architectures in the desktop directory
list(GET _archs 0 _first_arch)

execute_process(# Do not use copy_directory because of symlinks
	COMMAND "cp" "-R"  "linphone-sdk/mac-${_first_arch}/" "${CMAKE_INSTALL_PREFIX}"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)

#####		MIX
#Get all files in output
file(GLOB_RECURSE _binaries RELATIVE "${LINPHONESDK_BUILD_DIR}/linphone-sdk/mac-${_first_arch}/" "${LINPHONESDK_BUILD_DIR}/linphone-sdk/mac-${_first_arch}/*")
foreach(_file ${_binaries})
	get_filename_component( ABSOLUTE_FILE "linphone-sdk/mac-${_first_arch}/${_file}" ABSOLUTE)
	if(NOT IS_SYMLINK ${ABSOLUTE_FILE})
#Check if lipo can detect an architecture
		execute_process(COMMAND lipo -archs "linphone-sdk/mac-${_first_arch}/${_file}"
			OUTPUT_VARIABLE FILE_ARCHITECTURE
			OUTPUT_STRIP_TRAILING_WHITESPACE
			WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
			ERROR_QUIET
		)
		if( NOT "${FILE_ARCHITECTURE}" STREQUAL "" )
#There is at least one architecture : Use this candidate to mix with another architectures
			set(_all_arch_files)
			foreach(_arch ${_archs})
				list(APPEND _all_arch_files "linphone-sdk/mac-${_arch}/${_file}")
			endforeach()
			string(REPLACE ";" " " _arch_string "${_archs}")
			message (STATUS "Mixing ${_file} for archs [${_arch_string}]")
			execute_process(
				COMMAND "lipo" "-create" "-output" "${CMAKE_INSTALL_PREFIX}/${_file}" ${_all_arch_files}
				WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
			)
		endif()
	endif()
endforeach()

#When done, remove architectures folders and keep only desktop
foreach(_arch ${_archs})
	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "linphone-sdk/mac-${_arch}/"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	)
endforeach()

