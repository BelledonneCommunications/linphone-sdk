################################################################################
#
#  Copyright (c) 2010-2021 Belledonne Communications SARL.
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

cmake_minimum_required(VERSION 3.2)


project(linphone-sdk VERSION 5.0.0 LANGUAGES NONE)


set(LINPHONESDK_DIR "${CMAKE_CURRENT_LIST_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LINPHONESDK_DIR}/cmake")
include(LinphoneSdkUtils)

include(bctoolbox/cmake/BcToolboxCMakeUtils.cmake)


if(NOT LINPHONESDK_VERSION)#If LINPHONESDK_VERSION exists (ie: is in cache), then the variable has been overriden by user
	bc_compute_full_version(LINPHONESDK_VERSION)
endif()
if(NOT LINPHONESDK_STATE)
	bc_compute_snapshots_or_releases_state(LINPHONESDK_STATE)
endif()
if(NOT LINPHONESDK_BRANCH)
	linphone_sdk_compute_git_branch(LINPHONESDK_BRANCH)
endif()
set(LINPHONESDK_VERSION_CACHED ${LINPHONESDK_VERSION} CACHE STRING "" FORCE)#Put in cache the current version : it is used to read version in cache from 'cmake -L' command
set(LINPHONESDK_PREBUILD_DEPENDENCIES)
if(ENABLE_TUNNEL)
	add_custom_target(tunnel-clone
		"${CMAKE_COMMAND}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-P" "${LINPHONESDK_DIR}/cmake/LinphoneSdkTunnelClone.cmake"
	)
	list(APPEND LINPHONESDK_PREBUILD_DEPENDENCIES tunnel-clone)
endif()


set(LINPHONESDK_PLATFORM "Desktop" CACHE STRING "Platform to build")
set_property(CACHE LINPHONESDK_PLATFORM PROPERTY STRINGS "Android" "Desktop" "IOS" "UWP")

set(LINPHONESDK_PACKAGER "" CACHE STRING "Packager name")
set_property(CACHE LINPHONESDK_PACKAGER PROPERTY STRINGS "Nuget" "")

if(NOT LINPHONESDK_PACKAGER OR LINPHONESDK_PACKAGER STREQUAL "OFF")
	if (LINPHONESDK_PLATFORM STREQUAL "Android")
	  include(LinphoneSdkCheckBuildToolsAndroid)
	endif()
	
	# TODO: This will be deletable once we get rid of cmake-builder
	string(TOLOWER "${LINPHONESDK_PLATFORM}" _lowercase_platform)
	include(cmake-builder/cmake/LinphoneOptions.cmake)
	include(cmake-builder/configs/options-${_lowercase_platform}.cmake)
	include(cmake-builder/options/common.cmake)
	include(cmake-builder/options/bctoolbox.cmake)
	include(cmake-builder/options/bellesip.cmake)
	include(cmake-builder/options/ms2.cmake)
	include(cmake-builder/options/linphone.cmake)
	
	# Give a feature summary
	include(FeatureSummary)
	feature_summary(FILENAME "${CMAKE_BINARY_DIR}/enabled_features.txt" WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
	feature_summary(WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
	feature_summary(FILENAME "${CMAKE_BINARY_DIR}/disabled_features.txt" WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
	feature_summary(WHAT DISABLED_FEATURES DESCRIPTION "Disabled features:")
	
	if (ENABLE_SANITIZER)
	   include("${CMAKE_CURRENT_SOURCE_DIR}/cmake-builder/configs/config-sanitizer.cmake")
	endif()
	if (ENABLE_HW_SANITIZER)
	   include("${CMAKE_CURRENT_SOURCE_DIR}/cmake-builder/configs/config-hwsanitizer.cmake")
	endif()
	include(LinphoneSdkPlatform${LINPHONESDK_PLATFORM})
else()
	include(LinphoneSdkPackager)
endif()


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
  ***************************************************************************")
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
