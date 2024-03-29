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
project(androidcpufeatures LANGUAGES C)

include(GNUInstallDirs)


add_library(cpufeatures STATIC "${CMAKE_ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c")
target_include_directories(cpufeatures PUBLIC
	$<BUILD_INTERFACE:${CMAKE_ANDROID_NDK}/sources/android/cpufeatures>
	$<INSTALL_INTERFACE:include>
)

install(TARGETS cpufeatures EXPORT CpuFeaturesTargets
	RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(FILES "${CMAKE_ANDROID_NDK}/sources/android/cpufeatures/cpu-features.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(EXPORT CpuFeaturesTargets
	FILE "CpuFeaturesTargets.cmake"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/cpufeatures"
)
