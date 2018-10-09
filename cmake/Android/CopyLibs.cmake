############################################################################
# CopyLibs.cmake
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


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_ANDROID_ARCHS}" _archs)


execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "libs"
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "libs-debug"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)

foreach(_arch ${_archs})
	if(_arch STREQUAL "arm")
		set(_libarch "armeabi")
	elseif(_arch STREQUAL "armv7")
		set(_libarch "armeabi-v7a")
	elseif(_arch STREQUAL "arm64")
		set(_libarch "arm64-v8a")
	elseif(_arch STREQUAL "x86")
		set(_libarch "x86")
	elseif(_arch STREQUAL "x86_64")
		set(_libarch "x86_64")
	else()
		message(FATAL_ERROR "Unknown architecture ${_arch}")
	endif()

	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "libs/${_libarch}"
		COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "libs-debug/${_libarch}"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	)

	file(GLOB _libs "linphone-sdk/android-${_arch}/lib/lib*.so")
	file(GLOB _plugins "linphone-sdk/android-${_arch}/lib/mediastreamer/plugins/*.so")
	foreach(_lib ${_libs})
		execute_process(
			COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${_lib}" "libs/${_libarch}/"
			COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${_lib}" "libs-debug/${_libarch}/"
			WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
		)
	endforeach()
	foreach(_plugin ${_plugins})
		execute_process(
			COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${_plugin}" "libs/${_libarch}/"
			COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${_plugin}" "libs-debug/${_libarch}/"
			WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
		)
	endforeach()

	execute_process(
		COMMAND "sh" "WORK/android-${_arch}/strip.sh" "libs/${_libarch}/*.so"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	)
endforeach()
