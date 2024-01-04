############################################################################
# PlatformIOS.cmake
# Copyright (C) 2010-2023 Belledonne Communications, Grenoble France
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

include("${CMAKE_CURRENT_LIST_DIR}/PlatformCommon.cmake")

# Set default SYSROOT for IOS to avoid xcode12/cmake 3.18.2 to not be able to detect clang for x86_64
set(CMAKE_OSX_SYSROOT "iphonesimulator" CACHE STRING "System root for iOS" FORCE)

# Force swift doc to be disabled for archs other than arm64-simulator
if(CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_TOOLCHAIN_FILE MATCHES "^.*/toolchain-ios-arm64-simulator.cmake$")
	set(ENABLE_SWIFT_DOC OFF CACHE BOOL "Build the Swift doc from Liblinphone." FORCE)
endif()

if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")
endif()

set(CMAKE_MACOSX_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
