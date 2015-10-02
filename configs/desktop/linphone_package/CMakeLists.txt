############################################################################
# CMakeLists.txt
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

cmake_minimum_required(VERSION 3.0)
project(LINPHONE_PACKAGE) # Dummy project

find_package(Linphone REQUIRED)
find_package(Git)

set(LINPHONE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../EP_linphone")
set(LINPHONE_BUILD_DIR "${CMAKE_BINARY_DIR}/../linphone")

if(GIT_EXECUTABLE)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --always
		OUTPUT_VARIABLE LINPHONE_GIT_REVISION
		OUTPUT_STRIP_TRAILING_WHITESPACE
		WORKING_DIRECTORY ${LINPHONE_SOURCE_DIR}
	)
else()
	set(LINPHONE_GIT_REVISION "0.0.0")
endif()
string(REGEX REPLACE "([0-9.]+)-?.*" "\\1" LINPHONE_VERSION "${LINPHONE_GIT_REVISION}")
string(REPLACE "." ";" SPLITTED_LINPHONE_VERSION ${LINPHONE_VERSION})
list(GET SPLITTED_LINPHONE_VERSION 0 LINPHONE_MAJOR_VERSION)
list(GET SPLITTED_LINPHONE_VERSION 1 LINPHONE_MINOR_VERSION)
list(GET SPLITTED_LINPHONE_VERSION 2 LINPHONE_MICRO_VERSION)

# Translates absolute internal dependency path of FFMepeg libraries into relative path on MacOSX
if(APPLE)
	set(LINPHONE_SDK_TMP ${CMAKE_CURRENT_BINARY_DIR}/linphone-sdk-tmp)
	file(REMOVE_RECURSE ${LINPHONE_SDK_TMP})
	file(COPY ${LINPHONE_OUTPUT_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/OUTPUT ${LINPHONE_SDK_TMP})
	set(RPATH_CHANGE_CMDS
                -change ${LINPHONE_OUTPUT_DIR}/lib/libavcodec.53.61.100.dylib @rpath/libavcodec.53.61.100.dylib
                -change ${LINPHONE_OUTPUT_DIR}/lib/libavutil.51.35.100.dylib @rpath/libavutil.51.35.100.dylib
                -change ${LINPHONE_OUTPUT_DIR}/lib/libswscale.2.1.100.dylib @rpath/libswscale.2.1.100.dylib
                -change ${LINPHONE_OUTPUT_DIR}/lib/libswresample.0.6.100.dylib @rpath/libswresample.0.6.100.dylib
		-change ${LINPHONE_OUTPUT_DIR}/lib/libopenh264.0.dylib @rpath/libopenh264.0.dylib
		-change /opt/local/lib/libintl.8.dylib @rpath/libintl.8.dylib
		-change /opt/local/lib/libiconv.2.dylib @rpath/libiconv.2.dylib
		-change /lib/libopenh264.0.dylib @rpath/libopenh264.0.dylib
        )
        set(RPATH_LIBS_TO_MODIFY
                libavcodec.53.61.100.dylib
                libavutil.51.35.100.dylib
                liblinphone.7.dylib
                libmediastreamer_voip.5.dylib
                libswresample.0.6.100.dylib
                libswscale.2.1.100.dylib
		mediastreamer/plugins/libmsamr.dylib
		mediastreamer/plugins/libmsbcg729.dylib
		mediastreamer/plugins/libmsilbc.dylib
		mediastreamer/plugins/libmsopenh264.dylib
		mediastreamer/plugins/libmssilk.dylib
		mediastreamer/plugins/libmswebrtc.dylib
        )
        foreach(LIB_TO_CHANGE ${RPATH_LIBS_TO_MODIFY})
                execute_process(
			COMMAND install_name_tool ${RPATH_CHANGE_CMDS} ${LINPHONE_SDK_TMP}/lib/${LIB_TO_CHANGE}
			COMMAND install_name_tool -id @rpath/${LIB_TO_CHANGE} ${LINPHONE_SDK_TMP}/lib/${LIB_TO_CHANGE}
			COMMAND install_name_tool -delete_rpath ${LINPHONE_OUTPUT_DIR}/lib ${LINPHONE_SDK_TMP}/lib/${LIB_TO_CHANGE}
                )
        endforeach()
	file(COPY /opt/local/lib/libintl.8.dylib /opt/local/lib/libintl.dylib /opt/local/lib/libiconv.dylib /opt/local/lib/libiconv.2.dylib DESTINATION ${LINPHONE_SDK_TMP}/lib)
	execute_process(
		COMMAND install_name_tool -id @rpath/libintl.8.dylib -change /opt/local/lib/libiconv.2.dylib @rpath/libiconv.2.dylib ${LINPHONE_SDK_TMP}/lib/libintl.8.dylib
		COMMAND install_name_tool -id @rpath/libiconv.2.dylib ${LINPHONE_SDK_TMP}/lib/libiconv.2.dylib
	)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/README.openh264.txt DESTINATION ${LINPHONE_SDK_TMP})
