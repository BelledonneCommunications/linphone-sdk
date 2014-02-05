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

set(LINPHONE_BUILDER_TOOLCHAIN "" CACHE STRING "Toolchain to use for the build.")

# Include macros definition to be used by builders
include(cmake/LinphoneCMakeBuilder.cmake)

# Include toolchain definition if needed
if(NOT "${LINPHONE_BUILDER_TOOLCHAIN}" STREQUAL "")
	message("-- Using toolchain '${LINPHONE_BUILDER_TOOLCHAIN}'")
	include(toolchains/toolchain-${LINPHONE_BUILDER_TOOLCHAIN}.cmake)
	linphone_builder_apply_toolchain_flags()
endif(NOT "${LINPHONE_BUILDER_TOOLCHAIN}" STREQUAL "")

# Include project configuration according to the toolchain
include(configs/config-default.cmake)
if(NOT "${LINPHONE_BUILDER_TOOLCHAIN}" STREQUAL "")
	include(configs/config-${LINPHONE_BUILDER_TOOLCHAIN}.cmake)
else(NOT "${LINPHONE_BUILDER_TOOLCHAIN}" STREQUAL "")
	# Include the desktop configuration by default
	include(configs/config-desktop.cmake)
endif(NOT "${LINPHONE_BUILDER_TOOLCHAIN}" STREQUAL "")

# Include builders definitions
include(builders/CMakeLists.txt)
