############################################################################
# BcToolboxCMakeUtils.cmake
# Copyright (C) 2010-2019 Belledonne Communications, Grenoble France
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

macro(bc_check_git)
	find_program(GIT_EXECUTABLE git NAMES Git CMAKE_FIND_ROOT_PATH_BOTH)
endmacro()

macro(bc_init_compilation_flags CPP_FLAGS C_FLAGS CXX_FLAGS STRICT_COMPILATION)
	set(${CPP_FLAGS} )
	set(${C_FLAGS} )
	set(${CXX_FLAGS} )
	if(MSVC)
		if(${STRICT_COMPILATION})
			list(APPEND ${CPP_FLAGS} "/WX")
			list(APPEND STRICT_OPTIONS_CPP "/wd4996") # Disable deprecated function warnings
		endif()
	else()
		list(APPEND ${CPP_FLAGS} "-Wall" "-Wuninitialized")
		if(CMAKE_C_COMPILER_ID MATCHES "Clang")
			list(APPEND ${CPP_FLAGS} "-Wno-error=unknown-warning-option" "-Qunused-arguments" "-Wno-tautological-compare" "-Wno-builtin-requires-header" "-Wno-unused-function" "-Wno-gnu-designator" "-Wno-array-bounds")
		elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
			list(APPEND ${CPP_FLAGS} "-Wno-error=pragmas")
		endif()
		if(APPLE)
			list(APPEND ${CPP_FLAGS} "-Wno-error=unknown-warning-option" "-Qunused-arguments" "-Wno-tautological-compare" "-Wno-unused-function" "-Wno-array-bounds")
		endif()
		if(ENABLE_STRICT)
			list(APPEND ${CPP_FLAGS} "-Werror" "-Wextra" "-Wno-unused-parameter" "-Wno-error=unknown-pragmas" "-Wuninitialized" "-Wno-missing-field-initializers"
				"-fno-strict-aliasing" "-Wno-error=deprecated" "-Wno-error=deprecated-declarations")
			list(APPEND ${C_FLAGS} "-Werror" "-Wstrict-prototypes")
		endif()
	endif()
endmacro()

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


# TODO remove this macro
macro(bc_project_build_version PROJECT_VERSION PROJECT_BUILD_VERSION)
	find_program (WC wc)

	if(WC)
		set(GIT_MINIMUM_VERSION 1.7.1) #might be even lower
	else()
		set(GIT_MINIMUM_VERSION 1.7.10) # --count option of git rev-list is available only since (more or less) git 1.7.10)
	endif()

	find_package(Git ${GIT_MINIMUM_VERSION})
	string(COMPARE GREATER "${GIT_VERSION_STRING}" "1.7.10" GIT_REV_LIST_HAS_COUNT)

	if(GIT_REV_LIST_HAS_COUNT)
		set(GIT_REV_LIST_COMMAND "${GIT_EXECUTABLE}" "rev-list" "--count" "${PROJECT_VERSION}..HEAD")
		set(WC_COMMAND "more") #nop
	else()
		set(GIT_REV_LIST_COMMAND "${GIT_EXECUTABLE}" "rev-list" "${PROJECT_VERSION}..HEAD")
		set(WC_COMMAND "${WC}" "-l")
	endif()

	if(GIT_EXECUTABLE)
		execute_process(
			COMMAND ${GIT_REV_LIST_COMMAND}
			COMMAND ${WC_COMMAND}
			OUTPUT_VARIABLE PROJECT_VERSION_BUILD
			OUTPUT_STRIP_TRAILING_WHITESPACE
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		)
	endif()
	if (NOT PROJECT_VERSION_BUILD)
		set(PROJECT_VERSION_BUILD 0)
	endif()
endmacro()

macro(bc_generate_rpm_specfile SOURCE DEST)
	if(UNIX AND NOT APPLE AND NOT ANDROID)
		set(BC_PACKAGE_NAME_PREFIX "" CACHE STRING "Prefix for name of package.")

		get_cmake_property(_variableNames VARIABLES)
		set(RPM_ALL_CMAKE_OPTIONS "")
		foreach(_variableName ${_variableNames})
			if(_variableName MATCHES "^ENABLE_")
				if(${_variableName})
					set(${_variableName} 1)
				else()
					set(${_variableName} 0)
				endif()
				set(RPM_ALL_CMAKE_OPTIONS "${RPM_ALL_CMAKE_OPTIONS} -D${_variableName}=${${_variableName}}")
			endif()
		endforeach()
		configure_file(${SOURCE} ${DEST} @ONLY)
		unset(RPM_ALL_CMAKE_OPTIONS)
		unset(_variableNames)
	endif()
endmacro()

