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


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_IOS_ARCHS}" _archs)


# Create the apple-darwin directory that will contain the merged content of all architectures
execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "linphone-sdk/apple-darwin"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)

execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "linphone-sdk/apple-darwin"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)


# Copy and merge content of all architectures in the apple-darwin directory
list(GET _archs 0 _first_arch)

execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "linphone-sdk/${_first_arch}-apple-darwin.ios/Frameworks" "linphone-sdk/apple-darwin/Frameworks"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)
execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "linphone-sdk/${_first_arch}-apple-darwin.ios/share/liblinphone_tester" "linphone-sdk/apple-darwin/Resources/liblinphone_tester"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)



file(GLOB _frameworks "linphone-sdk/${_first_arch}-apple-darwin.ios/Frameworks/*.framework")
foreach(_framework ${_frameworks})
	get_filename_component(_framework_name "${_framework}" NAME_WE)
	set(_all_arch_frameworks)
	foreach(_arch ${_archs})
		list(APPEND _all_arch_frameworks "linphone-sdk/${_arch}-apple-darwin.ios/Frameworks/${_framework_name}.framework/${_framework_name}")
	endforeach()
	if(_framework_name STREQUAL "linphonesw")
		foreach(_arch ${_archs})
			execute_process(
				COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "linphone-sdk/${_arch}-apple-darwin.ios/Frameworks/linphonesw.framework/Modules/linphonesw.swiftmodule" "linphone-sdk/apple-darwin/Frameworks/linphonesw.framework/Modules/linphonesw.swiftmodule"
				WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
			)
		endforeach()
	endif()
	string(REPLACE ";" " " _arch_string "${_archs}")
	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "echo" "Mixing ${_framework_name} for archs [${_arch_string}]"
		COMMAND "lipo" "-create" "-output" "linphone-sdk/apple-darwin/Frameworks/${_framework_name}.framework/${_framework_name}" ${_all_arch_frameworks}
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	)
endforeach()

if(ENABLE_SWIFT_WRAPPER AND ENABLE_JAZZY_DOC)
	message("generating jazzy doc for swift module ......")
	execute_process(
		COMMAND "jazzy" "--readme" "${LINPHONESDK_DIR}/linphone/wrappers/swift/README"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}/WORK/ios-${_first_arch}/Build/linphone/"
	)
	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "WORK/ios-${_first_arch}/Build/linphone/docs" "docs"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	)
endif()
