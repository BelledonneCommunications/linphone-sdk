################################################################################
#  PlatformMacos.cmake
#  Copyright (c) 2021-2023 Belledonne Communications SARL.
# 
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#
################################################################################

include("${CMAKE_CURRENT_LIST_DIR}/PlatformCommon.cmake")

message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR : ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR : ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_HOST_SYSTEM_NAME : ${CMAKE_HOST_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_NAME : ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_APPLE_SILICON_PROCESSOR : ${CMAKE_APPLE_SILICON_PROCESSOR}")

include("${CMAKE_CURRENT_LIST_DIR}/PlatformCommon.cmake")

if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
	# Without instruction choose to target lower version between current machine and current used SDK
	execute_process(COMMAND sw_vers -productVersion  COMMAND awk -F \\. "{printf \"%i.%i\",$1,$2}"  RESULT_VARIABLE sw_vers_version OUTPUT_VARIABLE CURRENT_OSX_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND xcrun --sdk macosx --show-sdk-version RESULT_VARIABLE xcrun_sdk_version OUTPUT_VARIABLE CURRENT_SDK_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(${CURRENT_OSX_VERSION} VERSION_LESS ${CURRENT_SDK_VERSION})
		set(CMAKE_OSX_DEPLOYMENT_TARGET ${CURRENT_OSX_VERSION})
	else()
		set(CMAKE_OSX_DEPLOYMENT_TARGET ${CURRENT_SDK_VERSION})
	endif()
endif()

if(ENABLE_SCREENSHARING AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "12.3")
	message(FATAL_ERROR "Minimal OS X deployment target of 12.3 required for Screen Sharing!")
elseif(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "10.15")
	message(FATAL_ERROR "Minimal OS X deployment target of 10.15 required!")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(CMAKE_OSX_ARCHITECTURES ${CMAKE_SYSTEM_PROCESSOR})
else()
	set(CMAKE_OSX_ARCHITECTURES "i386")
endif()

set(CMAKE_MACOSX_RPATH TRUE)
if (ENABLE_PYTHON_WRAPPER)
	set(CMAKE_INSTALL_RPATH "@loader_path/Frameworks;@loader_path")
else()
	set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks;@executable_path/../lib")
endif()

if (NOT (CMAKE_SYSTEM_PROCESSOR STREQUAL CMAKE_HOST_SYSTEM_PROCESSOR))
	set (CMAKE_CROSSCOMPILING TRUE)
	message(STATUS "Forcing Cross compilation as CMAKE_SYSTEM_PROCESSOR != CMAKE_HOST_SYSTEM_PROCESSOR")
endif ()
