############################################################################
# config-desktop.cmake
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

# Define default values for the linphone builder options
set(DEFAULT_VALUE_ENABLE_DTLS ON)
set(DEFAULT_VALUE_ENABLE_FFMPEG ON)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_ILBC ON)
set(DEFAULT_VALUE_ENABLE_ISAC ON)
set(DEFAULT_VALUE_ENABLE_MKV ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
if(WIN32 OR APPLE)
	set(DEFAULT_VALUE_ENABLE_PACKAGING ON)
else()
	set(DEFAULT_VALUE_ENABLE_PACKAGING OFF)
endif()
set(DEFAULT_VALUE_ENABLE_SILK ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_TUNNEL OFF)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS ON)
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_VPX ON)
set(DEFAULT_VALUE_ENABLE_WASAPI ON)
set(DEFAULT_VALUE_ENABLE_WEBRTC_AEC OFF)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)

set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_STATIC=NO")


# Global configuration
set(LINPHONE_BUILDER_HOST "")
if(APPLE)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(CMAKE_OSX_ARCHITECTURES "x86_64")
		set(LINPHONE_BUILDER_HOST "x86_64-apple-darwin")
	else()
		set(CMAKE_OSX_ARCHITECTURES "i386")
		set(LINPHONE_BUILDER_HOST "i686-apple-darwin")
	endif()
	set(LINPHONE_BUILDER_CPPFLAGS "-arch ${CMAKE_OSX_ARCHITECTURES}")
	set(LINPHONE_BUILDER_OBJCFLAGS "-arch ${CMAKE_OSX_ARCHITECTURES}")
	set(LINPHONE_BUILDER_LDFLAGS "-arch ${CMAKE_OSX_ARCHITECTURES}")
	set(CMAKE_MACOSX_RPATH 1)
endif()
if(WIN32)
	set(LINPHONE_BUILDER_CPPFLAGS "-D_WIN32_WINNT=0x0501 -D_ALLOW_KEYWORD_MACROS")
endif()

# Adjust PKG_CONFIG_PATH to include install directory
if(UNIX)
	set(LINPHONE_BUILDER_PKG_CONFIG_PATH "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/:$ENV{PKG_CONFIG_PATH}:/usr/lib/pkgconfig/:/usr/lib/x86_64-linux-gnu/pkgconfig/:/usr/share/pkgconfig/:/usr/local/lib/pkgconfig/:/opt/local/lib/pkgconfig/")
else() # Windows
	set(LINPHONE_BUILDER_PKG_CONFIG_PATH "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/")
endif()


list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" _IS_SYSTEM_DIR)
if("${_IS_SYSTEM_DIR}" STREQUAL "-1")
   set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()


# Include builders
include(builders/CMakeLists.txt)

# linphone
if(WIN32)
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_RELATIVE_PREFIX=YES")
else(APPLE)
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_RELATIVE_PREFIX=${ENABLE_RELATIVE_PREFIX}")
endif()

# ms2
if(WIN32)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_RELATIVE_PREFIX=YES")
else(APPLE)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_RELATIVE_PREFIX=${ENABLE_RELATIVE_PREFIX}")
endif()

# opencoreamr
if(NOT WIN32)
	set(EP_opencoreamr_EXTRA_CFLAGS "${EP_opencoreamr_EXTRA_CFLAGS} -fPIC")
	set(EP_opencoreamr_EXTRA_CXXFLAGS "${EP_opencoreamr_EXTRA_CXXFLAGS} -fPIC")
	set(EP_opencoreamr_EXTRA_LDFLAGS "${EP_opencoreamr_EXTRA_LDFLAGS} -fPIC")
endif()

# openh264
set(EP_openh264_LINKING_TYPE "-shared")

# voamrwbenc
if(NOT WIN32)
	set(EP_voamrwbenc_EXTRA_CFLAGS "${EP_voamrwbenc_EXTRA_CFLAGS} -fPIC")
	set(EP_voamrwbenc_EXTRA_CXXFLAGS "${EP_voamrwbenc_EXTRA_CXXFLAGS} -fPIC")
	set(EP_voamrwbenc_EXTRA_LDFLAGS "${EP_voamrwbenc_EXTRA_LDFLAGS} -fPIC")
