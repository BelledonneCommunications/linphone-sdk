############################################################################
# LinphoneCMakeBuilder.cmake
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

include(ExternalProject)

set(ep_base ${CMAKE_CURRENT_SOURCE_DIR}/WORK)
set_property(DIRECTORY PROPERTY EP_BASE ${ep_base})


if(${CMAKE_VERBOSE_MAKEFILE})
	set(AUTOTOOLS_VERBOSE_MAKEFILE 1)
else(${CMAKE_VERBOSE_MAKEFILE})
	set(AUTOTOOLS_VERBOSE_MAKEFILE 0)
endif(${CMAKE_VERBOSE_MAKEFILE})


find_package(PythonInterp)
if(NOT PYTHONINTERP_FOUND)
	message(FATAL_ERROR "Could not find python!")
endif(NOT PYTHONINTERP_FOUND)

if(MSVC)
	find_program(SH_PROGRAM
		NAMES sh.exe
		HINTS "C:/MinGW/msys/1.0/bin"
	)
	if(NOT SH_PROGRAM)
		message(FATAL_ERROR "Could not find MinGW!")
	endif(NOT SH_PROGRAM)
endif(MSVC)

find_program(PATCH_PROGRAM
	NAMES patch patch.exe
)
if(NOT PATCH_PROGRAM)
	if(WIN32)
		message(FATAL_ERROR "Could not find the patch.exe program. Please install it from http://gnuwin32.sourceforge.net/packages/patch.htm")
	else(WIN32)
		message(FATAL_ERROR "Could not find the patch program.")
	endif(WIN32)
endif(NOT PATCH_PROGRAM)

set(LINPHONE_BUILDER_EP_VARS)

macro(linphone_builder_expand_external_project_vars)
  set(LINPHONE_BUILDER_EP_ARGS "")
  set(LINPHONE_BUILDER_EP_VARNAMES "")
  foreach(arg ${LINPHONE_BUILDER_EP_VARS})
    string(REPLACE ":" ";" varname_and_vartype ${arg})
    set(target_info_list ${target_info_list})
    list(GET varname_and_vartype 0 _varname)
    list(GET varname_and_vartype 1 _vartype)
    list(APPEND LINPHONE_BUILDER_EP_ARGS -D${_varname}:${_vartype}=${${_varname}})
    list(APPEND LINPHONE_BUILDER_EP_VARNAMES ${_varname})
  endforeach()
endmacro(linphone_builder_expand_external_project_vars)

