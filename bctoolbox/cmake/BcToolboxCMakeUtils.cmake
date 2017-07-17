############################################################################
# BcToolboxCMakeUtils.cmake
# Copyright (C) 2017  Belledonne Communications, Grenoble France
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

set(BCTOOLBOX_CMAKE_UTILS_DIR "${CMAKE_CURRENT_LIST_DIR}")

macro(bc_apply_compile_flags SOURCE_FILES)
	if(${SOURCE_FILES})
		set(options "")
		foreach(a ${ARGN})
			if(${a})
				string(REPLACE ";" " " options_${a} "${${a}}")
				set(options "${options} ${options_${a}}")
			endif()
		endforeach()
		if(options)
			set_source_files_properties(${${SOURCE_FILES}} PROPERTIES COMPILE_FLAGS "${options}")
		endif()
	endif()
endmacro()

macro(bc_git_version PROJECT_NAME PROJECT_VERSION)
	find_package(Git)
	add_custom_target(${PROJECT_NAME}-git-version
		COMMAND ${CMAKE_COMMAND} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -DPROJECT_NAME=${PROJECT_NAME} -DPROJECT_VERSION=${PROJECT_VERSION} -DWORK_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DTEMPLATE_DIR=${BCTOOLBOX_CMAKE_UTILS_DIR} -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR} -P ${BCTOOLBOX_CMAKE_UTILS_DIR}/BcGitVersion.cmake
		BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/gitversion.h"
	)
endmacro()


macro(bc_project_build_version PROJECT_VERSION PROJECT_BUILD_VERSION) 
	find_program (WC wc)

	if (WC)
       		 set(GIT_MINIMUN_VERSION 1.7.1) #might be even lower
	else()
       		 set(GIT_MINIMUN_VERSION 1.7.10) # --count option of git rev-list is available only since (more or less) git 1.7.10)
	endif()

	find_package(Git ${GIT_MINIMUN_VERSION})
	string(COMPARE GREATER "${GIT_VERSION_STRING}" "1.7.10" GIT_REV_LIST_HAS_COUNT)

	if (GIT_REV_LIST_HAS_COUNT)
       		 set(GIT_REV_LIST_COMMAND "${GIT_EXECUTABLE}" "rev-list" "--count" "${PROJECT_VERSION}..HEAD")
       		 set(WC_COMMAND  "more") #nop
	else()
       		 set(GIT_REV_LIST_COMMAND "${GIT_EXECUTABLE}" "rev-list" "${PROJECT_VERSION}..HEAD")
       		 set(WC_COMMAND "${WC}"  "-l")
	endif()

	if(GIT_EXECUTABLE)
       		 execute_process(
       		         COMMAND ${GIT_REV_LIST_COMMAND}
       		         COMMAND ${WC_COMMAND}
       		         OUTPUT_VARIABLE PROJECT_VERSION_BUILD
       		         OUTPUT_STRIP_TRAILING_WHITESPACE
       		         WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
       		 )
	endif()
	if (NOT PROJECT_VERSION_BUILD) 
		set(PROJECT_VERSION_BUILD 0)
	endif()
endmacro()
