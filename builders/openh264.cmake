############################################################################
# openh264.cmake
# Copyright (C) 2014  Belledonne Communications, Grenoble France
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	# Use prebuilt library in the source tree for Windows 10
	set(EP_openh264_EXTERNAL_SOURCE_PATHS "build/openh264")
else()
	find_program(NASM_PROGRAM
		NAMES nasm nasm.exe
		HINTS "${LINPHONE_BUILDER_WORK_DIR}/windows_tools"
	)
	if(NOT NASM_PROGRAM)
		if(WIN32)
			message(FATAL_ERROR "Could not find the nasm.exe program. Please install it from http://www.nasm.us/")
		else()
			message(FATAL_ERROR "Could not find the nasm program.")
		endif()
	endif()

	set(EP_openh264_VERSION "1.5.0")	# Keep this variable, it is used for packaging to know the version to download from Cisco
	set(EP_openh264_GIT_REPOSITORY "https://github.com/cisco/openh264" CACHE STRING "openh264 repository URL")
	set(EP_openh264_GIT_TAG "v${EP_openh264_VERSION}" CACHE STRING "openh264 tag to use")
	set(EP_openh264_EXTERNAL_SOURCE_PATHS "externals/openh264")

	set(EP_openh264_BUILD_METHOD "custom")
	set(EP_openh264_LINKING_TYPE "-static")
	set(EP_openh264_BUILD_TYPE "Release")	# Always use Release build type, otherwise the codec is too slow...
	set(EP_openh264_CONFIGURE_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/openh264/configure.sh.cmake)
	set(EP_openh264_BUILD_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/openh264/build.sh.cmake)
	set(EP_openh264_INSTALL_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/openh264/install.sh.cmake)
	if(MSVC)
		set(EP_openh264_ADDITIONAL_OPTIONS "OS=\"msvc\"")
	elseif(ANDROID)
		if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
			set(EP_openh264_ADDITIONAL_OPTIONS "OS=\"android\" NDKROOT=\"${ANDROID_NDK_PATH}\" TOOLCHAINPREFIX=\"${ANDROID_TOOLCHAIN_PREFIX}\" TARGET=\"android-${CMAKE_ANDROID_API}\" ARCH=\"arm\"")
		else()
			set(EP_openh264_ADDITIONAL_OPTIONS "OS=\"android\" NDKROOT=\"${ANDROID_NDK_PATH}\" TOOLCHAINPREFIX=\"${ANDROID_TOOLCHAIN_PREFIX}\" TARGET=\"android-${CMAKE_ANDROID_API}\" ARCH=\"x86\"")
		endif()
	elseif(APPLE)
		if(IOS)
			if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
				set(EP_openh264_ADDITIONAL_OPTIONS "OS=\"ios\" ARCH=\"arm64\"")
				#XCode7  allows bitcode
				if (NOT ${XCODE_VERSION} VERSION_LESS 7)
	        			set(EP_openh264_EXTRA_CFLAGS "-fembed-bitcode")
				endif()
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7")
				set(EP_openh264_ADDITIONAL_OPTIONS "OS=\"ios\" ARCH=\"armv7\"")
				#XCode7  allows bitcode
				if (NOT ${XCODE_VERSION} VERSION_LESS 7)
	        			set(EP_openh264_EXTRA_CFLAGS "-fembed-bitcode")
				endif()
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
				set(EP_openh264_ADDITIONAL_OPTIONS "OS=\"ios\" ARCH=\"x86_64\"")
			else()
				set(EP_openh264_ADDITIONAL_OPTIONS "OS=\"ios\" ARCH=\"i386\"")
			endif()
		else()
			if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
				set(EP_openh264_ADDITIONAL_OPTIONS "ARCH=\"x86_64\"")
			else()
				set(EP_openh264_ADDITIONAL_OPTIONS "ARCH=\"x86\"")
			endif()
		endif()
	endif()
endif()