list(APPEND LINPHONE_BUILDER_EP_VARS
	CMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH
	CMAKE_BUILD_TYPE:STRING
	CMAKE_BUNDLE_OUTPUT_DIRECTORY:PATH
	CMAKE_C_COMPILER:PATH
	CMAKE_C_COMPILER_FORCED:BOOL
	CMAKE_C_COMPILER_ID_RUN:BOOL
	CMAKE_C_COMPILER_ID:STRING
	CMAKE_C_COMPILER_VERSION:STRING
	CMAKE_C_FLAGS_DEBUG:STRING
	CMAKE_C_FLAGS_MINSIZEREL:STRING
	CMAKE_C_FLAGS_RELEASE:STRING
	CMAKE_C_FLAGS_RELWITHDEBINFO:STRING
	CMAKE_C_FLAGS:STRING
	CMAKE_COMPILER_IS_GNUCC:BOOL
	CMAKE_COMPILER_IS_GNUCXX:BOOL
	CMAKE_CROSSCOMPILING:BOOL
	CMAKE_CXX_COMPILER:PATH
	CMAKE_CXX_COMPILER_FORCED:BOOL
	CMAKE_CXX_COMPILER_ID_RUN:BOOL
	CMAKE_CXX_COMPILER_ID:STRING
	CMAKE_CXX_COMPILER_VERSION:STRING
	CMAKE_CXX_FLAGS_DEBUG:STRING
	CMAKE_CXX_FLAGS_MINSIZEREL:STRING
	CMAKE_CXX_FLAGS_RELEASE:STRING
	CMAKE_CXX_FLAGS_RELWITHDEBINFO:STRING
	CMAKE_CXX_FLAGS:STRING
	CMAKE_EXE_LINKER_FLAGS_DEBUG:STRING
	CMAKE_EXE_LINKER_FLAGS_MINSIZEREL:STRING
	CMAKE_EXE_LINKER_FLAGS_RELEASE:STRING
	CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO:STRING
	CMAKE_EXE_LINKER_FLAGS:STRING
	CMAKE_EXTRA_GENERATOR:STRING
	CMAKE_FIND_ROOT_PATH:PATH
	CMAKE_FIND_ROOT_PATH_MODE_INCLUDE:STRING
	CMAKE_FIND_ROOT_PATH_MODE_LIBRARY:STRING
	CMAKE_FIND_ROOT_PATH_MODE_PROGRAM:STRING
	CMAKE_INSTALL_PREFIX:PATH
	CMAKE_LIBRARY_OUTPUT_DIRECTORY:PATH
	CMAKE_MODULE_LINKER_FLAGS_DEBUG:STRING
	CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL:STRING
	CMAKE_MODULE_LINKER_FLAGS_RELEASE:STRING
	CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO:STRING
	CMAKE_MODULE_LINKER_FLAGS:STRING
	CMAKE_MODULE_PATH:PATH
	CMAKE_NO_BUILD_TYPE:BOOL
	CMAKE_PREFIX_PATH:STRING
	CMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH
	CMAKE_SHARED_LINKER_FLAGS_DEBUG:STRING
	CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL:STRING
	CMAKE_SHARED_LINKER_FLAGS_RELEASE:STRING
	CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO:STRING
	CMAKE_SHARED_LINKER_FLAGS:STRING
	CMAKE_SKIP_RPATH:BOOL
	CMAKE_SYSTEM_NAME:STRING
	CMAKE_SYSTEM_PROCESSOR:STRING
	CMAKE_VERBOSE_MAKEFILE:BOOL
	LINPHONE_BUILDER_TOOLCHAIN:STRING
	MSVC_C_ARCHITECTURE_ID:STRING
	MSVC_CXX_ARCHITECTURE_ID:STRING
	MSVC_VERSION:STRING
)
if(APPLE)
	list(APPEND LINPHONE_BUILDER_EP_VARS
		CMAKE_OSX_ARCHITECTURES:STRING
		CMAKE_OSX_DEPLOYMENT_TARGET:STRING
	)
endif(APPLE)


macro(linphone_builder_apply_toolchain_flags)
	foreach(BUILD_CONFIG "" "_DEBUG" "_MINSIZEREL" "_RELEASE" "_RELWITHDEBINFO")
		if(NOT "${LINPHONE_BUILDER_TOOLCHAIN_CPPFLAGS}" STREQUAL "")
			set(CMAKE_C_FLAGS${BUILD_CONFIG} "${CMAKE_C_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_TOOLCHAIN_CPPFLAGS}")
			set(CMAKE_CXX_FLAGS${BUILD_CONFIG} "${CMAKE_CXX_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_TOOLCHAIN_CPPFLAGS}")
		endif(NOT "${LINPHONE_BUILDER_TOOLCHAIN_CPPFLAGS}" STREQUAL "")
		if(NOT "${LINPHONE_BUILDER_TOOLCHAIN_CFLAGS}" STREQUAL "")
			set(CMAKE_C_FLAGS${BUILD_CONFIG} "${CMAKE_C_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_TOOLCHAIN_CFLAGS}")
		endif(NOT "${LINPHONE_BUILDER_TOOLCHAIN_CFLAGS}" STREQUAL "")
		if(NOT "${LINPHONE_BUILDER_TOOLCHAIN_CXXFLAGS}" STREQUAL "")
			set(CMAKE_CXX_FLAGS${BUILD_CONFIG} "${CMAKE_CXX_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_TOOLCHAIN_CXXFLAGS}")
		endif(NOT "${LINPHONE_BUILDER_TOOLCHAIN_CXXFLAGS}" STREQUAL "")
		if(NOT "${LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS}" STREQUAL "")
			# TODO: The two following lines should not be here
			set(CMAKE_C_FLAGS${BUILD_CONFIG} "${CMAKE_C_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS}")
			set(CMAKE_CXX_FLAGS${BUILD_CONFIG} "${CMAKE_CXX_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS}")

			set(CMAKE_EXE_LINKER_FLAGS${BUILD_CONFIG} "${CMAKE_EXE_LINKER_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS}")
			set(CMAKE_MODULE_LINKER_FLAGS${BUILD_CONFIG} "${CMAKE_MODULE_LINKER_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS}")
			set(CMAKE_SHARED_LINKER_FLAGS${BUILD_CONFIG} "${CMAKE_SHARED_LINKER_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS}")
		endif(NOT "${LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS}" STREQUAL "")
	endforeach(BUILD_CONFIG)
