############################################################################
# LinphoneSdkUtils.cmake
# Copyright (C) 2010-2018 Belledonne Communications, Grenoble France
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

#TODO remove. All CMAKE_* variables should be inherited and passed to external projects
function(linphone_sdk_get_inherited_cmake_args)
	set(_inherited_vars
		CMAKE_BUILD_TYPE:STRING
		CMAKE_BUILD_PARALLEL_LEVEL:STRING
		CMAKE_C_COMPILER_LAUNCHER:PATH
		CMAKE_CXX_COMPILER_LAUNCHER:PATH
		CMAKE_EXTRA_GENERATOR:STRING
		CMAKE_GENERATOR_PLATFORM:STRING
		CMAKE_INSTALL_MESSAGE:STRING
		CMAKE_VERBOSE_MAKEFILE:BOOL
		CMAKE_CROSSCOMPILING:BOOL
		CMAKE_TOOLCHAIN_FILE:PATH
		CMAKE_SYSTEM_PROCESSOR:STRING
		CMAKE_HOST_SYSTEM_PROCESSOR:STRING
		CMAKE_SYSTEM_NAME:STRING
		CMAKE_HOST_SYSTEM_NAME:STRING
		CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES:LIST
		CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES:LIST
		CMAKE_STAGING_PREFIX:PATH
		CMAKE_INSTALL_RPATH:PATH
		CMAKE_SKIP_BUILD_RPATH:BOOL
		CMAKE_SKIP_RPATH:BOOL
		CMAKE_INSTALL_PREFIX:PATH
		CMAKE_PREFIX_PATH:PATH
		CMAKE_C_COMPILER:STRING
		CMAKE_C_FLAGS:STRING
		CMAKE_C_FLAGS_RELEASE:STRING
		CMAKE_C_FLAGS_DEBUG:STRING
		CMAKE_C_FLAGS_RELWITHDEBINFO:STRING
		CMAKE_CXX_COMPILER:STRING
		CMAKE_CXX_FLAGS:STRING
		CMAKE_CXX_FLAGS_RELEASE:STRING
		CMAKE_CXX_FLAGS_DEBUG:STRING
		CMAKE_CXX_FLAGS_RELWITHDEBINFO:STRING
		CMAKE_EXE_LINKER_FLAGS:STRING
		CMAKE_EXE_LINKER_FLAGS_DEBUG:STRING
		CMAKE_EXE_LINKER_FLAGS_RELEASE:STRING
		CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO:STRING
		CMAKE_MODULE_LINKER_FLAGS:STRING
		CMAKE_MODULE_LINKER_FLAGS_DEBUG:STRING
		CMAKE_MODULE_LINKER_FLAGS_RELEASE:STRING
		CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO:STRING
		CMAKE_OSX_DEPLOYMENT_TARGET:STRING
		CMAKE_RC_COMPILER:STRING
		CMAKE_SHARED_LINKER_FLAGS:STRING
		CMAKE_SHARED_LINKER_FLAGS_DEBUG:STRING
		CMAKE_SHARED_LINKER_FLAGS_RELEASE:STRING
		CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO:STRING
		CMAKE_MODULE_PATH:STRING
		CMAKE_CHECKS_CACHE_FILE:STRING
		CMAKE_SYSTEM_VERSION:STRING
		CMAKE_SYSTEM_PROCESSOR:STRING
	)

	set(_inherited_cmake_args)
	foreach(_var ${_inherited_vars})
		string(REPLACE ":" ";" _varname_and_vartype ${_var})
		list(GET _varname_and_vartype 0 _varname)
		list(GET _varname_and_vartype 1 _vartype)
		if (DEFINED ${_varname})
			list(APPEND _inherited_cmake_args "-D${_varname}:${_vartype}=${${_varname}}")
		endif()
	endforeach()

	set(_inherited_cmake_args ${_inherited_cmake_args} PARENT_SCOPE)
endfunction()

function(linphone_sdk_get_enable_cmake_args)
	set(_enable_cmake_args)
	get_cmake_property(_variable_names VARIABLES)

	foreach(_variable_name ${_variable_names})
		if( (_variable_name MATCHES "^ENABLE_.*$"  OR _variable_name MATCHES "^LINPHONE_BUILDER_.*$") AND NOT _variable_name MATCHES ".*_AVAILABLE$")
			list(APPEND _enable_cmake_args "-D${_variable_name}=${${_variable_name}}")
		endif()
	endforeach()

	set(_enable_cmake_args ${_enable_cmake_args} PARENT_SCOPE)
endfunction()

# Add SDK variables here to propagate them through submodules (this function is call from LinphoneSdkPlatform<..>.cmake to set projects variables)
# For full propagation, theses variables must be added to LINPHONE_BUILDER_EP_VARS in cmake-builder/cmake/CMakeLists.txt too.
function(linphone_sdk_get_sdk_cmake_args)
	set(_linphone_sdk_vars
		LINPHONESDK_STATE:STRING
		LINPHONESDK_BRANCH:STRING
		LINPHONESDK_VERSION:STRING
	)

	set(_linphone_sdk_cmake_vars)
	foreach(_var ${_linphone_sdk_vars})
		string(REPLACE ":" ";" _varname_and_vartype ${_var})
		list(GET _varname_and_vartype 0 _varname)
		list(GET _varname_and_vartype 1 _vartype)
		if (DEFINED ${_varname})
			list(APPEND _linphone_sdk_cmake_vars "-D${_varname}:${_vartype}=${${_varname}}")
		endif()
	endforeach()

	set(_linphone_sdk_cmake_vars ${_linphone_sdk_cmake_vars} PARENT_SCOPE)
