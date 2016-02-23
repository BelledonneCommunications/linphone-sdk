############################################################################
# CMakeLists.txt
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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

if(NOT CPACK_PACKAGE_NAME)
	set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
endif()

if (NOT CPACK_PACKAGING_INSTALL_PREFIX AND CMAKE_INSTALL_PREFIX)
	set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
	message("Setting packaging install prefix to ${CPACK_PACKAGING_INSTALL_PREFIX}")
endif()

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Toolbox package used by Belledonne Communications projects")
set(CPACK_PACKAGE_VENDOR "Belledonne Communications")
set(CPACK_PACKAGE_CONTACT "jehan.monnier@linphone.org")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../COPYING")

set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${PROJECT_VERSION})

set(CPACK_SOURCE_GENERATOR "TGZ")

#set(CPACK_COMPONENTS_ALL runtime devel)
#set(CPACK_COMPONENT_DEVEL_DEPENDS runtime)
#set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME ${PACKAGE})
#set(CPACK_COMPONENT_DEVEL_DISPLAY_NAME ${PACKAGE}-devel)

set(CPACK_RPM_COMPONENT_INSTALL OFF)
if("${CPACK_GENERATOR}" STREQUAL "RPM")
	set (CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION ${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig ${CMAKE_INSTALL_FULL_LIBDIR}/cmake)
endif()

include(CPack)