function(bc_parse_full_version version major minor patch)
	if ("${version}" MATCHES "^(0|[1-9][0-9]*)[.](0|[1-9][0-9]*)[.](0|[1-9][0-9]*)(-[.0-9A-Za-z-]+)?([+][.0-9A-Za-z-]+)?$")
	    set(${major}       "${CMAKE_MATCH_1}" PARENT_SCOPE)
	    set(${minor}       "${CMAKE_MATCH_2}" PARENT_SCOPE)
	    set(${patch}       "${CMAKE_MATCH_3}" PARENT_SCOPE)
		if (ARGC GREATER 4)
			set(${ARGV4} "${CMAKE_MATCH_4}" PARENT_SCOPE)
		endif()
		if (ARGC GREATER 5)
			set(${ARGV5}    "${CMAKE_MATCH_5}" PARENT_SCOPE)
		endif()
	else()
		message(FATAL_ERROR "invalid full version '${version}'")
	endif()
endfunction()

function(bc_compute_full_version OUTPUT_VERSION)
	bc_check_git()
	if(GIT_EXECUTABLE)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" "describe" "--abbrev=0"
			OUTPUT_VARIABLE GIT_OUTPUT_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		)
		if (DEFINED GIT_OUTPUT_VERSION)
			set(output_version "${GIT_OUTPUT_VERSION}")
			bc_compute_commits_count_since_latest_tag(${GIT_OUTPUT_VERSION} COMMIT_COUNT)
			if (NOT ${COMMIT_COUNT} STREQUAL "0")
				execute_process(
					COMMAND "${GIT_EXECUTABLE}" "rev-parse" "--short" "HEAD"
					OUTPUT_VARIABLE COMMIT_HASH
					OUTPUT_STRIP_TRAILING_WHITESPACE
					ERROR_QUIET
					WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
				)
				string(APPEND output_version ".${COMMIT_COUNT}+${COMMIT_HASH}")
			else()
				#If commit count diff is 0, it means we are on the tag. Keep the version untouched
			endif()

			set(version_major )
			set(version_minor )
			set(version_patch )
			bc_parse_full_version("${output_version}" version_major version_minor version_patch)
			set(short_version "${version_major}.${version_minor}.${version_patch}")
			if (PROJECT_VERSION AND NOT (short_version VERSION_EQUAL PROJECT_VERSION))
				message(FATAL_ERROR "project and git version mismatch (project: '${PROJECT_VERSION}', git: '${short_version}')")
			endif()

			set(${OUTPUT_VERSION} "${output_version}" PARENT_SCOPE)
		endif()
	endif()
endfunction()

macro(bc_compute_commits_count_since_latest_tag LATEST_TAG OUTPUT_COMMITS_COUNT)
	bc_check_git()
	if(GIT_EXECUTABLE)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" "rev-list" "${LATEST_TAG}..HEAD" "--count"
			OUTPUT_VARIABLE ${OUTPUT_COMMITS_COUNT}
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		)
	endif()
endmacro()

function(bc_make_package_source_target)
	set(basename "")
	string(TOLOWER "${CMAKE_PROJECT_NAME}" basename)
	if (DEFINED BC_SPECFILE_NAME)
		set(specfile_name "${BC_SPECFILE_NAME}")
	else()
		set(specfile_name "${basename}.spec")
	endif()
	set(specfile_target "${basename}-rpm-spec")

	bc_generate_rpm_specfile("rpm/${specfile_name}.cmake" "rpm/${specfile_name}.cmake")

	add_custom_target(${specfile_target}
	    COMMAND ${CMAKE_COMMAND}
	        "-DPROJECT_VERSION=${PROJECT_VERSION}"
	        "-DBCTOOLBOX_CMAKE_UTILS=${BCTOOLBOX_CMAKE_UTILS}"
	        "-DSRC=${CMAKE_CURRENT_BINARY_DIR}/rpm/${specfile_name}.cmake"
	        "-DDEST=${PROJECT_SOURCE_DIR}/${specfile_name}"
	        -P "${BCTOOLBOX_CMAKE_DIR}/ConfigureSpecfile.cmake"
	    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	    BYPRODUCTS "${PROJECT_SOURCE_DIR}/${specfile_name}"
	)

	add_custom_target(package_source
	    COMMAND ${CMAKE_COMMAND}
	        "-DBCTOOLBOX_CMAKE_UTILS=${BCTOOLBOX_CMAKE_UTILS}"
	        "-DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
	        "-DPROJECT_BINARY_DIR=${PROJECT_BINARY_DIR}"
	        "-DPROJECT_VERSION=${PROJECT_VERSION}"
	        "-DCPACK_PACKAGE_NAME=${CPACK_PACKAGE_NAME}"
	        "-DEXCLUDE_PATTERNS='${CPACK_SOURCE_IGNORE_FILES}'"
	        -P "${BCTOOLBOX_CMAKE_DIR}/MakeArchive.cmake"
	    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	    DEPENDS ${specfile_target}
	)
endfunction()