endfunction()

macro(linphone_sdk_convert_comma_separated_list_to_cmake_list INPUT OUTPUT)
	string(REPLACE " " "" ${OUTPUT} "${INPUT}")
	string(REPLACE "," ";" ${OUTPUT} "${${OUTPUT}}")
endmacro()

macro(linphone_sdk_check_git)
	find_program(GIT_EXECUTABLE git NAMES Git CMAKE_FIND_ROOT_PATH_BOTH)
endmacro()

macro(linphone_sdk_compute_full_version OUTPUT_VERSION)
	linphone_sdk_check_git()
	if(GIT_EXECUTABLE)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" "describe" "--abbrev=0"
			OUTPUT_VARIABLE GIT_OUTPUT_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		)
		if (DEFINED GIT_OUTPUT_VERSION)

			# In case we need to decompose the version
			# if (NOT GIT_OUTPUT_VERSION MATCHES "^(0|[1-9][0-9]*)[.](0|[1-9][0-9]*)[.](0|[1-9][0-9]*)(-[.0-9A-Za-z-]+)?([+][.0-9A-Za-z-]+)?$")
			#     set( version_major "${CMAKE_MATCH_1}" )
			#     set( version_minor "${CMAKE_MATCH_2}" )
			#     set( version_patch "${CMAKE_MATCH_3}" )
			#     set( identifiers   "${CMAKE_MATCH_4}" )
			#     set( metadata      "${CMAKE_MATCH_5}" )
			# endif()

			SET(${OUTPUT_VERSION} "${GIT_OUTPUT_VERSION}")

			linphone_sdk_compute_commits_count_since_latest_tag(${GIT_OUTPUT_VERSION} COMMIT_COUNT)
			if (NOT ${COMMIT_COUNT} STREQUAL "0")
			   	execute_process(
					COMMAND "${GIT_EXECUTABLE}" "rev-parse" "--short" "HEAD"
					OUTPUT_VARIABLE COMMIT_HASH
					OUTPUT_STRIP_TRAILING_WHITESPACE
					ERROR_QUIET
					WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
				)
				set(${OUTPUT_VERSION} "${${OUTPUT_VERSION}}.${COMMIT_COUNT}+${COMMIT_HASH}")
			else()
				#If commit count diff is 0, it means we are on the tag. Keep the version untouched
			endif()
		endif()

		if (DEFINED ${OUTPUT_VERSION})
			execute_process(
				COMMAND "${GIT_EXECUTABLE}" "describe" "--abbrev=0"
				OUTPUT_VARIABLE PROJECT_GIT_TAG
				OUTPUT_STRIP_TRAILING_WHITESPACE
				ERROR_QUIET
				WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			)

			if(PROJECT_GIT_TAG VERSION_LESS PROJECT_VERSION)
				message(FATAL_ERROR "Project version (${PROJECT_VERSION}) and git tag (${PROJECT_GIT_TAG}) differ! Please synchronize them.")
			endif()
			unset(PROJECT_GIT_TAG)
		endif()
	endif()
	message(STATUS "Building LinphoneSDK Version : [${${OUTPUT_VERSION}}]")
endmacro()

macro(linphone_sdk_compute_git_branch OUTPUT_GIT_BRANCH)
	linphone_sdk_check_git()
	if(GIT_EXECUTABLE)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" "name-rev" "--name-only" "HEAD"
			OUTPUT_VARIABLE ${OUTPUT_GIT_BRANCH}
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		)
	endif()
endmacro()

macro(linphone_sdk_get_latest_tag OUTPUT_TAG)
	linphone_sdk_check_git()
	if(GIT_EXECUTABLE)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" "describe" "--abbrev=0"
			OUTPUT_VARIABLE ${OUTPUT_TAG}
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		)
	endif()
endmacro()

macro(linphone_sdk_compute_commits_count_since_latest_tag LATEST_TAG OUTPUT_COMMITS_COUNT)
	linphone_sdk_check_git()
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

macro(linphone_sdk_check_is_installed EXECUTABLE_NAME)
	string(TOUPPER "${EXECUTABLE_NAME}" _upper_executable_name)
	find_program(LINPHONESDK_${_upper_executable_name}_PROGRAM ${EXECUTABLE_NAME})
	if(NOT LINPHONESDK_${_upper_executable_name}_PROGRAM)
		message(FATAL_ERROR "${EXECUTABLE_NAME} has not been found on your system. Please install it.")
	endif()
	mark_as_advanced(LINPHONESDK_${_upper_executable_name}_PROGRAM)
endmacro()

function(linphone_sdk_check_python_module_is_installed MODULE_NAME)
	execute_process(
		COMMAND "${PYTHON_EXECUTABLE}" "-c" "import ${MODULE_NAME}"
		RESULT_VARIABLE _result
		OUTPUT_QUIET
		ERROR_QUIET
	)
	if(_result EQUAL 0)
		message(STATUS "'${MODULE_NAME}' python module found")
	else()
		message(FATAL_ERROR "'${MODULE_NAME}' python module not found")
	endif()
endfunction()

function(ExcludeFromList resultVar excludePattern)
	set(result)
	foreach(ITR ${ARGN})  # ARGN holds all arguments to function after last named one
	    if(NOT ITR MATCHES "(.*)${excludePattern}(.*)")
	        list(APPEND result ${ITR})
	    endif()
        endforeach()
	set(${resultVar} ${result} PARENT_SCOPE)
endfunction()
