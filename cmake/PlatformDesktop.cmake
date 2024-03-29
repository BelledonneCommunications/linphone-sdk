################################################################################
#  PlatformDesktop.cmake
#  Copyright (c) 2010-2023 Belledonne Communications SARL.
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


if(WIN32)
	# Force ffmpeg to be disabled on Windows
  set(ENABLE_FFMPEG OFF CACHE BOOL "Build mediastreamer2 with ffmpeg video support." FORCE)

	# Add some specific flags on Windows
	set(_WINDOWS_SPECIFIC_FLAGS "-D_WIN32_WINNT=0x0602 -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_ALLOW_KEYWORD_MACROS /MP")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_WINDOWS_SPECIFIC_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_WINDOWS_SPECIFIC_FLAGS}")

else()

	include(GNUInstallDirs)

	if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
		if(ENABLE_PYTHON_WRAPPER)
			set(CMAKE_INSTALL_RPATH "$ORIGIN")
		else()
			set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
		endif()
	endif()

endif()
