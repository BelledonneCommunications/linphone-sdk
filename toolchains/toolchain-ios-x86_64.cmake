############################################################################
# toolchan-ios-x86_64.cmake
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

set(CMAKE_SYSTEM_PROCESSOR "x86_64")
set(LINPHONE_BUILDER_OSX_ARCHITECTURES "x86_64")
set(COMPILER_PREFIX "x86_64-apple-darwin")
set(CLANG_TARGET "x86_64-apple-darwin")
set(PLATFORM "Simulator")
include("${CMAKE_CURRENT_LIST_DIR}/ios/toolchain-ios.cmake")


