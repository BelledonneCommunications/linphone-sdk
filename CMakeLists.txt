############################################################################
# CMakeLists.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

cmake_minimum_required(VERSION 2.8)

include(FeatureSummary)


set(LINPHONE_BUILDER_CONFIG_FILE "configs/config-desktop.cmake" CACHE STRING "Path to the linphone builder configuration file.")
set(LINPHONE_BUILDER_WORK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/WORK" CACHE PATH "Working directory for the build.")


# Include macros definition to be used by builders
include(cmake/LinphoneCMakeBuilder.cmake)


# Include toolchain definition
if(CMAKE_TOOLCHAIN_FILE)
	get_filename_component(CMAKE_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE} ABSOLUTE)
	message(STATUS "Using toolchain file '${CMAKE_TOOLCHAIN_FILE}'")
endif()


# Include project configuration according to the toolchain
get_filename_component(LINPHONE_BUILDER_CONFIG_FILE ${LINPHONE_BUILDER_CONFIG_FILE} ABSOLUTE)
message(STATUS "Using configuration file '${LINPHONE_BUILDER_CONFIG_FILE}'")
include(${LINPHONE_BUILDER_CONFIG_FILE})
linphone_builder_apply_flags()
linphone_builder_add_external_projects()


# Give a feature summary
if("${CMAKE_VERSION}" VERSION_GREATER "2.8.2")
	feature_summary(WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
	feature_summary(WHAT DISABLED_FEATURES DESCRIPTION "Disabled features:")
endif("${CMAKE_VERSION}" VERSION_GREATER "2.8.2")
