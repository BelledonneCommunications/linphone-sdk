# ###########################################################################
# TasksJava.cmake
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

include(GNUInstallDirs)

# ###########################################################################
# Run gradle clean and generate gradle build scripts
# ###########################################################################

add_custom_target(gradle-clean ALL
  COMMAND "${CMAKE_COMMAND}"
  "-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}"
  "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}"
  "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}"
  "-DLINPHONESDK_STATE=${LINPHONESDK_STATE}"
  "-DLINPHONESDK_BRANCH=${LINPHONESDK_BRANCH}"
  "-P" "${PROJECT_SOURCE_DIR}/cmake/Java/GradleClean.cmake"
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  COMMENT "Generating gradle build scripts and performing gradle clean"
  USES_TERMINAL
)

# ###########################################################################
# Build liblinphone
# ###########################################################################
linphone_sdk_get_inherited_cmake_args(_CMAKE_CONFIGURE_ARGS _CMAKE_BUILD_ARGS)
linphone_sdk_get_enable_cmake_args(_JAVA_CMAKE_ARGS)

set(_JAVA_BINARY_DIR "${PROJECT_BINARY_DIR}/java")
set(_JAVA_INSTALL_DIR "${PROJECT_BINARY_DIR}/linphone-sdk/java")
add_custom_target(liblinphone-java ALL
  COMMAND ${CMAKE_COMMAND} --preset=default -B ${_JAVA_BINARY_DIR} ${_CMAKE_CONFIGURE_ARGS} ${_JAVA_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=${_JAVA_INSTALL_DIR}
  COMMAND ${CMAKE_COMMAND} --build ${_JAVA_BINARY_DIR} --target install ${_CMAKE_BUILD_ARGS}
  DEPENDS gradle-clean
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMENT "Building Linphone SDK for Java"
  USES_TERMINAL
  COMMAND_EXPAND_LISTS
)

# ###########################################################################
# Generate the SDK
# ###########################################################################
add_custom_target(copy-libs ALL
  COMMAND "${CMAKE_COMMAND}"
  "-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}"
  "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}"
  "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  "-DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
  "-P" "${PROJECT_SOURCE_DIR}/cmake/Java/CopyLibs.cmake"
  DEPENDS liblinphone-java
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  COMMENT "Copying libs"
  USES_TERMINAL
)

add_custom_target(sdk ALL
  COMMAND "${CMAKE_COMMAND}"
  "-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}"
  "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}"
  "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  "-P" "${PROJECT_SOURCE_DIR}/cmake/Java/GenerateSDK.cmake"
  DEPENDS copy-libs
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  COMMENT "Generating the SDK (zip file)"
  USES_TERMINAL
)
