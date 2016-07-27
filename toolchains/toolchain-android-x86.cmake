############################################################################
# toolchan-android-x86.cmake
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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

set(CMAKE_SYSTEM_PROCESSOR "x86")
set(ARCHITECTURE "x86")
set(NDK_ARCHITECTURE "x86")
set(COMPILER_PREFIX "i686-linux-android")
set(CLANG_TARGET "i686-none-linux-androideabi")
include("${CMAKE_CURRENT_LIST_DIR}/android/toolchain-android.cmake")

add_compile_options(
	"-ffunction-sections"
	"-funwind-tables"
	"-fstack-protector"
	"-no-canonical-prefixes"
	"-fomit-frame-pointer"
	"-fstrict-aliasing"
)

if(NOT CLANG_EXECUTABLE)
	add_compile_options(
		"-funswitch-loops"
		"-finline-limit=300"
	)
endif()

link_libraries(
	"-no-canonical-prefixes"
	"-Wl,--no-undefined"
	"-Wl,-z,noexecstack"
	"-Wl,-z,relro"
	"-Wl,-z,now"
)