endmacro(linphone_builder_apply_toolchain_flags)


macro(linphone_builder_apply_extra_flags PROJNAME)
	if("${EP_${PROJNAME}_USE_AUTOTOOLS}" STREQUAL "yes")
		set(ep_extra_cppflags ${EP_${PROJNAME}_EXTRA_CPPFLAGS})
		set(ep_extra_cflags ${EP_${PROJNAME}_EXTRA_CFLAGS})
		set(ep_extra_cxxflags ${EP_${PROJNAME}_EXTRA_CXXFLAGS})
		set(ep_extra_ldflags ${EP_${PROJNAME}_EXTRA_LDFLAGS})
	else("${EP_${PROJNAME}_USE_AUTOTOOLS}" STREQUAL "yes")
		foreach(BUILD_CONFIG "" "_DEBUG" "_MINSIZEREL" "_RELEASE" "_RELWITHDEBINFO")
			if(NOT "${EP_${PROJNAME}_EXTRA_CFLAGS}" STREQUAL "")
				set(CMAKE_C_FLAGS${BUILD_CONFIG} "${CMAKE_C_FLAGS${BUILD_CONFIG}} ${EP_${PROJNAME}_EXTRA_CFLAGS}")
			endif(NOT "${EP_${PROJNAME}_EXTRA_CFLAGS}" STREQUAL "")
			if(NOT "${EP_${PROJNAME}_EXTRA_CXXFLAGS}" STREQUAL "")
				set(CMAKE_CXX_FLAGS${BUILD_CONFIG} "${CMAKE_CXX_FLAGS${BUILD_CONFIG}} ${EP_${PROJNAME}_EXTRA_CXXFLAGS}")
			endif(NOT "${EP_${PROJNAME}_EXTRA_CXXFLAGS}" STREQUAL "")
			if(NOT "${EP_${PROJNAME}_EXTRA_LDFLAGS}" STREQUAL "")
				set(CMAKE_EXE_LINKER_FLAGS${BUILD_CONFIG} "${CMAKE_EXE_LINKER_FLAGS${BUILD_CONFIG}} ${EP_${PROJNAME}_EXTRA_LDFLAGS}")
				set(CMAKE_MODULE_LINKER_FLAGS${BUILD_CONFIG} "${CMAKE_MODULE_LINKER_FLAGS${BUILD_CONFIG}} ${EP_${PROJNAME}_EXTRA_LDFLAGS}")
				set(CMAKE_SHARED_LINKER_FLAGS${BUILD_CONFIG} "${CMAKE_SHARED_LINKER_FLAGS${BUILD_CONFIG}} ${EP_${PROJNAME}_EXTRA_LDFLAGS}")
			endif(NOT "${EP_${PROJNAME}_EXTRA_LDFLAGS}" STREQUAL "")
		endforeach(BUILD_CONFIG)
	endif("${EP_${PROJNAME}_USE_AUTOTOOLS}" STREQUAL "yes")
