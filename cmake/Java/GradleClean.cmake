# ###########################################################################
# GradleClean.cmake
# Copyright (C) 2010-2024 Belledonne Communications, Grenoble France
#
# ###########################################################################
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
# ###########################################################################

configure_file("${LINPHONESDK_DIR}/cmake/Java/gradle/build.gradle.cmake" "build.gradle" @ONLY)
configure_file("${LINPHONESDK_DIR}/cmake/Java/gradle/gradle.properties.cmake" "gradle.properties" @ONLY)
configure_file("${LINPHONESDK_DIR}/cmake/Java/gradle/upload.gradle.cmake" "upload.gradle" @ONLY)

set(GRADLEW_COMMAND "${LINPHONESDK_DIR}/cmake/Java/gradlew")
if(WIN32)
	set(GRADLEW_COMMAND "${GRADLEW_COMMAND}.bat")
endif()

execute_process(
	COMMAND "${GRADLEW_COMMAND}" "clean"
	RESULT_VARIABLE _GRADLE_CLEAN_RESULT
)

if(_GRADLE_CLEAN_RESULT)
	message(FATAL_ERROR "Gradle clean failed")
endif()
