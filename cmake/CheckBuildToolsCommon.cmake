############################################################################
# CheckBuildToolsCommon.cmake
# Copyright (C) 2015-2023  Belledonne Communications, Grenoble France
#
############################################################################
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#
################################################################################

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" OR CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	set(WINDOWS_UNIVERSAL TRUE)
else()
	set(WINDOWS_UNIVERSAL FALSE)
endif()
if(WIN32)
	#Internal variable where to search MSYS2 programs
	set(_DEFAULT_MSYS2_BIN_PATH "C:/msys64/usr/bin")
endif()
	

if(WIN32)
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set(MINGW_PACKAGE_PREFIX "mingw-w64-x86_64-")
		set(MINGW_TYPE "mingw64")
	else()
		set(MINGW_PACKAGE_PREFIX "mingw-w64-i686-")
		set(MINGW_TYPE "mingw32")
	endif()
	find_program(MSYS2_PROGRAM
		NAMES msys2_shell.cmd
		HINTS "C:/msys64/"
	)
	if(NOT MSYS2_PROGRAM)
		message(FATAL_ERROR "Could not find MSYS2 for MinGW! Make sure to have msys2_shell.cmd in your PATH. The default folder is 'C:/msys64/'")
	endif()
endif()

if(MSVC AND NOT WINDOWS_UNIVERSAL)
	find_program(SH_PROGRAM
		NAMES sh.exe
		HINTS ${_DEFAULT_MSYS2_BIN_PATH}
	)
	if(NOT SH_PROGRAM)
		message(FATAL_ERROR "Could not find sh for MinGW! Make sure to have sh.exe in your PATH. The default folder is : '${_DEFAULT_MSYS2_BIN_PATH}'")
	endif()
endif()

set(CMAKE_PROGRAM_PATH "${CMAKE_BINARY_DIR}/programs")
string(REGEX REPLACE "^([a-zA-Z]):(.*)$" "/\\1\\2" AUTOTOOLS_PROGRAM_PATH "${CMAKE_PROGRAM_PATH}")
string(REPLACE "\\" "/" AUTOTOOLS_PROGRAM_PATH ${AUTOTOOLS_PROGRAM_PATH})
file(MAKE_DIRECTORY ${CMAKE_PROGRAM_PATH})
file(COPY "${CMAKE_CURRENT_LIST_DIR}/../scripts/gas-preprocessor.pl" DESTINATION "${CMAKE_PROGRAM_PATH}")
if(WIN32)
	find_program(7Z_PROGRAM 7z.exe REQUIRED)
	
	if(ENABLE_WINDOWS_TOOLS_CHECK)
		set(CHECK_WINDOWS_TOOLS_STATUS "1" CACHE INTERNAL "for internal use only; do not modify")
		if (NOT CHECK_WINDOWS_TOOLS_STATUS EQUAL 0)
			message(STATUS "Installing windows tools : toolchains, make, perl, yasm, gawk, bzip2, nasm, sed, python, doxygen, graphviz")
			execute_process(
				COMMAND "${MSYS2_PROGRAM}" "-${MINGW_TYPE}" "-here" "-full-path" "-defterm" "-shell" "sh" "-l" "-c" "pacman -Sy base-devel ${MINGW_PACKAGE_PREFIX}toolchain ${MINGW_PACKAGE_PREFIX}python make perl ${MINGW_PACKAGE_PREFIX}yasm bzip2 nasm ${MINGW_PACKAGE_PREFIX}doxygen gawk sed ${MINGW_PACKAGE_PREFIX}graphviz --noconfirm --needed"
				RESULT_VARIABLE EXECUTE_STATUS
			)
			set(CHECK_WINDOWS_TOOLS_STATUS ${EXECUTE_STATUS} CACHE INTERNAL "for internal use only; do not modify" FORCE)
		else()
			message(STATUS "Windows tools already checked: toolchains, make, perl, yasm, gawk, bzip2, nasm, sed, python, doxygen, graphviz")
		endif()
	
		set(CHECK_WINDOWS_TOOLS_PYTHON_STATUS "1" CACHE INTERNAL "for internal use only; do not modify")
		if (NOT CHECK_WINDOWS_TOOLS_PYTHON_STATUS EQUAL 0)
			message(STATUS "Installing windows tools : python modules")
			execute_process(
				COMMAND "${MSYS2_PROGRAM}" "-${MINGW_TYPE}" "-here" "-full-path" "-defterm" "-shell" "sh" "-l" "-c" "python -m ensurepip ; python -m pip install six pystache"
				RESULT_VARIABLE EXECUTE_STATUS
			)
			set(CHECK_WINDOWS_TOOLS_PYTHON_STATUS ${EXECUTE_STATUS} CACHE INTERNAL "for internal use only; do not modify" FORCE)
		else()
			message(STATUS "Windows tools already checked : python modules")
		endif()
	
		if(ENABLE_LDAP)
			set(CHECK_WINDOWS_TOOLS_LDAP_STATUS "1" CACHE INTERNAL "for internal use only; do not modify")
			if (NOT CHECK_WINDOWS_TOOLS_LDAP_STATUS EQUAL 0)
				message(STATUS "Installing windows tools for LDAP : posix regex (libsystre)")
				execute_process(
					COMMAND "${MSYS2_PROGRAM}" "-${MINGW_TYPE}" "-here" "-full-path" "-defterm" "-shell" "sh" "-l" "-c" "pacman -S ${MINGW_PACKAGE_PREFIX}libsystre --noconfirm  --needed"
					RESULT_VARIABLE EXECUTE_STATUS
				)
				set(CHECK_WINDOWS_TOOLS_LDAP_STATUS ${EXECUTE_STATUS} CACHE INTERNAL "for internal use only; do not modify" FORCE)
			else()
				message(STATUS "Windows tools for LDAP already checked: posix regex (libsystre)")
			endif()
		endif()

		if(ENABLE_AV1)
			set(CHECK_WINDOWS_AV1_TOOLS_STATUS "1" CACHE INTERNAL "for internal use only; do not modify")
			if (NOT CHECK_WINDOWS_AV1_TOOLS_STATUS EQUAL 0)
				message(STATUS "Installing windows tools for AV1 : meson, ninja")
				execute_process(
						COMMAND "${MSYS2_PROGRAM}" "-${MINGW_TYPE}" "-here" "-full-path" "-defterm" "-shell" "sh" "-l" "-c" "pacman -Sy base-devel ${MINGW_PACKAGE_PREFIX}meson ${MINGW_PACKAGE_PREFIX}ninja --noconfirm --needed"
						RESULT_VARIABLE EXECUTE_STATUS
				)
				set(CHECK_WINDOWS_AV1_TOOLS_STATUS ${EXECUTE_STATUS} CACHE INTERNAL "for internal use only; do not modify" FORCE)
			else()
				message(STATUS "Windows tools for AV1 already checked: meson, ninja")
			endif()
		endif()
	endif()
