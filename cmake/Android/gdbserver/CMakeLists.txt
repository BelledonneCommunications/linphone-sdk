############################################################################
# CMakeLists.txt
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
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

cmake_minimum_required(VERSION 3.22)
project(gdbserver LANGUAGES) # Dummy project

configure_file(gdb.setup.in gdb.setup @ONLY)

install(FILES "${PROJECT_BINARY_DIR}/gdb.setup" DESTINATION bin)

if(EXISTS "${CMAKE_ANDROID_NDK}/prebuilt/android-${ANDROID_SYSROOT_ABI}/gdbserver/gdbserver")
	install(PROGRAMS "${CMAKE_ANDROID_NDK}/prebuilt/android-${ANDROID_SYSROOT_ABI}/gdbserver/gdbserver" DESTINATION bin)
else()
	message("Failed to find gdb-server at [${CMAKE_ANDROID_NDK}/prebuilt/android-${ANDROID_SYSROOT_ABI}/gdbserver/gdbserver], trying with lldb-server")

	if(EXISTS "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/${CMAKE_C_COMPILER_VERSION}/lib/linux/${CMAKE_SYSTEM_PROCESSOR}/lldb-server")
		install(PROGRAMS "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/${CMAKE_C_COMPILER_VERSION}/lib/linux/${CMAKE_SYSTEM_PROCESSOR}/lldb-server" DESTINATION bin)
	else()
		message("Failed to find lldb-server at [${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/${CMAKE_C_COMPILER_VERSION}/lib/linux/${CMAKE_SYSTEM_PROCESSOR}/lldb-server]")
	endif()
endif()