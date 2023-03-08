############################################################################
# HWSanitizer.cmake
# Copyright (C) 2010-2023  Belledonne Communications, Grenoble France
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

#####################################################################################################################################
# !Warning: be SURE YOU REALLY KNOW what you are doing before modifying this file!
#####################################################################################################################################

# See https://developer.android.com/ndk/guides/hwasan

set(sanitize_flags "-fsanitize=hwaddress -fno-omit-frame-pointer")
set(sanitize_linker_flags "-fsanitize=hwaddress")

# These link options are prepended by a semicolon if the following quotes are missing.
# We must set this quotes to prevent cmake from considering the given set as a list append
# See	https://cmake.org/cmake/help/v3.16/manual/cmake-language.7.html#cmake-language-variables

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${sanitize_flags}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${sanitize_flags}")

set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} ${sanitize_linker_flags})
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${sanitize_linker_flags}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${sanitize_linker_flags}")

unset(sanitize_flags)
unset(sanitize_linker_flags)
