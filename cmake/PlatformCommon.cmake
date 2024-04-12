############################################################################
# PlatformCommon.cmake
# Copyright (C) 2010-2024 Belledonne Communications, Grenoble France
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

# Add clang-format pre commit hook to specified projects
set(SUBMODULES_TO_HOOK bctoolbox belcard belle-sip belr liblinphone mediastreamer2 ortp)
foreach(SUBMODULE IN LISTS SUBMODULES_TO_HOOK)
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git/modules/${SUBMODULE}")
		file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/cmake/hook/pre-commit" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/.git/modules/${SUBMODULE}/hooks/")
	endif()
endforeach()

# Enable color diagnostics
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24.0)
	set(CMAKE_COLOR_DIAGNOSTICS ON)
else()
	if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
	elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcolor-diagnostics")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
	endif()
endif()

if(LINPHONESDK_BUILD_TYPE STREQUAL "Default")
	if(NOT DEFINED CMAKE_INSTALL_MESSAGE)
		set(CMAKE_INSTALL_MESSAGE LAZY CACHE STRING "Specify verbosity of installation script code" FORCE)
		set_property(CACHE CMAKE_INSTALL_MESSAGE PROPERTY STRINGS "ALWAYS" "LAZY" "NEVER")
	endif()

	if(WIN32)
		find_program(SCCACHE_PROGRAM "sccache")
		if(SCCACHE_PROGRAM)
			set(CMAKE_C_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}" CACHE PATH "Compiler launcher for C source code" FORCE)
			set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}" CACHE PATH "Compiler launcher for C++ source code" FORCE)
		endif()
		mark_as_advanced(SCCACHE_PROGRAM)
	else()
		find_program(CCACHE_PROGRAM "ccache")
		if(CCACHE_PROGRAM)
			set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE PATH "Compiler launcher for C source code" FORCE)
			set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE PATH "Compiler launcher for C++ source code" FORCE)
		endif()
		mark_as_advanced(CCACHE_PROGRAM)
	endif()

	if((NOT DEFINED CMAKE_INSTALL_PREFIX) OR CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
		string(TOLOWER "${LINPHONESDK_PLATFORM}" LINPHONESDK_PLATFORM_LOWER)
		set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/linphone-sdk/${LINPHONESDK_PLATFORM_LOWER}" CACHE PATH "Default linphone-sdk installation prefix" FORCE)
	endif()
endif()

include(ExternalProject)
