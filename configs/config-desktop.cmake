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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

# Define default values for the linphone builder options
set(DEFAULT_VALUE_ENABLE_BV16 ON)
set(DEFAULT_VALUE_ENABLE_FFMPEG ON)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_GTK_UI ON)
set(DEFAULT_VALUE_ENABLE_JPEG ON)
set(DEFAULT_VALUE_ENABLE_MBEDTLS ON)
set(DEFAULT_VALUE_ENABLE_MKV ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS ON)
set(DEFAULT_VALUE_ENABLE_VCARD ON)
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_VPX ON)
set(DEFAULT_VALUE_ENABLE_WASAPI ON)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)
set(DEFAULT_VALUE_ENABLE_TOOLS OFF)


include(configs/config-desktop-common.cmake)


# Install GTK and intltool for build with Visual Studio
if(MSVC)
	if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/intltool_win32.zip")
		message(STATUS "Installing intltool")
		file(DOWNLOAD http://ftp.acc.umu.se/pub/GNOME/binaries/win32/intltool/0.40/intltool_0.40.4-1_win32.zip "${CMAKE_CURRENT_BINARY_DIR}/intltool_win32.zip" SHOW_PROGRESS)
		execute_process(
			COMMAND "${CMAKE_COMMAND}" "-E" "tar" "x" "${CMAKE_CURRENT_BINARY_DIR}/intltool_win32.zip"
			WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
		)
	endif()
	if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/gtk+-bundle_win32.zip")
		message(STATUS "Installing GTK")
		file(DOWNLOAD http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.24/gtk+-bundle_2.24.10-20120208_win32.zip "${CMAKE_CURRENT_BINARY_DIR}/gtk+-bundle_win32.zip" SHOW_PROGRESS)
		execute_process(
			COMMAND "${CMAKE_COMMAND}" "-E" "tar" "x" "${CMAKE_CURRENT_BINARY_DIR}/gtk+-bundle_win32.zip"
			WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
		)
	endif()
endif()


# Add config step for packaging
set(LINPHONE_BUILDER_ADDITIONAL_CONFIG_STEPS "${CMAKE_CURRENT_LIST_DIR}/desktop/additional_steps.cmake")