endmacro(linphone_builder_apply_extra_flags)


macro(linphone_builder_set_ep_directories PROJNAME)
	set(ep_source "${ep_base}/Source/EP_${PROJNAME}")
	if(NOT "${LINPHONE_BUILDER_TOOLCHAIN}" STREQUAL "")
		set(ep_tmp "${ep_base}/tmp-${LINPHONE_BUILDER_TOOLCHAIN}/${PROJNAME}")
		set(ep_build "${ep_base}/Build-${LINPHONE_BUILDER_TOOLCHAIN}/${PROJNAME}")
	else()
		set(ep_tmp "${ep_base}/tmp/${PROJNAME}")
		set(ep_build "${ep_base}/Build/${PROJNAME}")
	endif()
endmacro(linphone_builder_set_ep_directories)


macro(linphone_builder_create_autogen_command PROJNAME)
	if("${EP_${PROJNAME}_USE_AUTOGEN}" STREQUAL "yes")
		if("${EP_${PROJNAME}_CONFIGURE_OPTIONS_PASSED_TO_AUTOGEN}" STREQUAL "yes")
			set(ep_autogen_options "")
			foreach(OPTION ${EP_${PROJNAME}_CROSS_COMPILATION_OPTIONS} ${EP_${PROJNAME}_LINKING_TYPE} ${EP_${PROJNAME}_CONFIGURE_OPTIONS})
				set(ep_autogen_options "${ep_autogen_options} \"${OPTION}\"")
			endforeach(OPTION)
		endif("${EP_${PROJNAME}_CONFIGURE_OPTIONS_PASSED_TO_AUTOGEN}" STREQUAL "yes")
		set(ep_autogen_command "${ep_source}/autogen.sh ${ep_autogen_options}")
	else("${EP_${PROJNAME}_USE_AUTOGEN}" STREQUAL "yes")
		set(ep_autogen_command "")
	endif("${EP_${PROJNAME}_USE_AUTOGEN}" STREQUAL "yes")
endmacro(linphone_builder_create_autogen_command)


macro(linphone_builder_create_configure_command PROJNAME)
	if("${EP_${PROJNAME}_CONFIGURE_OPTIONS_PASSED_TO_AUTOGEN}" STREQUAL "yes")
		set(ep_configure_command "")
	else("${EP_${PROJNAME}_CONFIGURE_OPTIONS_PASSED_TO_AUTOGEN}" STREQUAL "yes")
		set(ep_configure_options "")
		foreach(OPTION ${EP_${PROJNAME}_CROSS_COMPILATION_OPTIONS} ${EP_${PROJNAME}_LINKING_TYPE} ${EP_${PROJNAME}_CONFIGURE_OPTIONS})
			set(ep_configure_options "${ep_configure_options} \"${OPTION}\"")
		endforeach(OPTION)
		set(ep_configure_command "${ep_source}/configure ${ep_configure_options}")
	endif("${EP_${PROJNAME}_CONFIGURE_OPTIONS_PASSED_TO_AUTOGEN}" STREQUAL "yes")
endmacro(linphone_builder_create_configure_command)


