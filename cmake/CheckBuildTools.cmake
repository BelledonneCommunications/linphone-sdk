############################################################################
# CheckBuildTools.txt
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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

find_package(PythonInterp)
if(NOT PYTHONINTERP_FOUND)
	message(FATAL_ERROR "Could not find python!")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" OR CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	set(WINDOWS_UNIVERSAL TRUE)
else()
	set(WINDOWS_UNIVERSAL FALSE)
endif()

if(MSVC AND NOT WINDOWS_UNIVERSAL)
	find_program(SH_PROGRAM
		NAMES sh.exe
		HINTS "C:/MinGW/msys/1.0/bin"
	)
	if(NOT SH_PROGRAM)
		message(FATAL_ERROR "Could not find MinGW!")
	endif()

	find_file(GCC_LIBRARY
		NAMES libgcc.a
		HINTS "C:/MinGW/lib/gcc/mingw32/*"
	)
	execute_process(COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${GCC_LIBRARY}" "${CMAKE_INSTALL_PREFIX}/lib/gcc.lib")
	find_file(MINGWEX_LIBRARY
		NAMES libmingwex.a
		HINTS "C:/MinGW/lib"
	)
	execute_process(COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${MINGWEX_LIBRARY}" "${CMAKE_INSTALL_PREFIX}/lib/mingwex.lib")
endif()

set(CMAKE_PROGRAM_PATH "${CMAKE_BINARY_DIR}/programs")
string(REGEX REPLACE "^([a-zA-Z]):(.*)$" "/\\1\\2" AUTOTOOLS_PROGRAM_PATH "${CMAKE_PROGRAM_PATH}")
string(REPLACE "\\" "/" AUTOTOOLS_PROGRAM_PATH ${AUTOTOOLS_PROGRAM_PATH})
file(MAKE_DIRECTORY ${CMAKE_PROGRAM_PATH})
file(COPY "${CMAKE_CURRENT_LIST_DIR}/../scripts/gas-preprocessor.pl" DESTINATION "${CMAKE_PROGRAM_PATH}")
if(WIN32)
	if(NOT EXISTS "${CMAKE_BINARY_DIR}/linphone_builder_windows_tools.zip")
		message(STATUS "Installing windows tools")
		file(DOWNLOAD https://www.linphone.org/files/linphone_builder_windows_tools.zip "${CMAKE_BINARY_DIR}/linphone_builder_windows_tools.zip")
		execute_process(
			COMMAND "${CMAKE_COMMAND}" "-E" "tar" "x" "${CMAKE_BINARY_DIR}/linphone_builder_windows_tools.zip"
			WORKING_DIRECTORY ${CMAKE_PROGRAM_PATH}
		)
		if(NOT WINDOWS_UNIVERSAL)
			if(CMAKE_SIZEOF_VOID_P EQUAL 8)
				file(RENAME "${CMAKE_PROGRAM_PATH}/yasm-1.3.0-win64.exe" "${CMAKE_PROGRAM_PATH}/yasm.exe")
				file(REMOVE "${CMAKE_PROGRAM_PATH}/yasm-1.3.0-win32.exe")
			else()
				file(RENAME "${CMAKE_PROGRAM_PATH}/yasm-1.3.0-win32.exe" "${CMAKE_PROGRAM_PATH}/yasm.exe")
				file(REMOVE "${CMAKE_PROGRAM_PATH}/yasm-1.3.0-win64.exe")
			endif()
		endif()
	endif()
endif()

find_program(PATCH_PROGRAM
	NAMES patch patch.exe
)
if(NOT PATCH_PROGRAM)
	if(WIN32)
		message(FATAL_ERROR "Could not find the patch.exe program. Please install it from http://gnuwin32.sourceforge.net/packages/patch.htm")
	else()
		message(FATAL_ERROR "Could not find the patch program.")
	endif()
endif()

find_program(SED_PROGRAM
	NAMES sed sed.exe
)
if(NOT SED_PROGRAM)
	if(WIN32)
		message(FATAL_ERROR "Could not find the sed.exe program. Please install it from http://gnuwin32.sourceforge.net/packages/sed.htm")
	else()
		message(FATAL_ERROR "Could not find the sed program.")
	endif()
endif()

find_program(PKG_CONFIG_PROGRAM
	NAMES pkg-config pkg-config.exe
	HINTS "C:/MinGW/bin"
)

if(NOT WINDOWS_UNIVERSAL)
	if(NOT PKG_CONFIG_PROGRAM)
		if(WIN32)
			message(STATUS "Installing pkg-config to C:/MinGW/bin")
			set(_pkg_config_dir ${CMAKE_BINARY_DIR}/pkg-config)
			file(MAKE_DIRECTORY ${_pkg_config_dir})
			file(DOWNLOAD http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/pkg-config_0.26-1_win32.zip "${CMAKE_BINARY_DIR}/pkg-config.zip")
			execute_process(
				COMMAND "${CMAKE_COMMAND}" "-E" "tar" "x" "${CMAKE_BINARY_DIR}/pkg-config.zip"
				WORKING_DIRECTORY ${_pkg_config_dir}
			)
			file(DOWNLOAD http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/pkg-config-dev_0.26-1_win32.zip "${CMAKE_BINARY_DIR}/pkg-config-dev.zip")
			execute_process(
				COMMAND "${CMAKE_COMMAND}" "-E" "tar" "x" "${CMAKE_BINARY_DIR}/pkg-config-dev.zip"
				WORKING_DIRECTORY ${_pkg_config_dir}
			)
			file(DOWNLOAD http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/gettext-runtime_0.18.1.1-2_win32.zip "${CMAKE_BINARY_DIR}/gettext-runtime.zip")
			execute_process(
				COMMAND "${CMAKE_COMMAND}" "-E" "tar" "x" "${CMAKE_BINARY_DIR}/gettext-runtime.zip"
				WORKING_DIRECTORY ${_pkg_config_dir}
			)
			file(DOWNLOAD http://ftp.acc.umu.se/pub/gnome/binaries/win32/glib/2.28/glib_2.28.8-1_win32.zip "${CMAKE_BINARY_DIR}/glib.zip")
			execute_process(
				COMMAND "${CMAKE_COMMAND}" "-E" "tar" "x" "${CMAKE_BINARY_DIR}/glib.zip"
				WORKING_DIRECTORY ${_pkg_config_dir}
			)
			file(RENAME "${_pkg_config_dir}/bin/pkg-config.exe" "C:/MinGW/bin/pkg-config.exe")
			file(RENAME "${_pkg_config_dir}/share/aclocal/pkg.m4" "C:/MinGW/share/aclocal/pkg.m4")
			file(RENAME "${_pkg_config_dir}/bin/libglib-2.0-0.dll" "C:/MinGW/bin/libglib-2.0-0.dll")
			file(RENAME "${_pkg_config_dir}/bin/intl.dll" "C:/MinGW/bin/intl.dll")
			unset(_pkg_config_dir)
		endif()

		find_program(PKG_CONFIG_PROGRAM
			NAMES pkg-config pkg-config.exe
			HINTS "C:/MinGW/bin"
		)
	endif()

	if(NOT PKG_CONFIG_PROGRAM AND NOT MSVC)
		message(FATAL_ERROR "Could not find the pkg-config program.")
	endif()

	if(ENABLE_NLS)
		find_program(INTLTOOLIZE_PROGRAM
			NAMES intltoolize
			HINTS "C:/MinGW/msys/1.0/bin"
		)

		if(NOT INTLTOOLIZE_PROGRAM)
			if(WIN32)
				message(STATUS "Installing intltoolize to C:/MinGW/bin")
				set(_intltoolize_dir ${CMAKE_BINARY_DIR}/intltoolize)
				file(MAKE_DIRECTORY ${_intltoolize_dir})
				file(DOWNLOAD http://ftp.gnome.org/pub/gnome/binaries/win32/intltool/0.40/intltool_0.40.4-1_win32.zip "${CMAKE_BINARY_DIR}/intltoolize.zip")
				execute_process(
					COMMAND "${CMAKE_COMMAND}" "-E" "tar" "x" "${CMAKE_BINARY_DIR}/intltoolize.zip"
					WORKING_DIRECTORY ${_intltoolize_dir}
				)
				execute_process(
					COMMAND "${SED_PROGRAM}" "-i" "s;/opt/perl/bin/perl;/bin/perl;g" "${_intltoolize_dir}/bin/intltool-extract"
					COMMAND "${SED_PROGRAM}" "-i" "s;/opt/perl/bin/perl;/bin/perl;g" "${_intltoolize_dir}/bin/intltool-merge"
					COMMAND "${SED_PROGRAM}" "-i" "s;/opt/perl/bin/perl;/bin/perl;g" "${_intltoolize_dir}/bin/intltool-prepare"
					COMMAND "${SED_PROGRAM}" "-i" "s;/opt/perl/bin/perl;/bin/perl;g" "${_intltoolize_dir}/bin/intltool-update"
				)
				file(RENAME "${_intltoolize_dir}/bin/intltoolize" "C:/MinGW/msys/1.0/bin/intltoolize")
				file(RENAME "${_intltoolize_dir}/bin/intltool-extract" "C:/MinGW/bin/intltool-extract")
				file(RENAME "${_intltoolize_dir}/bin/intltool-merge" "C:/MinGW/bin/intltool-merge")
				file(RENAME "${_intltoolize_dir}/bin/intltool-prepare" "C:/MinGW/bin/intltool-prepare")
				file(RENAME "${_intltoolize_dir}/bin/intltool-update" "C:/MinGW/bin/intltool-update")
				file(RENAME "${_intltoolize_dir}/share/aclocal/intltool.m4" "C:/MinGW/share/aclocal/intltool.m4")
				file(MAKE_DIRECTORY "C:/MinGW/msys/1.0/share/intltool")
				file(RENAME "${_intltoolize_dir}/share/intltool/Makefile.in.in" "C:/MinGW/msys/1.0/share/intltool/Makefile.in.in")
				unset(_intltoolize_dir)
			endif()

			find_program(INTLTOOLIZE_PROGRAM
				NAMES intltoolize
				HINTS "C:/MinGW/msys/1.0/bin"
			)
		endif()

		if(NOT INTLTOOLIZE_PROGRAM AND NOT MSVC)
			message(FATAL_ERROR "Could not find the intltoolize program.")
		endif()
	endif()
endif()

if(MSVC AND NOT WINDOWS_UNIVERSAL)
	# Install headers needed by MSVC
	file(GLOB MSVC_HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MSVC/*.h")
	file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/include/MSVC")
	file(INSTALL ${MSVC_HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/MSVC")
endif()
