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

find_program(NASM_PROGRAM
	NAMES nasm nasm.exe
)
if(NOT NASM_PROGRAM)
	if(WIN32)
		message(FATAL_ERROR "Could not find the nasm.exe program. Please install it from http://www.nasm.us/")
	else()
		message(FATAL_ERROR "Could not find the nasm program.")
	endif()
endif()

set(EP_openh264_GIT_REPOSITORY "https://github.com/cisco/openh264")
set(EP_openh264_GIT_TAG "v1.0.0")
set(EP_openh264_BUILD_METHOD "custom")
set(EP_openh264_BUILD_IN_SOURCE "yes")
set(EP_openh264_LINKING_TYPE "-shared")
set(EP_openh264_BUILD_TYPE "Release")	# Always use Release build type, otherwise the codec is too slow...
set(EP_openh264_PATCH_COMMAND "${PATCH_PROGRAM}" "-p1" "-i" "${CMAKE_CURRENT_SOURCE_DIR}/builders/openh264/permissive.patch")
set(EP_openh264_CONFIGURE_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/openh264/configure.sh.cmake)
set(EP_openh264_BUILD_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/openh264/build.sh.cmake)
set(EP_openh264_INSTALL_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/openh264/install.sh.cmake)
