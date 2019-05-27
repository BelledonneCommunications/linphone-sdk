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

function(linphone_sdk_get_inherited_cmake_args)
	set(_inherited_vars
		CMAKE_BUILD_TYPE:STRING
		CMAKE_C_COMPILER_LAUNCHER:PATH
		CMAKE_CXX_COMPILER_LAUNCHER:PATH
		CMAKE_EXTRA_GENERATOR:STRING
		CMAKE_GENERATOR_PLATFORM:STRING
		CMAKE_INSTALL_MESSAGE:STRING
		CMAKE_VERBOSE_MAKEFILE:BOOL
	)

	set(_inherited_cmake_args)
	foreach(_var ${_inherited_vars})
		string(REPLACE ":" ";" _varname_and_vartype ${_var})
		list(GET _varname_and_vartype 0 _varname)
		list(GET _varname_and_vartype 1 _vartype)
		list(APPEND _inherited_cmake_args "-D${_varname}:${_vartype}=${${_varname}}")
	endforeach()

	set(_inherited_cmake_args ${_inherited_cmake_args} PARENT_SCOPE)
endfunction()

function(linphone_sdk_get_enable_cmake_args)
	set(_enable_cmake_args)
	get_cmake_property(_variable_names VARIABLES)

	foreach(_variable_name ${_variable_names})
		if(_variable_name MATCHES "^ENABLE_.*$" AND NOT _variable_name MATCHES ".*_AVAILABLE$")
			list(APPEND _enable_cmake_args "-D${_variable_name}=${${_variable_name}}")
		endif()
	endforeach()

	set(_enable_cmake_args ${_enable_cmake_args} PARENT_SCOPE)
endfunction()

macro(linphone_sdk_convert_comma_separated_list_to_cmake_list INPUT OUTPUT)
	string(REPLACE " " "" ${OUTPUT} "${INPUT}")
	string(REPLACE "," ";" ${OUTPUT} "${${OUTPUT}}")
endmacro()

macro(linphone_sdk_check_git)
	find_package(Git 1.7.10)
endmacro()

function(linphone_sdk_git_submodule_update)
	if(NOT EXISTS "${LINPHONESDK_DIR}/linphone/CMakeLists.txt")
		linphone_sdk_check_git()
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" "submodule" "update" "--recursive" "--init"
			WORKING_DIRECTORY "${LINPHONESDK_DIR}"
		)
	endif()
endfunction()

macro(linphone_sdk_compute_full_version OUTPUT_VERSION)
	linphone_sdk_check_git()
	if(GIT_EXECUTABLE)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" "describe"
			OUTPUT_VARIABLE ${OUTPUT_VERSION}
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		)
		if(${OUTPUT_VERSION})
			execute_process(
				COMMAND "${GIT_EXECUTABLE}" "describe" "--abbrev=0"
				OUTPUT_VARIABLE PROJECT_GIT_TAG
				OUTPUT_STRIP_TRAILING_WHITESPACE
				ERROR_QUIET
				WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			)
			if(NOT PROJECT_GIT_TAG VERSION_EQUAL PROJECT_VERSION)
				message(FATAL_ERROR "Project version (${PROJECT_VERSION}) and git tag (${PROJECT_GIT_TAG}) differ! Please synchronize them.")
			endif()
			unset(PROJECT_GIT_TAG)
		else()
			execute_process(
				COMMAND "${GIT_EXECUTABLE}" "rev-parse" "HEAD"
				OUTPUT_VARIABLE PROJECT_GIT_REVISION
				OUTPUT_STRIP_TRAILING_WHITESPACE
				ERROR_QUIET
				WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			)
			set(${OUTPUT_VERSION} "${PROJECT_VERSION}-g${PROJECT_GIT_REVISION}")
		endif()
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