endif()

find_package(PythonInterp 3 REQUIRED)

if(WIN32)
	#Should be already installed from MSYS2
	find_program(SED_PROGRAM
		NAMES  sed sed.exe
		HINTS ${_DEFAULT_MSYS2_BIN_PATH}
		CMAKE_FIND_ROOT_PATH_BOTH
	)
else()
	find_program(SED_PROGRAM
		NAMES  sed sed.exe
		CMAKE_FIND_ROOT_PATH_BOTH
	)
endif()

if(NOT SED_PROGRAM)
		message(FATAL_ERROR "Could not find the sed program.")
endif()


if(NOT WINDOWS_UNIVERSAL)
	if(ENABLE_NLS)
		find_program(INTLTOOLIZE_PROGRAM
			NAMES intltoolize
			HINTS ${_DEFAULT_MSYS2_BIN_PATH}
		)
		if(NOT INTLTOOLIZE_PROGRAM)
			if(WIN32 AND ENABLE_WINDOWS_TOOLS_CHECK)
				set(CHECK_WINDOWS_TOOLS_INTLTOOLIZE_STATUS "1" CACHE INTERNAL "for internal use only; do not modify")
				if (NOT CHECK_WINDOWS_TOOLS_INTLTOOLIZE_STATUS EQUAL 0)
					message(STATUS "Installing intltoolize to MSYS2")
					execute_process(
						COMMAND "${MSYS2_PROGRAM}" "-${MINGW_TYPE}" "-here" "-full-path" "-no-start" "-defterm" "-shell" "sh" "-l" "-c"
						"pacman -Sy intltool --noconfirm --needed"
						RESULT_VARIABLE EXECUTE_STATUS
					)
					set(CHECK_WINDOWS_TOOLS_INTLTOOLIZE_STATUS ${EXECUTE_STATUS} CACHE INTERNAL "for internal use only; do not modify" FORCE)
				else()
					message(STATUS "intltoolize to MSYS2 already checked")
				endif()
			endif()
			find_program(INTLTOOLIZE_PROGRAM
				NAMES intltoolize
				HINTS ${_DEFAULT_MSYS2_BIN_PATH}
			)
		endif()

		if(NOT INTLTOOLIZE_PROGRAM AND NOT MSVC)
			message(FATAL_ERROR "Could not find the intltoolize program.")
		endif()
	endif()
endif()



find_package(PythonInterp 3 REQUIRED)

linphone_sdk_check_git()

if(ENABLE_CSHARP_WRAPPER OR ENABLE_CXX_WRAPPER OR ENABLE_DOC OR ENABLE_JAVA_WRAPPER OR ENABLE_PYTHON_WRAPPER)
	linphone_sdk_check_is_installed(doxygen)
	linphone_sdk_check_python_module_is_installed(pystache)
	linphone_sdk_check_python_module_is_installed(six)

	if (ENABLE_PYTHON_WRAPPER)
		linphone_sdk_check_python_module_is_installed(cython)

		if (ENABLE_DOC)
			linphone_sdk_check_python_module_is_installed(pdoc)
		endif()
	endif()

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

if(ENABLE_VPX OR ENABLE_AV1)
	linphone_sdk_check_is_installed(yasm)
endif()

if(ENABLE_AV1)
	linphone_sdk_check_is_installed(perl)
	linphone_sdk_check_is_installed(meson)
	linphone_sdk_check_is_installed(ninja)
endif()
