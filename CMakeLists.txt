############################################################################
# CMakeLists.txt
# Copyright (c) 2010-2023 Belledonne Communications SARL.
#
############################################################################
#
# This file is part of Linphone SDK 
# (see https://gitlab.linphone.org/BC/public/linphone-sdk).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
############################################################################

cmake_minimum_required(VERSION 3.22)

include("bctoolbox/cmake/BCToolboxCMakeUtils.cmake")
include("cmake/LinphoneSdkUtils.cmake")

if(POLICY CMP0149)
    # VS generator looks for most recent Windows SDK, ignoring
    # CMAKE_SYSTEM_VERSION and allowing override by WindowsSDKVersion
    # environment variable. New in 3.27. This is to allow override
    # in the Windows CI builds.
    # This MUST be set before any project() or or enable_language() command.
    cmake_policy(SET CMP0149 NEW)
endif()

if(POLICY CMP0141)
    # Changes the way debug info on Windows are stored (forces /Z7)
    # This is a requirement to use build cache on Windows, since /Zi is another Microsoft cache, incompatible with any third party cache known to date
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
    cmake_policy(SET CMP0141 NEW)
endif()

# Unset PROJECT_VERSION for the case linphone-sdk is included from an other project
unset(PROJECT_VERSION)

if(NOT LINPHONESDK_VERSION) # If LINPHONESDK_VERSION exists (ie: is in cache), then the variable has been overriden by user
	bc_compute_full_version(LINPHONESDK_VERSION)
endif()
if(NOT LINPHONESDK_STATE)
	bc_compute_snapshots_or_releases_state(LINPHONESDK_STATE)
endif()
if(NOT LINPHONESDK_BRANCH)
	linphone_sdk_compute_git_branch(LINPHONESDK_BRANCH)
endif()
set(LINPHONESDK_VERSION_CACHED ${LINPHONESDK_VERSION} CACHE STRING "" FORCE) # Put in cache the current version : it is used to read version in cache

bc_parse_full_version("${LINPHONESDK_VERSION}" MAJOR MINOR PATCH)

project(linphone-sdk VERSION "${MAJOR}.${MINOR}.${PATCH}" LANGUAGES C CXX)

unset(MAJOR)
unset(MINOR)
unset(PATCH)


if(APPLE)
	if(LINPHONESDK_PLATFORM MATCHES "Desktop") # On Apple, Desktop == Macos
		set(LINPHONESDK_PLATFORM "Macos" CACHE STRING "Platform to build" FORCE)
	else()
		set(LINPHONESDK_PLATFORM "Macos" CACHE STRING "Platform to build")
	endif()
else()
	set(LINPHONESDK_PLATFORM "Desktop" CACHE STRING "Platform to build")
endif()
set_property(CACHE LINPHONESDK_PLATFORM PROPERTY STRINGS "Android" "Desktop" "IOS" "Macos" "UWP" "Yocto")

set(LINPHONESDK_BUILD_TYPE "Default" CACHE STRING "Type of build")
set_property(CACHE LINPHONESDK_BUILD_TYPE PROPERTY STRINGS "Default" "Android" "Flexisip" "IOS" "Macos" "Packager" "UWP" "Windows")

set(LINPHONESDK_PACKAGER "" CACHE STRING "Packager name")
set_property(CACHE LINPHONESDK_PACKAGER PROPERTY STRINGS "Nuget" "")


if(LINPHONESDK_PLATFORM STREQUAL "Android")
	set(ANDROID TRUE)
elseif(LINPHONESDK_PLATFORM STREQUAL "IOS")
	set(IOS TRUE)
elseif(LINPHONESDK_PLATFORM STREQUAL "UWP")
	set(UWP TRUE)
endif()

include("cmake/Options.cmake")
include("cmake/CheckBuildTools${LINPHONESDK_PLATFORM}.cmake")

if(ENABLE_SANITIZER)
	include("cmake/Sanitizer.cmake")
endif()
if(ENABLE_HW_SANITIZER)
   include("cmake/HWSanitizer.cmake")
endif()

include("cmake/Platform${LINPHONESDK_PLATFORM}.cmake")

if(LINPHONESDK_BUILD_TYPE STREQUAL "Default" OR LINPHONESDK_BUILD_TYPE STREQUAL "Flexisip")
	add_custom_target(sdk)
	if(ENABLE_EKT_SERVER_PLUGIN)
		include("cmake/EktServerClone.cmake")
	endif()
	if(ENABLE_TUNNEL)
		include("cmake/TunnelClone.cmake")
	endif()

	if(ENABLE_PQCRYPTO AND IOS AND (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64"))
        	set(ENABLE_PQCRYPTO OFF)
        	message (WARNING "PQCRYPTO disabled for x86_64 ios simultator due to compatibility issue with xcode 15")
	endif()
	# Need to activate ASM and ASM_NASM at the top level and in this order to workaround a bug of CMake
	enable_language(ASM)
	enable_language(ASM_NASM)

	list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules")
	include("cmake/ExternalDependencies.cmake")
	include("cmake/BCProjects.cmake")

elseif(LINPHONESDK_BUILD_TYPE STREQUAL "Packager")

	include("cmake/LinphoneSdkPackager.cmake")

else()

	include("cmake/Tasks${LINPHONESDK_BUILD_TYPE}.cmake")

endif()

# Give a feature summary
include(FeatureSummary)
feature_summary(FILENAME "${CMAKE_BINARY_DIR}/enabled_features.txt" WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
feature_summary(WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
feature_summary(FILENAME "${CMAKE_BINARY_DIR}/disabled_features.txt" WHAT DISABLED_FEATURES DESCRIPTION "Disabled features:")
feature_summary(WHAT DISABLED_FEATURES DESCRIPTION "Disabled features:")

if(ENABLE_GPL_THIRD_PARTIES)
	message(WARNING "
  ***************************************************************************
  ***************************************************************************
  ***** CAUTION, this Linphone SDK is built using third party GPL code  *****
  ***** Even if you acquired a proprietary license from Belledonne      *****
  ***** Communications, this SDK is GPL and GPL only.                   *****
  ***** To disable third party GPL code, set the                        *****
  ***** ENABLE_GPL_THIRD_PARTIES option to OFF                          *****
  ***************************************************************************
  ***************************************************************************
  ")
elseif(ENABLE_G729 OR ENABLE_G729B_CNG)
	message(WARNING "
  ***************************************************************************
  ***************************************************************************
  ***** CAUTION, this Linphone SDK is built with G729 codec (thanks to  *****
  ***** the bcg729 library).                                            *****
  ***** You must in addition acquire a bcg729 licence extension to use  *****
  ***** this SDK to create a proprietary application.                   *****
  ***************************************************************************
  ***************************************************************************
  ")
else()
	message("
  ***************************************************************************
  ***************************************************************************
  ***** Linphone SDK without third party GPL software                   *****
  ***** If you acquired a proprietary license from Belledonne           *****
  ***** Communications, this SDK can be used to create                  *****
  ***** a proprietary Linphone-based application.                       *****
  ***************************************************************************
  ***************************************************************************
  ")
endif()

if(ENABLE_PYTHON_WRAPPER)
   include("cmake/python/GenerateWheel.cmake")
endif()
