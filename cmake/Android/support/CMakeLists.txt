############################################################################
# CMakeLists.txt
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
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

cmake_minimum_required(VERSION 3.22)
project(androidsupport LANGUAGES C CXX)

include(GNUInstallDirs)


if(CMAKE_ANDROID_NDK_VERSION VERSION_LESS 16)
	set(SUPPORT_SOURCES
		"${CMAKE_ANDROID_NDK}/sources/android/support/src/musl-multibyte/wctomb.c"
	)
elseif(CMAKE_ANDROID_NDK_VERSION VERSION_LESS 17)
	set(SUPPORT_SOURCES
		"${CMAKE_ANDROID_NDK}/sources/android/support/src/iswblank.cpp"
	)
else()
	set(SUPPORT_SOURCES
		"${CMAKE_ANDROID_NDK}/sources/android/support/src/locale_support.cpp"
	)
endif()

add_library(support STATIC ${SUPPORT_SOURCES})
target_include_directories(support PUBLIC "${CMAKE_ANDROID_NDK}/sources/android/support/include")

if(CMAKE_ANDROID_NDK_VERSION VERSION_LESS 16)
	if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
		target_compile_options(support PRIVATE "-std=c99")
	endif()

	if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
		install(FILES
			"${CMAKE_ANDROID_NDK}/sources/android/support/include/ctype.h"
			"${CMAKE_ANDROID_NDK}/sources/android/support/include/langinfo.h"
			"${CMAKE_ANDROID_NDK}/sources/android/support/include/nl_types.h"
			"${CMAKE_ANDROID_NDK}/sources/android/support/include/xlocale.h"
			DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
	endif()
endif()

install(TARGETS support
	RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
