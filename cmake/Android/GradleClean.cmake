############################################################################
# GradleClean.cmake
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

configure_file("${LINPHONESDK_DIR}/cmake/Android/gradle/build.gradle.cmake" "${LINPHONESDK_BUILD_DIR}/build.gradle" @ONLY)
configure_file("${LINPHONESDK_DIR}/cmake/Android/gradle/upload.gradle.cmake" "${LINPHONESDK_BUILD_DIR}/upload.gradle" @ONLY)
configure_file("${LINPHONESDK_DIR}/cmake/Android/gradle/gradle.properties.cmake" "${LINPHONESDK_BUILD_DIR}/gradle.properties" @ONLY)
configure_file("${LINPHONESDK_DIR}/cmake/Android/gradle/LinphoneSdkManifest.xml.cmake" "${LINPHONESDK_BUILD_DIR}/LinphoneSdkManifest.xml" @ONLY)

execute_process(
	COMMAND "${LINPHONESDK_DIR}/cmake/Android/gradlew" "clean"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	RESULT_VARIABLE _gradle_clean_result
)
if(_gradle_clean_result)
	message(FATAL_ERROR "Gradle clean failed")
endif()
