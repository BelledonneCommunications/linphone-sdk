############################################################################
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
############################################################################

find_program(NUGET nuget REQUIRED)
find_program(MSBUILD msbuild REQUIRED)

if(NOT LINPHONESDK_CSHARP_WRAPPER_PATH)
	message(FATAL_ERROR "Please specify LINPHONESDK_CSHARP_WRAPPER_PATH. This should be the path to `LinphoneWrapper.cs`.")
endif()
if(NOT LINPHONESDK_ANDROID_AAR_PATH)
	message(FATAL_ERROR "Please specify LINPHONESDK_ANDROID_AAR_PATH. This should be the path to the Android `.aar` archive (e.g. `linphone-sdk-android-release.aar`).")
endif()
if(NOT LINPHONESDK_IOS_FRAMEWORKS_PATH)
	message(FATAL_ERROR "Please specify LINPHONESDK_IOS_FRAMEWORKS_PATH. This should be the path to the `Frameworks/` folder containing Apple `.framework` folders.")
endif()

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/LinphoneSDK.Xamarin DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.nuspec.in
	${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.nuspec
	@ONLY
)

add_custom_command(
	COMMENT "Copying native iOS & Android libraries"
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.Shared/LinphoneWrapper.cs
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.Android/Jars/liblinphone-sdk.aar
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.iOS/Frameworks/linphone.framework/linphone
	DEPENDS
		${LINPHONESDK_CSHARP_WRAPPER_PATH}
		${LINPHONESDK_ANDROID_AAR_PATH}
		${LINPHONESDK_IOS_FRAMEWORKS_PATH}/linphone.framework/linphone
	COMMAND ${CMAKE_COMMAND} -E copy ${LINPHONESDK_CSHARP_WRAPPER_PATH} ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.Shared/LinphoneWrapper.cs
	COMMAND ${CMAKE_COMMAND} -E copy ${LINPHONESDK_ANDROID_AAR_PATH} ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.Android/Jars/liblinphone-sdk.aar
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${LINPHONESDK_IOS_FRAMEWORKS_PATH} ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.iOS/Frameworks/
)

add_custom_command(
	COMMENT "Building wrapped SDK for Xamarin"
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.Android/bin/Release/Linphone.dll
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.iOS/bin/Release/Linphone.dll
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.Standard/bin/Release/netstandard2.0/Linphone.dll
	DEPENDS 
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.Shared/LinphoneWrapper.cs
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.Android/Jars/liblinphone-sdk.aar
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.iOS/Frameworks/linphone.framework/linphone
	COMMAND ${NUGET} "restore"
	COMMAND ${MSBUILD} "-maxCpuCount" "-p:Configuration=Release"
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin
)

add_custom_command(
	COMMENT "Packaging Xamarin NuGet"
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/packages/LinphoneSDK.Xamarin.${LINPHONESDK_VERSION}.nupkg"
	DEPENDS
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.Android/bin/Release/Linphone.dll
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.iOS/bin/Release/Linphone.dll
		${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.Standard/bin/Release/netstandard2.0/Linphone.dll
	COMMAND ${NUGET} "pack" "LinphoneSDK.Xamarin.nuspec" "-OutputDirectory" "${CMAKE_CURRENT_BINARY_DIR}/packages"
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Xamarin
)

add_custom_target(${PROJECT_NAME}
	ALL
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/packages/LinphoneSDK.Xamarin.${LINPHONESDK_VERSION}.nupkg"
)

install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages"
        DESTINATION ${CMAKE_INSTALL_PREFIX}
        FILES_MATCHING PATTERN "*.nupkg"
)