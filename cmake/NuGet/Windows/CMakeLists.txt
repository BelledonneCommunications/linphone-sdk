############################################################################
# GenerateSDK.cmake
# Copyright (C) 2010-2020 Belledonne Communications, Grenoble France
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

linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_UWP_ARCHS}" UWP_ARCHS)
linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_WINDOWS_ARCHS}" WINDOWS_ARCHS)
linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_WINDOWSSTORE_ARCHS}" WINDOWSSTORE_ARCHS)

function(get_windows_name ARCH OUTPUT_VAR)
	if(ARCH STREQUAL "x86")
		set(${OUTPUT_VAR} "win32" PARENT_SCOPE)
	else()
		set(${OUTPUT_VAR} "win64" PARENT_SCOPE)
	endif()
endfunction()

function(get_windowsstore_name ARCH OUTPUT_VAR)
	if(ARCH STREQUAL "x86")
		set(${OUTPUT_VAR} "win32store" PARENT_SCOPE)
	else()
		set(${OUTPUT_VAR} "win64store" PARENT_SCOPE)
	endif()
endfunction()

# Configuration

if(NOT LINPHONESDK_DESKTOP_ZIP_PATH AND NOT LINPHONESDK_UWP_ZIP_PATH AND NOT LINPHONESDK_WINDOWSSTORE_ZIP_PATH)
	message(FATAL_ERROR "You need to specify at least one of LINPHONESDK_DESKTOP_ZIP_PATH, LINPHONESDK_UWP_ZIP_PATH, or LINPHONESDK_WINDOWSSTORE_ZIP_PATH")
endif()

