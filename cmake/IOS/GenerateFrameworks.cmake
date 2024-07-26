############################################################################
# GenerateFrameworks.cmake
# Copyright (C) 2010-2023 Belledonne Communications, Grenoble France
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

include("${LINPHONESDK_DIR}/cmake/LinphoneSdkUtils.cmake")


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_IOS_ARCHS}" _IOS_ARCHS)

message("Creating the destination directory that will contain the merged content of all architectures")
# Create the destination directory that will contain the merged content of all architectures
execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "${CMAKE_INSTALL_PREFIX}"
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "${CMAKE_INSTALL_PREFIX}/Frameworks"
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "${CMAKE_INSTALL_PREFIX}/Resources"
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "${CMAKE_INSTALL_PREFIX}/share"
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "${CMAKE_INSTALL_PREFIX}/XCFrameworks"
)


# Copy and merge content of all architectures in the destination directory
list(GET _IOS_ARCHS 0 _FIRST_ARCH)

list(FIND _IOS_ARCHS "x86_64-simulator" _X86_64_SIMULATOR_FOUND)
list(FIND _IOS_ARCHS "arm64-simulator" _ARM64_SIMULATOR_FOUND)


if(ENABLE_SWIFT_DOC AND _ARM64_SIMULATOR_FOUND GREATER -1)
	message("Generating doc for swift module, we need arch arm64 to generate doc!")
	set(CONFIGURATION "RelWithDebInfo")
	if (CMAKE_BUILD_TYPE)
		set(CONFIGURATION "${CMAKE_BUILD_TYPE}")
	elseif (CMAKE_CONFIGURATION_TYPES)
		if (NOT "RelWithDebInfo" IN_LIST CMAKE_CONFIGURATION_TYPES)
			list(GET CMAKE_CONFIGURATION_TYPES 0 CONFIGURATION)
		endif()
	endif()
	execute_process(
		COMMAND "xcodebuild" "docbuild" "-scheme" "linphonesw" "-configuration" "${CONFIGURATION}" "-destination" "platform=iOS Simulator,name=iPhone 13" "DOCC_HOSTING_BASE_PATH=${LINPHONESDK_SWIFT_DOC_HOSTING_PATH}"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}/ios-arm64-simulator/liblinphone/"
	)
	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "linphonesw.doccarchive" "${LINPHONESDK_BUILD_DIR}/docs/linphonesw.doccarchive"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}/ios-arm64-simulator/lib/${CONFIGURATION}/"
	)

	if(NOT ENABLE_SWIFT_WRAPPER_COMPILATION)
		message("Not ENABLE_SWIFT_WRAPPER_COMPILATION, remove linphonesw.frameworks...")
		foreach(_ARCH IN LISTS _IOS_ARCHS)
			file(REMOVE_RECURSE "${LINPHONESDK_NAME}/ios-${_ARCH}/Frameworks/linphonesw.framework")
		endforeach()
	endif()
else()
	message("Swift doc not generated. ENABLE_SWIFT_DOC=${ENABLE_SWIFT_DOC}, _ARM64_SIMULATOR_FOUND=${_ARM64_SIMULATOR_FOUND}")
endif()

if(ENABLE_FAT_BINARY)
	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "${LINPHONESDK_NAME}/ios-${_FIRST_ARCH}/Frameworks" "${CMAKE_INSTALL_PREFIX}/Frameworks"
	)
endif()
execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "${LINPHONESDK_NAME}/ios-${_FIRST_ARCH}/share/liblinphone-tester" "${CMAKE_INSTALL_PREFIX}/Resources/liblinphone-tester"
	COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "${LINPHONESDK_NAME}/ios-${_FIRST_ARCH}/share/linphonesw" "${CMAKE_INSTALL_PREFIX}/share/linphonesw"
	COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "${LINPHONESDK_NAME}/ios-${_FIRST_ARCH}/share/linphonecs" "${CMAKE_INSTALL_PREFIX}/share/linphonecs"
)

file(GLOB _FRAMEWORKS "${LINPHONESDK_NAME}/ios-${_FIRST_ARCH}/Frameworks/*.framework")

