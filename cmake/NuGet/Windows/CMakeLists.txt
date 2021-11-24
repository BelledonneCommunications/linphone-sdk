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

#Configuration

if(NOT LINPHONESDK_DESKTOP_ZIP_PATH OR NOT LINPHONESDK_UWP_ZIP_PATH OR NOT LINPHONESDK_WINDOWSSTORE_ZIP_PATH)
	message(FATAL_ERROR "You need to specify at least one of LINPHONESDK_DESKTOP_ZIP_PATH, LINPHONESDK_UWP_ZIP_PATH, or LINPHONESDK_WINDOWSSTORE_ZIP_PATH")
endif()

function(add_command_from_zip zip_path nuget_folder)
	FILE(GLOB ZIP_FILES ${zip_path}/*.zip)
	set(FILE_ITEM "")
	foreach(item ${ZIP_FILES})
		if("${FILE_ITEM}" STREQUAL "")
			set(FILE_ITEM ${item})
		elseif(${item} IS_NEWER_THAN ${FILE_ITEM})
			set(FILE_ITEM ${item})
		endif()
	endforeach(item)
	if(NOT ${FILE_ITEM} STREQUAL "")
		message(STATUS "-  ${FILE_ITEM}")
		add_custom_command(TARGET unzip PRE_BUILD
			COMMAND ${CMAKE_COMMAND} -E tar xzf ${FILE_ITEM}
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/WORK/packages/nuget/${nuget_folder}
			COMMENT "Unzipping files : ${FILE_ITEM}\n"
			VERBATIM)
	else()
		message(FATAL_ERROR ".zip files not found in folder : ${zip_path}.")
	endif()
endfunction()

set(LINPHONESDK_OUTPUT_DIR ${CMAKE_BINARY_DIR}/WORK/packages/nuget)
add_custom_target( unzip ALL)
add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E remove_directory ${LINPHONESDK_OUTPUT_DIR})
add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${LINPHONESDK_OUTPUT_DIR}/desktop)
add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${LINPHONESDK_OUTPUT_DIR}/uwp)
add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${LINPHONESDK_OUTPUT_DIR}/windowsstore)

if(LINPHONESDK_DESKTOP_ZIP_PATH)
	message(STATUS "Retrieve Desktop build")
	add_command_from_zip(${LINPHONESDK_DESKTOP_ZIP_PATH} "desktop")

	set(NUSPEC_WIN32 "<file src=\"$OutputSdkDir$\\desktop\\desktop\\bin\\*.winmd\" target=\"lib\\win\\\" />
			<file src=\"$OutputSdkDir$\\desktop\\desktop\\bin\\**\\*.*\" target=\"lib\\win\\x86\" exclude=\"**\\*.pc;**\\cmake\\**\"/>
			<file src=\"$OutputSdkDir$\\desktop\\desktop\\lib\\**\\*.*\" target=\"lib\\win\\x86\" exclude=\"**\\*.pc;**\\cmake\\**\"/>
			<file src=\"LinphoneSDK.Windows.targets\" target=\"build\\win\" />
			")
	set(NUSPEC_GENERIC "<file src=\"$OutputSdkDir$\\desktop\\desktop\\share\\**\\*\" exclude=\"**\\cmake\\**\" target=\"content\\\" />")
endif()
if(LINPHONESDK_WINDOWSSTORE_ZIP_PATH)
	message(STATUS "Retrieve Windows Store build")
	add_command_from_zip(${LINPHONESDK_WINDOWSSTORE_ZIP_PATH} "windowsstore")

	set(NUSPEC_WINDOWSSTORE "<file src=\"$OutputSdkDir$\\windowsstore\\desktop\\bin\\*.winmd\" target=\"lib\\netcore\\\" />
		<file src=\"$OutputSdkDir$\\windowsstore\\desktop\\bin\\**\\*.*\" target=\"lib\\netcore\\x86\" exclude=\"**\\*.pc;**\\cmake\\**\"/>
		<file src=\"$OutputSdkDir$\\windowsstore\\desktop\\lib\\**\\*.*\" target=\"lib\\netcore\\x86\" exclude=\"**\\*.pc;**\\cmake\\**\"/>
		<file src=\"LinphoneSDK.Windows.targets\" target=\"build\\netcore\" />
		")
	set(NUSPEC_GENERIC "<file src=\"$OutputSdkDir$\\windowsstore\\desktop\\share\\**\\*\" exclude=\"**\\cmake\\**\" target=\"content\\\" />")
endif()
if(LINPHONESDK_UWP_ZIP_PATH)
	message(STATUS "Retrieve UWP build")
	add_command_from_zip(${LINPHONESDK_UWP_ZIP_PATH} "uwp")

	set(NUSPEC_UWP "<file src=\"$OutputSdkDir$\\uwp\\uwp-x64\\bin\\*.winmd\" target=\"lib\\uap10.0\\\" />
			<file src=\"$OutputSdkDir$\\uwp\\uwp-x64\\bin\\**\\*.*\" target=\"lib\\uap10.0\\x64\" exclude=\"**\\*.pc;**\\cmake\\**\"/>
			<file src=\"$OutputSdkDir$\\uwp\\uwp-x64\\lib\\**\\*.*\" target=\"lib\\uap10.0\\x64\" exclude=\"**\\*.pc;**\\cmake\\**\"/>
			<file src=\"LinphoneSDK.Windows.targets\" target=\"build\\UAP10.0\" />
			")
	set(NUSPEC_GENERIC "<file src=\"$OutputSdkDir$\\uwp\\uwp-x64\\share\\**\\*\" exclude=\"**\\cmake\\**\" target=\"content\\\" />")
endif()


#Configuration files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/LinphoneSDK.Windows.nuspec.in ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Windows.nuspec @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/_._ ${CMAKE_CURRENT_BINARY_DIR}/_._)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/LinphoneSDK.Windows.targets.in ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.Windows.targets @ONLY)
if(LINPHONESDK_OUTPUT_DIR)
	file(TO_NATIVE_PATH "${LINPHONESDK_OUTPUT_DIR}" WINDOWS_INSTALL_PATH)
else()
	file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}" WINDOWS_INSTALL_PATH)
endif()

#Nuget binary
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
# "-OutputDirectory" "${WINDOWS_INSTALL_PATH}"
#install(FILES "*.nupkg" DESTINATION ${CMAKE_INSTALL_PREFIX})

install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages"
        DESTINATION ${CMAKE_INSTALL_PREFIX}
        FILES_MATCHING PATTERN "*.nupkg"
)