endif()

# Generates a zip archive containing the development files of liblinphone
if(WIN32)
	find_program(7Z_PROGRAM 7z.exe)
	execute_process(
		COMMAND ${7Z_PROGRAM} a -tzip ${CMAKE_BINARY_DIR}/linphone-sdk-${LINPHONE_GIT_REVISION}-win32.zip @${CMAKE_SOURCE_DIR}/linphone-sdk.list
		WORKING_DIRECTORY ${LINPHONE_OUTPUT_DIR}
	)
	execute_process(
		COMMAND ${7Z_PROGRAM} a -tzip ${CMAKE_BINARY_DIR}/linphone-sdk-${LINPHONE_GIT_REVISION}-win32.zip ${CMAKE_SOURCE_DIR}/README.openh264.txt
	)
elseif(APPLE)
	execute_process(
		COMMAND rm -rf ${CMAKE_BINARY_DIR}/linphone-sdk-${LINPHONE_GIT_REVISION}-mac.zip
		COMMAND zip -ry ${CMAKE_BINARY_DIR}/linphone-sdk-${LINPHONE_GIT_REVISION}-mac.zip . -i @${CMAKE_SOURCE_DIR}/linphone-sdk-mac.list
		WORKING_DIRECTORY ${LINPHONE_SDK_TMP}
	)
endif()

if(WIN32)
	install(DIRECTORY "${LINPHONE_OUTPUT_DIR}/"
		DESTINATION "."
		COMPONENT "linphone_base"
		PATTERN "doc" EXCLUDE
		PATTERN "etc" EXCLUDE
		PATTERN "include" EXCLUDE
		PATTERN "lib/cmake" EXCLUDE
		PATTERN "lib/gdk-pixbuf-2.0" EXCLUDE
		PATTERN "lib/glib-2.0" EXCLUDE
		PATTERN "lib/mediastreamer" EXCLUDE
		PATTERN "lib/pkgconfig" EXCLUDE
		PATTERN "lib/*.a" EXCLUDE
		PATTERN "lib/*.def" EXCLUDE
		PATTERN "lib/*.exp" EXCLUDE
		PATTERN "lib/*.la" EXCLUDE
		PATTERN "lib/*.lib" EXCLUDE
		PATTERN "man" EXCLUDE
		PATTERN "manifest" EXCLUDE
		PATTERN "share/aclocal" EXCLUDE
		PATTERN "share/CUnit" EXCLUDE
		PATTERN "share/glib-2.0" EXCLUDE
		PATTERN "share/gtk-2.0" EXCLUDE
		PATTERN "share/gtk-doc" EXCLUDE
		PATTERN "share/intltool" EXCLUDE
		PATTERN "share/java" EXCLUDE
		PATTERN "src" EXCLUDE
		PATTERN "gtk+-bundle_*" EXCLUDE
		PATTERN "linphone.lnk" EXCLUDE
		PATTERN "bin/envsubst.exe" EXCLUDE
		PATTERN "bin/fc-*" EXCLUDE
		PATTERN "bin/freetype-config" EXCLUDE
		PATTERN "bin/g*.exe" EXCLUDE
		PATTERN "bin/gettext.sh" EXCLUDE
		PATTERN "bin/glib*" EXCLUDE
		PATTERN "bin/gtk-*" EXCLUDE
		PATTERN "bin/intltool*" EXCLUDE
		PATTERN "bin/ngettext.exe" EXCLUDE
		PATTERN "bin/pango*.exe" EXCLUDE
		PATTERN "bin/pkg-config.exe" EXCLUDE
		PATTERN "bin/xmlwf.exe" EXCLUDE
		PATTERN "bin/openh264.dll" EXCLUDE
	)

	install(DIRECTORY "${LINPHONE_OUTPUT_DIR}/lib/mediastreamer/plugins/"
		DESTINATION "lib/mediastreamer/plugins"
		COMPONENT "msopenh264"
		FILES_MATCHING PATTERN "libmsopenh264.*"
	)
	
	install(DIRECTORY "${LINPHONE_OUTPUT_DIR}/lib/mediastreamer/plugins/"
		DESTINATION "lib/mediastreamer/plugins"
		FILES_MATCHING PATTERN "libmssilk.*" PATTERN "libmsilbc.*"
	)
