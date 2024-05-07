############################################################################
#
#  Copyright (c) 2021-2023 Belledonne Communications SARL.
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

# Require dotnet to generate the nuget package
find_program(DOTNET dotnet REQUIRED)

if(NOT LINPHONESDK_VERSION)
	message(FATAL_ERROR "Please specify LINPHONESDK_VERSION. This will be used to derive the version of the `.nupkg`.")
endif()

# We absolutely need the wrapper to compile for at least one platform
if(NOT LINPHONESDK_CSHARP_WRAPPER_PATH)
	message(FATAL_ERROR "Please specify LINPHONESDK_CSHARP_WRAPPER_PATH. This should be the path to `LinphoneWrapper.cs`.")
endif()

if(NOT EXISTS "${LINPHONESDK_CSHARP_WRAPPER_PATH}/LinphoneWrapper.cs")
	message(FATAL_ERROR "${LINPHONESDK_CSHARP_WRAPPER_PATH}/LinphoneWrapper.cs not found.")
endif()

if(NOT LINPHONESDK_SHARE_PATH)
	message(WARNING "LINPHONESDK_SHARE_PATH not specified. This should be the path to the share folder containing grammars and rootca.pem.")
endif()

if(NOT LINPHONESDK_WIN32_DESKTOP_PATH)
	message(WARNING "LINPHONESDK_WIN32_DESKTOP_PATH not specified. This should be the path to the bin folder containing Win32 dll.")
endif()

if(NOT LINPHONESDK_WIN64_DESKTOP_PATH)
	message(WARNING "LINPHONESDK_WIN64_DESKTOP_PATH not specified. This should be the path to the bin folder containing Win64 dll.")
endif()

if(NOT LINPHONESDK_OSX_FRAMEWORK_PATH)
	message(WARNING "LINPHONESDK_OSX_FRAMEWORK_PATH not specified. This should be the path to the osx frameworks.")
endif()

if(NOT LINPHONESDK_LINUX_SO_PATH)
	message(WARNING "LINPHONESDK_LINPHONE_SO_PATH not specified. This should be the path to `liblinphone.so` and other .so libraries.")
else()
	file(REAL_PATH ${LINPHONESDK_LINUX_SO_PATH} LINPHONESDK_LINUX_SO_PATH EXPAND_TILDE)
endif()

if(NOT LINPHONESDK_ANDROID_AAR_PATH)
	message(WARNING "LINPHONESDK_ANDROID_AAR_PATH not specified. This should be the path to `liblinphone.aar`.")
endif()

if(NOT LINPHONESDK_IOS_XCFRAMEWORK_PATH)
	message(WARNING "LINPHONESDK_IOS_XCFRAMEWORK_PATH not specified. This should be the path to the `XCFrameworks/` folder containing Apple `.xcframework` folders.")
endif()


####################### Csproj generation ##############

# Project file of the nuget, where the magic happens
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/LinphoneSDK.csproj.in
	${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.csproj
	@ONLY
)

####################### Targets generation ##############

# Generate native calls for all ios frameworks in targets file
if(LINPHONESDK_IOS_XCFRAMEWORK_PATH)
	set(NATIVE_REFERENCES "")
	file(GLOB files "${LINPHONESDK_IOS_XCFRAMEWORK_PATH}/*")
	foreach(file ${files})
		get_filename_component(fullName ${file} NAME)
		string(APPEND NATIVE_REFERENCES "    <NativeReference Include=\"\$(MSBuildThisFileDirectory)\\..\\runtimes\\ios\\native\\${fullName}\">\n")
		string(APPEND NATIVE_REFERENCES "      <Kind>Framework</Kind>\n")
		string(APPEND NATIVE_REFERENCES "    </NativeReference>\n")
	endforeach()
endif()


# file to specify to the client what to embed in apk and bundle app (android/ios)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/LinphoneSDK.targets.in
	${CMAKE_CURRENT_BINARY_DIR}/build/LinphoneSDK.targets
	@ONLY
)

####################### Nuget generation ##############

# Call pack, need at least the csharp Wrapper. Empty variables PATH will be ignored.
add_custom_command(
	COMMENT "Packaging NuGet"
	OUTPUT "${CMAKE_BINARY_DIR}/packages/LinphoneSDK.${NUPKG_VERSION}.nupkg"
	DEPENDS
		"${LINPHONESDK_CSHARP_WRAPPER_PATH}/LinphoneWrapper.cs"
	COMMAND ${DOTNET} "pack" "LinphoneSDK.csproj" "--output" "${CMAKE_BINARY_DIR}/packages"
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(${PROJECT_NAME}
	ALL
	DEPENDS
		"${CMAKE_BINARY_DIR}/packages/LinphoneSDK.${NUPKG_VERSION}.nupkg"
		"${LINPHONESDK_CSHARP_WRAPPER_PATH}/LinphoneWrapper.cs"
)
