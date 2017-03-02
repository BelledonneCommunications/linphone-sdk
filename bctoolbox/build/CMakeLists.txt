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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

if(NOT CPACK_PACKAGE_NAME)
	set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
endif()

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Toolbox package used by Belledonne Communications projects")
set(CPACK_PACKAGE_VENDOR "Belledonne Communications")
set(CPACK_PACKAGE_CONTACT "jehan.monnier@linphone.org")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../COPYING")

set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${PROJECT_VERSION})

set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
"^${CMAKE_BINARY_DIR}"
"^${PROJECT_SOURCE_DIR}/\\\\.git.*"
)

#set(CPACK_COMPONENTS_ALL runtime devel)
#set(CPACK_COMPONENT_DEVEL_DEPENDS runtime)
#set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME ${PACKAGE})
#set(CPACK_COMPONENT_DEVEL_DISPLAY_NAME ${PACKAGE}-devel)

find_package(Git 1.7.10) # --count option of git rev-list is available only since (more or less) git 1.7.10
if(GIT_EXECUTABLE)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-list --count ${PROJECT_VERSION}..HEAD
		OUTPUT_VARIABLE PROJECT_VERSION_BUILD
		OUTPUT_STRIP_TRAILING_WHITESPACE
		WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	)
	# if BUILD VERSION is available, append it to package filename
	set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-${PROJECT_VERSION_BUILD}")

	set(CPACK_RPM_PACKAGE_RELEASE "${PROJECT_VERSION_BUILD}")
	set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION}-${PROJECT_VERSION_BUILD}")
endif()

set(CPACK_RPM_COMPONENT_INSTALL OFF)
if("${CPACK_GENERATOR}" STREQUAL "RPM")
	set (CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION ${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig ${CMAKE_INSTALL_FULL_LIBDIR}/cmake)
endif()

if("${CPACK_GENERATOR}" STREQUAL "RPM" OR "${CPACK_GENERATOR}" STREQUAL "DEB")
	if (NOT CPACK_PACKAGING_INSTALL_PREFIX AND CMAKE_INSTALL_PREFIX)
		set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
		message(STATUS "Setting packaging install prefix to ${CPACK_PACKAGING_INSTALL_PREFIX}")
	endif()
endif()

if("${CPACK_GENERATOR}" STREQUAL "DEB")
	set(CPACK_DEBIAN_PACKAGE_DEBUG ON)
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
	set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackEnv.cmake")
	file(WRITE "${CPACK_PROJECT_CONFIG_FILE}" "set(ENV{LD_LIBRARY_PATH} \"./${CMAKE_INSTALL_FULL_LIBDIR}\")")
	set(SHLIBS_FILE     "${CMAKE_CURRENT_BINARY_DIR}/shlibs")
	file(WRITE "${SHLIBS_FILE}" "libbctoolbox  ${BCTOOLBOX_SO_VERSION} ${CPACK_PACKAGE_NAME}\n")
	file(APPEND "${SHLIBS_FILE}" "libbctoolbox-tester  ${BCTOOLBOXTESTER_SO_VERSION} ${CPACK_PACKAGE_NAME}\n")
	execute_process(COMMAND chmod 644 "${SHLIBS_FILE}")
	set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA ${SHLIBS_FILE})
endif()

include(CPack)
