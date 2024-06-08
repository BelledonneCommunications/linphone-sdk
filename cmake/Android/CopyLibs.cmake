############################################################################
# CopyLibs.cmake
# Copyright (C) 2010-2024 Belledonne Communications, Grenoble France
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


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_ANDROID_ARCHS}" _ANDROID_ARCHS)


execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "libs"
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "libs-debug"
)

foreach(_arch ${_ANDROID_ARCHS})
	if(_arch STREQUAL "arm")
		set(_libarch "armeabi")
		set(_asan_libarch "arm")
		set(_ndk_sysroot "arm-linux-androideabi")
	elseif(_arch STREQUAL "armv7")
		set(_libarch "armeabi-v7a")
		set(_asan_libarch "arm")
		set(_ndk_sysroot "arm-linux-androideabi")
	elseif(_arch STREQUAL "arm64")
		set(_libarch "arm64-v8a")
		set(_asan_libarch "aarch64")
		set(_ndk_sysroot "aarch64-linux-android")
	elseif(_arch STREQUAL "x86")
		set(_libarch "x86")
		set(_asan_libarch "i686")
		set(_ndk_sysroot "i686-linux-android")
	elseif(_arch STREQUAL "x86_64")
		set(_libarch "x86_64")
		set(_asan_libarch "x86_64")
		set(_ndk_sysroot "x86_64-linux-android")
	else()
		message(FATAL_ERROR "Unknown architecture ${_arch}")
	endif()

	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "libs/${_libarch}"
		COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "libs-debug/${_libarch}"
	)

	file(GLOB_RECURSE _libs "linphone-sdk/android-${_arch}/lib/*.so")
	foreach(_lib ${_libs})
		execute_process(
			COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${_lib}" "libs/${_libarch}/"
			COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${_lib}" "libs-debug/${_libarch}/"
			WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
		)
	endforeach()

	if(IS_ASAN)
		configure_file("${LINPHONESDK_DIR}/cmake/Android/wrap.sh.in" "libs/${_libarch}/wrap.sh")
		configure_file("${LINPHONESDK_DIR}/cmake/Android/wrap.sh.in" "libs-debug/${_libarch}/wrap.sh")
	endif()

	execute_process(
		COMMAND "sh" "./android-${_arch}/${STRIP_COMMAND}" "libs/${_libarch}/*.so"
	)
endforeach()