endif()

# vpx
if(WIN32)
	set(EP_vpx_LINKING_TYPE "--enable-static" "--disable-shared" "--enable-pic")
endif()

# Create a shortcut to linphone.exe in install prefix
if(LINPHONE_BUILDER_TARGET STREQUAL linphone AND WIN32)
	set(SHORTCUT_PATH "${CMAKE_INSTALL_PREFIX}/linphone.lnk")
	set(SHORTCUT_TARGET_PATH "${CMAKE_INSTALL_PREFIX}/bin/linphone.exe")
	set(SHORTCUT_WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/configs/desktop/linphone_package/winshortcut.vbs.in" "${CMAKE_CURRENT_BINARY_DIR}/winshortcut.vbs" @ONLY)
	add_custom_command(OUTPUT "${SHORTCUT_PATH}"
		COMMAND "cscript" "${CMAKE_CURRENT_BINARY_DIR}/winshortcut.vbs"
	)
	add_custom_target(linphone_winshortcut ALL DEPENDS "${SHORTCUT_PATH}" TARGET_linphone)
endif()


# Packaging
if (ENABLE_PACKAGING)
	# Linphone and linphone SDK packages
	if(LINPHONE_BUILDER_TARGET STREQUAL linphone)
		linphone_builder_apply_flags()
		linphone_builder_set_ep_directories(linphone_package)
		linphone_builder_expand_external_project_vars()
		ExternalProject_Add(TARGET_linphone_package
			DEPENDS TARGET_linphone_builder
			TMP_DIR ${ep_tmp}
			BINARY_DIR ${ep_build}
			DOWNLOAD_COMMAND ""
			PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "${CMAKE_CURRENT_LIST_DIR}/desktop/linphone_package" "<SOURCE_DIR>"
			CMAKE_GENERATOR ${CMAKE_GENERATOR}
			CMAKE_ARGS ${LINPHONE_BUILDER_EP_ARGS} -DCMAKE_INSTALL_PREFIX=${LINPHONE_BUILDER_WORK_DIR}/PACKAGE -DLINPHONE_OUTPUT_DIR=${CMAKE_INSTALL_PREFIX} -DENABLE_ZRTP:BOOL=${ENABLE_ZRTP}
		)
	endif()

	# Mediastreamer SDK packages
	if(LINPHONE_BUILDER_TARGET STREQUAL ms2
			OR LINPHONE_BUILDER_TARGET STREQUAL ms2-plugins)

		if(LINPHONE_BUILDER_TARGET STREQUAL ms2)
			set(MS2_PACKAGE_DEPEND_TARGET TARGET_ms2)
		else()
			set(MS2_PACKAGE_DEPEND_TARGET TARGET_ms2plugins)
		endif()
		linphone_builder_apply_flags()
		linphone_builder_set_ep_directories(ms2_package)
		linphone_builder_expand_external_project_vars()
		ExternalProject_Add(TARGET_ms2_package
			DEPENDS ${MS2_PACKAGE_DEPEND_TARGET}
			TMP_DIR ${ep_tmp}
			BINARY_DIR ${ep_build}
			DOWNLOAD_COMMAND ""
			PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "${CMAKE_CURRENT_LIST_DIR}/desktop/ms2_package" "<SOURCE_DIR>"
			CMAKE_GENERATOR ${CMAKE_GENERATOR}
			CMAKE_ARGS ${LINPHONE_BUILDER_EP_ARGS} -DCMAKE_INSTALL_PREFIX=${LINPHONE_BUILDER_WORK_DIR}/PACKAGE -DLINPHONE_OUTPUT_DIR=${CMAKE_INSTALL_PREFIX} -DENABLE_ZRTP:BOOL=${ENABLE_ZRTP}
		)
	endif()
endif()
