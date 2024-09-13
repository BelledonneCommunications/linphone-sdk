# ##############################################################################
# toolchain-raspberrypi-3+.cmake
# Copyright (C) 2023  Belledonne Communications, Grenoble France
# ##############################################################################
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
# ##############################################################################

include("${CMAKE_CURRENT_LIST_DIR}/toolchain-raspberrypi-common.cmake")

set(CMAKE_SYSTEM_PROCESSOR "armv8")

# Define the compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+crypto -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -march=armv8-a+crypto -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8")