elseif(APPLE)
	find_program(MAC_BUNDLER gtk-mac-bundler)
	configure_file(mac-bundler/Info-linphone.plist.in mac-bundler/Info-linphone.plist @ONLY)
	configure_file(mac-bundler/call-bundler.cmake.in mac-bundler/call-bundler.cmake)
	configure_file(mac-bundler/environment.sh mac-bundler/environment.sh COPYONLY)
	configure_file(mac-bundler/gtkrc mac-bundler/gtkrc COPYONLY)
	configure_file(mac-bundler/linphone.bundle mac-bundler/linphone.bundle COPYONLY)
	configure_file(mac-bundler/linphone.icns mac-bundler/linphone.icns COPYONLY)
	install(SCRIPT ${CMAKE_BINARY_DIR}/mac-bundler/call-bundler.cmake)
endif()

if(MSVC)
	install(DIRECTORY "${LINPHONE_OUTPUT_DIR}/lib/mediastreamer/plugins/"
		DESTINATION "lib/mediastreamer/plugins"
		FILES_MATCHING PATTERN "libmswasapi.dll"
	)
	string(REGEX REPLACE "Visual Studio ([0-9]+).*" "\\1" MSVC_VERSION "${CMAKE_GENERATOR}")
	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		set(MSVC_DEBUG_SYSTEM_LIBRARIES "d")
	endif()
	find_file(MSVCP_LIB "msvcp${MSVC_VERSION}0${MSVC_DEBUG_SYSTEM_LIBRARIES}.dll" PATHS "C:/Windows/System32")
	find_file(MSVCR_LIB "msvcr${MSVC_VERSION}0${MSVC_DEBUG_SYSTEM_LIBRARIES}.dll" PATHS "C:/Windows/System32")
	install(FILES ${MSVCP_LIB} ${MSVCR_LIB} DESTINATION "bin")
endif()

