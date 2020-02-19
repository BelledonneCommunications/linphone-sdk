############################################################################
# CMakeLists.txt
# Copyright (C) 2019 Belledonne Communications, Grenoble France
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

cmake_minimum_required(VERSION 3.0)

#Builds camera2 with android sdk 24 while others projects are builds with another version
#(defined in cmake builer toolchain android file)
#This systems ensures a retrocompatible build, and the activation of new features ONLY
#on devices supporting it
set(ANDROID_PLATFORM "android-24" CACHE STRING "" FORCE)

#Needed to take priority over cache to redefine android sdk version with ANDROID_PLATFORM
#Otherwize, the sdk version is replaced by the cmake builder toolchain android default one
set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS "")
set(CMAKE_SHARED_LINKER_FLAGS "")
set(CMAKE_MODULE_LINKER_FLAGS "")

if(ENABLE_SANITIZER)

	message(WARNING "CODE DUPLICATION : duplication of sanitizer flags ! TODO use config-sanitizer instead for linphone-sdk 5.0 ! ")

	set(sanitize_flags "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls -Wl,-llog")
	set(sanitize_linker_flags "-fsanitize=address,undefined")

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${sanitize_flags}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${sanitize_flags}")

	set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} ${sanitize_linker_flags})
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${sanitize_linker_flags}")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${sanitize_linker_flags}")
endif()
if(ENABLE_HW_SANITIZER)
	message(WARNING "CODE DUPLICATION : duplication of sanitizer flags ! TODO use config-hwsanitizer instead for linphone-sdk 5.0 ! ")

	set(sanitize_flags "-fsanitize=hwaddress -fno-omit-frame-pointer")
	set(sanitize_linker_flags "-fsanitize=hwaddress")

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${sanitize_flags}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${sanitize_flags}")

	set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} ${sanitize_linker_flags})
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${sanitize_linker_flags}")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${sanitize_linker_flags}")
endif()

project(MSAndroidCamera2 VERSION 0.1 LANGUAGES CXX)

option(ENABLE_SHARED "Build shared library." YES)
option(ENABLE_STATIC "Build static library." NO)

include(GNUInstallDirs)

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

message(STATUS "NDK API IS ${ANDROID_NATIVE_API_LEVEL}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(Mediastreamer2 CONFIG REQUIRED)
find_package(bctoolbox CONFIG REQUIRED)
find_package(ortp CONFIG REQUIRED)

include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}
	${MEDIASTREAMER2_INCLUDE_DIRS}
)

set(LIBS ${MEDIASTREAMER2_LIBRARIES} android camera2ndk mediandk ${ORTP_LIBRARIES} ${BCTOOLBOX_CORE_LIBRARIES})

set(SOURCE_FILES android-camera2-capture.cpp)

#Inherited from ms2 cmake config file
set(MS2_PLUGINS_DIR "${MEDIASTREAMER2_PLUGINS_LOCATION}")

if(ENABLE_STATIC)
	add_library(msandroidcamera2-static STATIC ${SOURCE_FILES})
	set_target_properties(msandroidcamera2-static PROPERTIES OUTPUT_NAME msandroidcamera2)
	target_link_libraries(msandroidcamera2-static ${LIBS})
	install(TARGETS msandroidcamera2-static
		RUNTIME DESTINATION ${MS2_PLUGINS_DIR}
		LIBRARY DESTINATION ${MS2_PLUGINS_DIR}
		ARCHIVE DESTINATION ${MS2_PLUGINS_DIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	add_library(msandroidcamera2 MODULE ${SOURCE_FILES})
	target_link_libraries(msandroidcamera2 ${LIBS})
	install(TARGETS msandroidcamera2
		RUNTIME DESTINATION ${MS2_PLUGINS_DIR}
		LIBRARY DESTINATION ${MS2_PLUGINS_DIR}
		ARCHIVE DESTINATION ${MS2_PLUGINS_DIR}
		FRAMEWORK DESTINATION Frameworks
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