function(add_command_from_zip zip_path nuget_folder)
	FILE(GLOB ZIP_FILES ${zip_path}/*.zip)
	if("${ZIP_FILES}" STREQUAL "")
		message(FATAL_ERROR ".zip files not found in folder : ${zip_path}.")
	else()
		foreach(item ${ZIP_FILES})
			message(STATUS "-  ${item}")
			set(out_dir "${CMAKE_BINARY_DIR}/packages/nuget/${nuget_folder}")
			add_custom_command(TARGET unzip PRE_BUILD
				COMMAND ${CMAKE_COMMAND} -E tar xzf ${item}
				WORKING_DIRECTORY "${out_dir}"
				COMMENT "Unzipping files : ${item} into ${out_dir}\n"
				VERBATIM)
		endforeach()
	endif()
endfunction()

set(LINPHONESDK_OUTPUT_DIR ${CMAKE_BINARY_DIR}/packages/nuget)
add_custom_target(unzip ALL)
add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E remove_directory ${LINPHONESDK_OUTPUT_DIR})
add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${LINPHONESDK_OUTPUT_DIR}/uwp)
add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${LINPHONESDK_OUTPUT_DIR}/windows)
add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${LINPHONESDK_OUTPUT_DIR}/windowsstore)

if(LINPHONESDK_DESKTOP_ZIP_PATH)
	message(STATUS "Retrieve Windows build")
	add_command_from_zip(${LINPHONESDK_DESKTOP_ZIP_PATH} "windows")

	set(NUSPEC_WINDOWS "")
	foreach(WINDOWS_ARCH ${WINDOWS_ARCHS})
		get_windows_name("${WINDOWS_ARCH}" WINDOWS_NAME)
		set(NUSPEC_WINDOWS "${NUSPEC_WINDOWS}
				<file src=\"$OutputSdkDir$\\windows\\linphone-sdk\\${WINDOWS_NAME}\\bin\\*.winmd\" target=\"lib\\win\\\" />
				<file src=\"$OutputSdkDir$\\windows\\linphone-sdk\\${WINDOWS_NAME}\\bin\\**\\*.*\" target=\"lib\\win\\${WINDOWS_ARCH}\" exclude=\"**\\*.pc;**\\cmake\\**\"/>
				<file src=\"$OutputSdkDir$\\windows\\linphone-sdk\\${WINDOWS_NAME}\\lib\\**\\*.*\" target=\"lib\\win\\${WINDOWS_ARCH}\" exclude=\"**\\*.pc;**\\cmake\\**\"/>
				<file src=\"LinphoneSDK.Windows.targets\" target=\"build\\win\" />
				")
		set(NUSPEC_GENERIC "<file src=\"$OutputSdkDir$\\windows\\linphone-sdk\\${WINDOWS_NAME}\\share\\**\\*\" exclude=\"**\\cmake\\**\" target=\"content\\\" />")
	endforeach()
endif()
if(LINPHONESDK_WINDOWSSTORE_ZIP_PATH)
	message(STATUS "Retrieve Windows Store build")
	add_command_from_zip(${LINPHONESDK_WINDOWSSTORE_ZIP_PATH} "windowsstore")

	set(NUSPEC_WINDOWSSTORE "")
	# <file src=\"LinphoneSDK.Windows.targets\" target=\"build\\netcore\" />
	# is already included in nuspec.in, so we avoid declaring twice to avoid errors
	foreach(WINDOWSSTORE_ARCH ${WINDOWSSTORE_ARCHS})
		get_windowsstore_name("${WINDOWSSTORE_ARCH}" WINDOWSSTORE_NAME)
		set(NUSPEC_WINDOWSSTORE "${NUSPEC_WINDOWSSTORE}
			<file src=\"$OutputSdkDir$\\windowsstore\\linphone-sdk\\${WINDOWSSTORE_NAME}\\bin\\*.winmd\" target=\"lib\\netcore\\\" />
			<file src=\"$OutputSdkDir$\\windowsstore\\linphone-sdk\\${WINDOWSSTORE_NAME}\\bin\\**\\*.*\" target=\"lib\\netcore\\${WINDOWSSTORE_ARCH}\" exclude=\"**\\*.pc;**\\cmake\\**\"/>
			<file src=\"$OutputSdkDir$\\windowsstore\\linphone-sdk\\${WINDOWSSTORE_NAME}\\lib\\**\\*.*\" target=\"lib\\netcore\\${WINDOWSSTORE_ARCH}\" exclude=\"**\\*.pc;**\\cmake\\**\"/>
			")
		set(NUSPEC_GENERIC "<file src=\"$OutputSdkDir$\\windowsstore\\linphone-sdk\\${WINDOWSSTORE_NAME}\\share\\**\\*\" exclude=\"**\\cmake\\**\" target=\"content\\\" />")
	endforeach()
endif()
if(LINPHONESDK_UWP_ZIP_PATH)
	message(STATUS "Retrieve UWP build from [${LINPHONESDK_UWP_ARCHS}] archs.")
	add_command_from_zip(${LINPHONESDK_UWP_ZIP_PATH} "uwp")

	set(NUSPEC_UWP "")
	# file <file src=\"LinphoneSDK.Windows.targets\" target=\"build\\UAP10.0\" />
	# is already included in nuspec.in, so we avoid declaring twice to avoid errors
	foreach(UWP_ARCH ${UWP_ARCHS})
		set(NUSPEC_UWP "${NUSPEC_UWP}
			<file src=\"$OutputSdkDir$\\uwp\\linphone-sdk\\uwp-${UWP_ARCH}\\bin\\*.winmd\" target=\"lib\\uap10.0\\\" />
			<file src=\"$OutputSdkDir$\\uwp\\linphone-sdk\\uwp-${UWP_ARCH}\\bin\\**\\*.*\" target=\"lib\\uap10.0\\${UWP_ARCH}\" exclude=\"**\\*.pc;**\\cmake\\**\" />
			<file src=\"$OutputSdkDir$\\uwp\\linphone-sdk\\uwp-${UWP_ARCH}\\lib\\**\\*.*\" target=\"lib\\uap10.0\\${UWP_ARCH}\" exclude=\"**\\*.pc;**\\cmake\\**\" />
		")
		set(NUSPEC_GENERIC "<file src=\"$OutputSdkDir$\\uwp\\linphone-sdk\\uwp-${UWP_ARCH}\\share\\**\\*\" exclude=\"**\\cmake\\**\" target=\"content\\\" />")
	endforeach()
endif()


# Configuration files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/LinphoneSDK.Windows.nuspec.in ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Windows.nuspec @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/_._ ${CMAKE_CURRENT_BINARY_DIR}/_._)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/LinphoneSDK.Windows.targets.in ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Windows.targets @ONLY)
if(LINPHONESDK_OUTPUT_DIR)
	file(TO_NATIVE_PATH "${LINPHONESDK_OUTPUT_DIR}" WINDOWS_INSTALL_PATH)
else()
	file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}" WINDOWS_INSTALL_PATH)
endif()

# Nuget binary
find_program(NUGET_PROGRAM nuget.exe)
if(NOT NUGET_PROGRAM)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/nuget.exe DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	set(NUGET_PROGRAM "nuget.exe")
endif()

add_custom_target(nuget ALL DEPENDS unzip)

#add_custom_command(TARGET nuget PRE_BUILD
#	COMMAND ${CMAKE_COMMAND} -E rm -rRf "${CMAKE_CURRENT_BINARY_DIR}/packages"
#)
	
add_custom_command(TARGET nuget
	COMMAND ${NUGET_PROGRAM} "pack" "LinphoneSDK.Windows.nuspec" "-Properties" "OutputSdkDir=${WINDOWS_INSTALL_PATH}" "-OutputDirectory" "packages"
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_command(TARGET nuget
	COMMAND ${CMAKE_COMMAND} "-E" "copy_directory"  "${CMAKE_CURRENT_BINARY_DIR}/packages" "${CMAKE_INSTALL_PREFIX}/packages"
	WORKING_DIRECTORY ${LINPHONESDK_OUTPUT_DIR}
)
# "-OutputDirectory" "${WINDOWS_INSTALL_PATH}"
