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
		set(_asan_libarch "arm")
	elseif(_arch STREQUAL "armv7")
		set(_libarch "armeabi-v7a")
		set(_asan_libarch "arm")
	elseif(_arch STREQUAL "arm64")
		set(_libarch "arm64-v8a")
		set(_asan_libarch "aarch64")
	elseif(_arch STREQUAL "x86")
		set(_libarch "x86")
		set(_asan_libarch "i686")
	elseif(_arch STREQUAL "x86_64")
		set(_libarch "x86_64")
		set(_asan_libarch "x86_64")
	else()
		message(FATAL_ERROR "Unknown architecture ${_arch}")
	endif()

	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "libs/${_libarch}"
		COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "libs-debug/${_libarch}"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	)
	
	file(COPY "${CMAKE_ANDROID_NDK}/sources/cxx-stl/llvm-libc++/libs/${_libarch}/libc++_shared.so" DESTINATION "linphone-sdk/android-${_arch}/lib/")

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

	if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
		file(GLOB _asan_libs "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/*/lib/linux/libclang_rt.asan-${_asan_libarch}-android.so")
		foreach(_asan_lib ${_asan_libs})
			configure_file(${LINPHONESDK_DIR}/cmake/Android/wrap.sh.in ${LINPHONESDK_BUILD_DIR}/libs/${_libarch}/wrap.sh)
			configure_file(${LINPHONESDK_DIR}/cmake/Android/wrap.sh.in ${LINPHONESDK_BUILD_DIR}/libs-debug/${_libarch}/wrap.sh)
			execute_process(
				COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${_asan_lib}" "libs/${_libarch}/"
				COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${_asan_lib}" "libs-debug/${_libarch}/"
				WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
			)

			
		endforeach()
	endif()

	execute_process(
		COMMAND "sh" "WORK/android-${_arch}/strip.sh" "libs/${_libarch}/*.so"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	)
endforeach()
