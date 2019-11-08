############################################################################
# LinphoneSdkPlatformCommon.cmake
# Copyright (C) 2010-2018 Belledonne Communications, Grenoble France
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

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified")
	set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo" FORCE)
	# Set the available build type values for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()

if(NOT DEFINED CMAKE_INSTALL_MESSAGE)
	set(CMAKE_INSTALL_MESSAGE LAZY CACHE STRING "Specify verbosity of installation script code" FORCE)
	set_property(CACHE CMAKE_INSTALL_MESSAGE PROPERTY STRINGS "ALWAYS" "LAZY" "NEVER")
endif()

find_program(CCACHE_PROGRAM "ccache")
if(CCACHE_PROGRAM)
	set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE PATH "Compiler launcher for C source code" FORCE)
	set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE PATH "Compiler launcher for C++ source code" FORCE)
endif()
mark_as_advanced(CCACHE_PROGRAM)

include(ExternalProject)
