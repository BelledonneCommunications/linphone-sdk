############################################################################
# toolchain-bundled-android-common.cmake
# Copyright (C) 2010-2025  Belledonne Communications, Grenoble France
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

set(ANDROID_STL "c++_static")
set(ANDROID_PLATFORM "android-28") # At least Android 28 is needed to build Android-specific mediastreamer2 plugins statically

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--exclude-libs,libc++_static.a -Wl,--exclude-libs,libc++abi.a")

include("${CMAKE_CURRENT_LIST_DIR}/toolchain-android-common.cmake")