set(CPACK_PACKAGE_NAME "Linphone")
set(CPACK_PACKAGE_VENDOR "Belledonne communications")
set(CPACK_PACKAGE_VERSION_MAJOR ${LINPHONE_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${LINPHONE_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${LINPHONE_MICRO_VERSION})
set(CPACK_PACKAGE_EXECUTABLES "linphone;Linphone")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Linphone")
set(CPACK_RESOURCE_FILE_LICENSE "${LINPHONE_SOURCE_DIR}/COPYING")

if(WIN32)
	set(CPACK_COMPONENT_LINPHONE_BASE_DISPLAY_NAME "Linphone")
	set(CPACK_COMPONENT_LINPHONE_BASE_DESCRIPTION "Minimal installation of Linphone")
	set(CPACK_COMPONENT_LINPHONE_BASE_REQUIRED True)
	set(CPACK_COMPONENT_MSOPENH264_DISPLAY_NAME "Cisco's OpenH264 codec")
	set(CPACK_COMPONENT_MSOPENH264_DESCRIPTION "Download OpenH264 Video Codec provided by Cisco Systems, Inc.")
	set(CPACK_COMPONENT_MSOPENH264_DISABLED True)
endif()

if(WIN32)
	set(CPACK_GENERATOR "NSIS")
	set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\linphone-banner.bmp")
	set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${LINPHONE_GIT_REVISION}-win32")
	set(CPACK_NSIS_MUI_ICON "${LINPHONE_SOURCE_DIR}/gtk/linphone.ico")
	set(CPACK_NSIS_MUI_UNIICON "${LINPHONE_SOURCE_DIR}/gtk/linphone.ico")
	set(CPACK_NSIS_DISPLAY_NAME "Linphone")
	set(CPACK_NSIS_PACKAGE_NAME "Linphone ${LINPHONE_MAJOR_VERSION}.${LINPHONE_MINOR_VERSION}.${LINPHONE_MICRO_VERSION}")
	set(CPACK_NSIS_URL_INFO_ABOUT "http://www.linphone.org/")
	set(CPACK_NSIS_MUI_FINISHPAGE_RUN "linphone")
	set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL "ON")
	
	file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" DOS_STYLE_SOURCE_DIR)
	string(REPLACE "\\" "\\\\" ESCAPED_DOS_STYLE_SOURCE_DIR "${DOS_STYLE_SOURCE_DIR}")
	
	# Add NSI instructions to download the openh264 codec from Cisco's servers whether msopenh264 has been installed
	set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "!include \\\"${ESCAPED_DOS_STYLE_SOURCE_DIR}\\\\openh264-install.nsi\\\"")
	
	# Add NSI instructions to delete the openh264 codecs while uninstalling Linphone
	set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "!include \\\"${ESCAPED_DOS_STYLE_SOURCE_DIR}\\\\openh264-uninstall.nsi\\\"")

	# Sign the installer
	set(TIMESTAMP_URL "http://timestamp.verisign.com/scripts/timestamp.dll")
	set(PFX_FILE "${CMAKE_CURRENT_SOURCE_DIR}/sign/linphone.pfx")
	set(PASSPHRASE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/sign/passphrase.txt")
	get_filename_component(WINSDK_DIR "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows;CurrentInstallFolder]" REALPATH CACHE)
	find_package(PythonInterp)
	find_program(SIGNTOOL signtool PATHS ${WINSDK_DIR}/bin)
	set(SIGNTOOL_COMMAND "${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/signtool.py")
	set(PERFORM_SIGNING 0)
	if(EXISTS ${PFX_FILE})
		if(SIGNTOOL)
			set(SIGNTOOL_COMMAND "${SIGNTOOL_COMMAND} signtool sign /f ${PFX_FILE}")
			set(SIGNTOOL_COMMAND "${SIGNTOOL_COMMAND} /p ${PASSPHRASE_FILE}")
			set(SIGNTOOL_COMMAND "${SIGNTOOL_COMMAND} /t ${TIMESTAMP_URL}")
			message("Found signtool and certificate ${PFX_FILE}")
			set(PERFORM_SIGNING 1)
		else()
			message(STATUS "Could not find signtool! Code signing disabled (${SIGNTOOL})")
		endif()
	else()
		message(STATUS "No signtool certificate found; assuming development machine (${PFX_FILE})")
	endif()
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/WindowsPackaging.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/WindowsPackaging.cmake" @ONLY)
	install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/WindowsPackaging.cmake")
endif()
include(CPack)
