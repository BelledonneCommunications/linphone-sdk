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
	find_program(GIT_EXECUTABLE git NAMES Git CMAKE_FIND_ROOT_PATH_BOTH)
	if(GIT_EXECUTABLE)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" "describe"
			OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		)

		# parse git describe version
		if (NOT (GIT_DESCRIBE_VERSION MATCHES "^([0-9]+)[.]([0-9]+)[.]([0-9]+)(-alpha|-beta)?(-[0-9]+)?(-g[0-9a-f]+)?$"))
			message(FATAL_ERROR "invalid git describe version: '${GIT_DESCRIBE_VERSION}'")
		endif()
		set(version_major ${CMAKE_MATCH_1})
		set(version_minor ${CMAKE_MATCH_2})
		set(version_patch ${CMAKE_MATCH_3})
		if (CMAKE_MATCH_4)
			string(SUBSTRING "${CMAKE_MATCH_4}" 1 -1 version_prerelease)
		endif()
		if (CMAKE_MATCH_5)
			string(SUBSTRING "${CMAKE_MATCH_5}" 1 -1 version_commit)
		endif()
		if (CMAKE_MATCH_6)
			string(SUBSTRING "${CMAKE_MATCH_6}" 2 -1 version_hash)
		endif()

		# interpret untagged hotfixes as pre-releases of the next "patch" release
		if (NOT version_prerelease AND version_commit)
			math(EXPR version_patch "${version_patch} + 1")
			set(version_prerelease "pre")
		endif()

		# format full version
		set(full_version "${version_major}.${version_minor}.${version_patch}")
		if (version_prerelease)
			string(APPEND full_version "-${version_prerelease}")
			if (version_commit)
				string(APPEND full_version ".${version_commit}+${version_hash}")
			endif()
		endif()

		# check that the major and minor versions declared by the `project()` command are equal to the ones
		# that have been found out while parsing `git describe` result.
		if (PROJECT_VERSION)
			set(short_git_version "${version_major}.${version_minor}")
			set(short_project_version "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
			if(NOT (short_project_version VERSION_EQUAL short_git_version))
				message(FATAL_ERROR
					"project and git version are not compatible (project: '${PROJECT_VERSION}', git: '${full_version}'): "
					"major and minor version are not equal !"
				)
			endif()
		endif()

		set(${OUTPUT_VERSION} "${full_version}" CACHE STRING "")
	endif()
endfunction()

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
	        "-DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}"
	        "-DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}"
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
	        "-DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}"
	        "-DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}"
	        "-DCPACK_PACKAGE_NAME=${CPACK_PACKAGE_NAME}"
	        "-DEXCLUDE_PATTERNS='${CPACK_SOURCE_IGNORE_FILES}'"
	        -P "${BCTOOLBOX_CMAKE_DIR}/MakeArchive.cmake"
	    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	    DEPENDS ${specfile_target}
	)
endfunction()