if(NOT ENABLE_FAT_BINARY)
	if((_X86_64_SIMULATOR_FOUND GREATER -1) AND (_ARM64_SIMULATOR_FOUND GREATER -1))
		# We have to lipo both x86_64-simulator and arm64-simulator as -create-xcframework doesn't do it itself.
		# See: https://developer.apple.com/forums/thread/666335
		message(STATUS "Mixing x86_64-simulator and arm64-simulator before creating XCFrameworks")

		foreach(_FRAMEWORK IN LISTS _FRAMEWORKS)
			get_filename_component(_FRAMEWORK_NAME "${_FRAMEWORK}" NAME_WE)
			execute_process(
				COMMAND "lipo" "-create" "-output" "${LINPHONESDK_NAME}/ios-x86_64-simulator/Frameworks/${_FRAMEWORK_NAME}.framework/${_FRAMEWORK_NAME}" "${LINPHONESDK_NAME}/ios-x86_64-simulator/Frameworks/${_FRAMEWORK_NAME}.framework/${_FRAMEWORK_NAME}" "${LINPHONESDK_NAME}/ios-arm64-simulator/Frameworks/${_FRAMEWORK_NAME}.framework/${_FRAMEWORK_NAME}"
			)
		endforeach()

		# Remove then arm64-simulator as x86_64-simulator contains it
		list(REMOVE_ITEM _IOS_ARCHS "arm64-simulator")
	endif()
endif()

foreach(_FRAMEWORK IN LISTS _FRAMEWORKS)
	get_filename_component(_FRAMEWORK_NAME "${_FRAMEWORK}" NAME_WE)
	set(_ALL_ARCH_FRAMEWORKS)
	foreach(_ARCH IN LISTS _IOS_ARCHS)
		if(ENABLE_FAT_BINARY)
			list(APPEND _ALL_ARCH_FRAMEWORKS "${LINPHONESDK_NAME}/ios-${_ARCH}/Frameworks/${_FRAMEWORK_NAME}.framework/${_FRAMEWORK_NAME}")
		else()
			list(APPEND _ALL_ARCH_FRAMEWORKS "-framework")
			list(APPEND _ALL_ARCH_FRAMEWORKS "${LINPHONESDK_NAME}/ios-${_ARCH}/Frameworks/${_FRAMEWORK_NAME}.framework")
		endif()
	endforeach()
	if(ENABLE_SWIFT_WRAPPER AND ENABLE_SWIFT_WRAPPER_COMPILATION)
		if(_FRAMEWORK_NAME STREQUAL "linphonesw")
			foreach(_ARCH IN LISTS _IOS_ARCHS)
				execute_process(
					COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "${LINPHONESDK_NAME}/ios-${_ARCH}/Frameworks/linphonesw.framework/Modules/linphonesw.swiftmodule" "${CMAKE_INSTALL_PREFIX}/Frameworks/linphonesw.framework/Modules/linphonesw.swiftmodule"
				)
			endforeach()
		endif()
	endif()
	string(REPLACE ";" " " _ARCH_STRING "${_IOS_ARCHS}")
	if(ENABLE_FAT_BINARY)
		message (STATUS "Mixing ${_FRAMEWORK_NAME} for archs [${_ARCH_STRING}]")
		execute_process(
			COMMAND "lipo" "-create" "-output" "${CMAKE_INSTALL_PREFIX}/Frameworks/${_FRAMEWORK_NAME}.framework/${_FRAMEWORK_NAME}" ${_ALL_ARCH_FRAMEWORKS}
		)
	else()
		message (STATUS "Creating XCFramework for ${_FRAMEWORK_NAME} for archs [${_ARCH_STRING}]")
		execute_process(
			COMMAND "xcodebuild" "-create-xcframework" "-output" "${CMAKE_INSTALL_PREFIX}/XCFrameworks/${_FRAMEWORK_NAME}.xcframework" ${_ALL_ARCH_FRAMEWORKS}
		)
	endif()
endforeach()
