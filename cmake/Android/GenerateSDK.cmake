############################################################################
# GenerateSDK.cmake
# Copyright (C) 2010-2023 Belledonne Communications, Grenoble France
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

if(CMAKE_BUILD_TYPE STREQUAL "Release")
	execute_process(
		COMMAND "${LINPHONESDK_DIR}/cmake/Android/gradlew" "--stacktrace" "assembleRelease"
		RESULT_VARIABLE _gradle_assemble_result
	)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
	execute_process(
		COMMAND "${LINPHONESDK_DIR}/cmake/Android/gradlew" "--stacktrace" "assembleDebug"
		RESULT_VARIABLE _gradle_assemble_result
	)
elseif(CMAKE_BUILD_TYPE STREQUAL "ASAN")
	execute_process(
		COMMAND "${LINPHONESDK_DIR}/cmake/Android/gradlew" "--stacktrace" "assembleDebug"
		RESULT_VARIABLE _gradle_assemble_result
	)
else()
	execute_process(
		COMMAND "${LINPHONESDK_DIR}/cmake/Android/gradlew" "--stacktrace" "assemble"
		RESULT_VARIABLE _gradle_assemble_result
	)
endif()
if(_gradle_assemble_result)
	message(FATAL_ERROR "Gradle assemble failed")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
	execute_process(
		COMMAND "${LINPHONESDK_DIR}/cmake/Android/gradlew" "--stacktrace" "-b" "upload.gradle" "publishReleasePublicationToMavenRepository"
	)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
	execute_process(
		COMMAND "${LINPHONESDK_DIR}/cmake/Android/gradlew" "--stacktrace" "-b" "upload.gradle" "publishDebugPublicationToMavenRepository"
	)
elseif(CMAKE_BUILD_TYPE STREQUAL "ASAN")
	execute_process(
		COMMAND "${LINPHONESDK_DIR}/cmake/Android/gradlew" "--stacktrace" "-b" "upload.gradle" "publishDebugPublicationToMavenRepository"
	)
else()
	execute_process(
		COMMAND "${LINPHONESDK_DIR}/cmake/Android/gradlew" "--stacktrace" "-b" "upload.gradle" "publish"
	)
endif()

execute_process(
	COMMAND "${LINPHONESDK_DIR}/cmake/Android/gradlew" "--stacktrace" "-q" "sdkZip"
	RESULT_VARIABLE _gradle_sdkzip_result
)
if(_gradle_sdkzip_result)
	message(FATAL_ERROR "Gradle sdkZip failed")
endif()