function(linphone_builder_add_project PROJNAME)
	linphone_builder_set_ep_directories(${PROJNAME})
	linphone_builder_apply_extra_flags(${PROJNAME})
	linphone_builder_expand_external_project_vars()

	if("${EP_${PROJNAME}_USE_AUTOTOOLS}" STREQUAL "yes")
		linphone_builder_create_autogen_command(${PROJNAME})
		linphone_builder_create_configure_command(${PROJNAME})
		if("${EP_${PROJNAME}_CONFIG_H_FILE}" STREQUAL "")
			set(ep_config_h_file config.h)
		else("${EP_${PROJNAME}_CONFIG_H_FILE}" STREQUAL "")
			set(ep_config_h_file ${EP_${PROJNAME}_CONFIG_H_FILE})
		endif("${EP_${PROJNAME}_CONFIG_H_FILE}" STREQUAL "")

		if(MSVC)
			set(SCRIPT_EXTENSION bat)
			set(MSVC_PROJNAME ${PROJNAME})
			configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/mingw_configure.bat.cmake ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_configure.bat)
			configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/mingw_build.bat.cmake ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_build.bat)
			configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/mingw_install.bat.cmake ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_install.bat)
			# Build in source with MinGW as build out-of-source does not work
			set(ep_build ${ep_source})
		else(MSVC)
			set(SCRIPT_EXTENSION sh)
		endif(MSVC)

		if("${EP_${PROJNAME}_PKG_CONFIG}" STREQUAL "")
			set(LINPHONE_BUILDER_PKG_CONFIG "pkg-config")
		else("${EP_${PROJNAME}_PKG_CONFIG}" STREQUAL "")
			set(LINPHONE_BUILDER_PKG_CONFIG "${EP_${PROJNAME}_PKG_CONFIG}")
		endif("${EP_${PROJNAME}_PKG_CONFIG}" STREQUAL "")

		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/configure.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_configure.sh)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/build.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_build.sh)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/install.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_install.sh)

		set(BUILD_COMMANDS
			CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_configure.${SCRIPT_EXTENSION}
			BUILD_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_build.${SCRIPT_EXTENSION}
			INSTALL_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_install.${SCRIPT_EXTENSION}
		)
	else("${EP_${PROJNAME}_USE_AUTOTOOLS}" STREQUAL "yes")
		set(BUILD_COMMANDS
			CMAKE_ARGS ${EP_${PROJNAME}_CMAKE_OPTIONS} ${EP_${PROJNAME}_LINKING_TYPE}
			CMAKE_CACHE_ARGS ${LINPHONE_BUILDER_EP_ARGS}
		)
	endif("${EP_${PROJNAME}_USE_AUTOTOOLS}" STREQUAL "yes")

	if(NOT "${EP_${PROJNAME}_URL}" STREQUAL "")
		set(DOWNLOAD_SOURCE URL ${EP_${PROJNAME}_URL})
	else(NOT "${EP_${PROJNAME}_URL}" STREQUAL "")
		set(DOWNLOAD_SOURCE GIT_REPOSITORY ${EP_${PROJNAME}_GIT_REPOSITORY} GIT_TAG ${EP_${PROJNAME}_GIT_TAG})
	endif(NOT "${EP_${PROJNAME}_URL}" STREQUAL "")

	ExternalProject_Add(EP_${PROJNAME}
		DEPENDS ${EP_${PROJNAME}_DEPENDENCIES}
		TMP_DIR ${ep_tmp}
		BINARY_DIR ${ep_build}
		${DOWNLOAD_SOURCE}
		PATCH_COMMAND ${EP_${PROJNAME}_PATCH_COMMAND}
		CMAKE_GENERATOR ${CMAKE_GENERATOR}
		${BUILD_COMMANDS}
	)

	if(MSVC)
		if("${EP_${PROJNAME}_USE_AUTOTOOLS}" STREQUAL "yes")
			ExternalProject_Add_Step(EP_${PROJNAME} postinstall
				COMMAND ${CMAKE_COMMAND} -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DINSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -P ${CMAKE_CURRENT_SOURCE_DIR}/builders/${PROJNAME}/postinstall.cmake
				COMMENT "Performing post-installation step"
				DEPENDEES mkdir update patch download configure build install
				WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			)
		endif("${EP_${PROJNAME}_USE_AUTOTOOLS}" STREQUAL "yes")
	endif(MSVC)
endfunction(linphone_builder_add_project)

function(linphone_builder_add_external_projects)
	foreach(BUILDER ${LINPHONE_BUILDER_BUILDERS})
		linphone_builder_add_project(${BUILDER})
	endforeach(BUILDER)
endfunction(linphone_builder_add_external_projects)
