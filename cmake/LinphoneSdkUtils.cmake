############################################################################
# LinphoneSdkUtils.cmake
# Copyright (C) 2010-2024 Belledonne Communications, Grenoble France
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

function(linphone_sdk_get_inherited_cmake_args OUTPUT_CONFIGURE_ARGS_VAR OUTPUT_BUILD_ARGS_VAR)
	set(_CMAKE_CONFIGURE_ARGS)
	set(_CMAKE_BUILD_ARGS)

	if(NOT "${CMAKE_GENERATOR}" STREQUAL "")
	  list(APPEND _CMAKE_CONFIGURE_ARGS "-G ${CMAKE_GENERATOR}")
	endif()
	if(NOT "${CMAKE_BUILD_PARALLEL_LEVEL}" STREQUAL "")
	  list(APPEND _CMAKE_BUILD_ARGS "--parallel ${CMAKE_BUILD_PARALLEL_LEVEL}")
	endif()
	if(CMAKE_VERBOSE_MAKEFILE)
	  list(APPEND _CMAKE_BUILD_ARGS "--verbose")
	endif()
	if(APPLE AND NOT "${CMAKE_OSX_DEPLOYMENT_TARGET}" STREQUAL "")
	  list(APPEND _CMAKE_CONFIGURE_ARGS "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
	endif()
	if(CMAKE_GENERATOR STREQUAL "Ninja Multi-Config" OR CMAKE_GENERATOR STREQUAL "Xcode" OR CMAKE_GENERATOR MATCHES "^Visual Studio")
		list(APPEND _CMAKE_BUILD_ARGS "--config $<CONFIG>")
	elseif(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
		list(APPEND _CMAKE_CONFIGURE_ARGS "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
	endif()

	set("${OUTPUT_CONFIGURE_ARGS_VAR}" ${_CMAKE_CONFIGURE_ARGS} PARENT_SCOPE)
	set("${OUTPUT_BUILD_ARGS_VAR}" ${_CMAKE_BUILD_ARGS} PARENT_SCOPE)
endfunction()

function(linphone_sdk_get_enable_cmake_args OUTPUT_VAR)
	set(_ENABLE_CMAKE_ARGS)
	get_cmake_property(_VARIABLE_NAMES VARIABLES)

	foreach(_VARIABLE_NAME ${_VARIABLE_NAMES})
		if((_VARIABLE_NAME MATCHES "^ENABLE_.*$" AND NOT _VARIABLE_NAME MATCHES "^.*_AVAILABLE$") OR (_VARIABLE_NAME MATCHES "^BUILD_.*$"))
			list(APPEND _ENABLE_CMAKE_ARGS "-D${_VARIABLE_NAME}=${${_VARIABLE_NAME}}")
		endif()
	endforeach()

	set("${OUTPUT_VAR}" ${_ENABLE_CMAKE_ARGS} PARENT_SCOPE)
endfunction()

function(linphone_sdk_get_sdk_cmake_args OUTPUT_VAR)
	set(_SDK_CMAKE_ARGS)
	get_cmake_property(_VARIABLE_NAMES VARIABLES)

	foreach(_VARIABLE_NAME ${_VARIABLE_NAMES})
		if ((_VARIABLE_NAME MATCHES "^LINPHONESDK_.*$"))
			list(APPEND _SDK_CMAKE_ARGS "-D${_VARIABLE_NAME}=${${_VARIABLE_NAME}}")
		endif()
	endforeach()

	set("${OUTPUT_VAR}" ${_SDK_CMAKE_ARGS} PARENT_SCOPE)
endfunction()

macro(linphone_sdk_convert_comma_separated_list_to_cmake_list INPUT OUTPUT)
	string(REPLACE " " "" ${OUTPUT} "${INPUT}")
	string(REPLACE "," ";" ${OUTPUT} "${${OUTPUT}}")
endmacro()

macro(linphone_sdk_check_git)
	find_program(GIT_EXECUTABLE git NAMES Git CMAKE_FIND_ROOT_PATH_BOTH)
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

macro(linphone_sdk_check_is_installed EXECUTABLE_NAME)
	string(TOUPPER "${EXECUTABLE_NAME}" _upper_executable_name)
	if (win32)
		find_program(LINPHONESDK_${_upper_executable_name}_PROGRAM
			NAMES ${EXECUTABLE_NAME} ${EXECUTABLE_NAME}.exe
			HINTS ${_DEFAULT_MSYS2_BIN_PATH}
		)
	else()
		find_program(LINPHONESDK_${_upper_executable_name}_PROGRAM ${EXECUTABLE_NAME})
	endif()

	if(NOT LINPHONESDK_${_upper_executable_name}_PROGRAM)
		message(FATAL_ERROR "${EXECUTABLE_NAME} has not been found on your system. Please install it.")
	endif()
	#Escape spaces in paths as find_program doesn't do it for some reason...
	set(LINPHONESDK_${_upper_executable_name}_PROGRAM "\"${LINPHONESDK_${_upper_executable_name}_PROGRAM}\"" CACHE FILEPATH "Path to a program" FORCE)
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

function(linphone_sdk_add_dummy_library LIBRARY_NAME)
	if(APPLE)
		string(REGEX REPLACE "^lib" "" NOPREFIX_LIBRARY_NAME ${LIBRARY_NAME})
		set(LIBRARY_NAME "lib${NOPREFIX_LIBRARY_NAME}")
		if(NOT EXISTS "${CMAKE_INSTALL_PREFIX}/Frameworks/${LIBRARY_NAME}.framework")
			message(STATUS "Generating dummy library ${LIBRARY_NAME}")
		  configure_file("${CMAKE_CURRENT_LIST_DIR}/dummy_libraries/dummy.c.in" "${PROJECT_BINARY_DIR}/${LIBRARY_NAME}.c" @ONLY)
			add_library(${NOPREFIX_LIBRARY_NAME} SHARED "${PROJECT_BINARY_DIR}/${LIBRARY_NAME}.c")
			configure_file("${CMAKE_CURRENT_LIST_DIR}/dummy_libraries/Info.plist.in" "${PROJECT_BINARY_DIR}/Info_${NOPREFIX_LIBRARY_NAME}.plist" @ONLY)
			set_target_properties(${NOPREFIX_LIBRARY_NAME} PROPERTIES
				FRAMEWORK TRUE
				MACOSX_FRAMEWORK_IDENTIFIER org.linphone.${NOPREFIX_LIBRARY_NAME}
				MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_BINARY_DIR}/Info_${NOPREFIX_LIBRARY_NAME}.plist"
			)
			install(TARGETS ${NOPREFIX_LIBRARY_NAME}
				LIBRARY DESTINATION "Frameworks"
				ARCHIVE DESTINATION "Frameworks"
				FRAMEWORK DESTINATION "Frameworks"
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)
		endif()
	endif()
endfunction()
