############################################################################
# BelcardConfig.cmake
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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
#
# Config file for the belcard package.
# It defines the following variables:
#
#  BELCARD_FOUND - system has belcard
#  BELCARD_INCLUDE_DIRS - the belcard include directory
#  BELCARD_LIBRARIES - The libraries needed to use belcard

if(NOT LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	include("${CMAKE_CURRENT_LIST_DIR}/BelcardTargets.cmake")
endif()

if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	include("${EP_belr_CONFIG_DIR}/BelrConfig.cmake")
else()
	find_package(Belr REQUIRED)
endif()

get_filename_component(BELCARD_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	set(BELCARD_INCLUDE_DIRS "${EP_belcard_INCLUDE_DIR}")
else()
	set(BELCARD_INCLUDE_DIRS "${BELCARD_CMAKE_DIR}/../../../include")
endif()

if(BELR_FOUND)
	list(APPEND BELCARD_INCLUDE_DIRS ${BELR_INCLUDE_DIRS})
	list(APPEND BELCARD_LIBRARIES ${BELR_LIBRARIES})
endif()

set(BELCARD_LIBRARIES belcard)
set(BELCARD_FOUND 1)
