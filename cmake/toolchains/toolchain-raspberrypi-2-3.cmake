# ##############################################################################
# toolchain-raspberrypi-2-3.cmake
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

set(CMAKE_SYSTEM_PROCESSOR "armv7")

# Define the compiler flags
# if(RASPBERRY_VERSION VERSION_GREATER 2) set(CMAKE_C_FLAGS "-mcpu=cortex-a53
# -mfpu=neon-vfpv4 -mfloat-abi=hard" CACHE STRING "Flags for Raspberry PI 3")
# set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Flags for Raspberry PI
# 3") elseif(RASPBERRY_VERSION VERSION_GREATER 1)
set(CMAKE_C_FLAGS "-march=armv7-a -mfloat-abi=hard -mfpu=neon-vfpv4" CACHE STRING "Flags for Raspberry PI 2 & 3 Model A/B")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Flags for Raspberry PI 2 & 3 Model A/B")
