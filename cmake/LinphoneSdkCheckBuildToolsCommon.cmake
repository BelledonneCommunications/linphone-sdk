############################################################################
# LinphoneSDKCheckBuildToolsCommon.cmake
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

find_package(PythonInterp 3 REQUIRED)

linphone_sdk_check_git()

if(ENABLE_CSHARP_WRAPPER OR ENABLE_CXX_WRAPPER OR ENABLE_DOC OR ENABLE_JAVA_WRAPPER)
	linphone_sdk_check_is_installed(doxygen)
	linphone_sdk_check_python_module_is_installed(pystache)
	linphone_sdk_check_python_module_is_installed(six)
endif()
if(ENABLE_OPENH264)
	linphone_sdk_check_is_installed(nasm)
	if(APPLE)
		execute_process(
			COMMAND "${NASM_PROGRAM}" "-f" "elf32"
			ERROR_VARIABLE _nasm_error
		)
		if(_nasm_error MATCHES "fatal: unrecognised output format")
			message(FATAL_ERROR "Invalid version of nasm detected. Please make sure that you are NOT using Apple's binary here")
		endif()
	endif()
endif()
if(ENABLE_VPX)
	linphone_sdk_check_is_installed(yasm)
endif()


set(CMAKE_PROGRAM_PATH "${CMAKE_BINARY_DIR}/programs")
file(MAKE_DIRECTORY ${CMAKE_PROGRAM_PATH})
file(COPY "${CMAKE_CURRENT_LIST_DIR}/scripts/gas-preprocessor.pl" DESTINATION "${CMAKE_PROGRAM_PATH}")
