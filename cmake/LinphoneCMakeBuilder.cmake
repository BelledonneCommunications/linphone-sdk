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

set(ep_base ${LINPHONE_BUILDER_WORK_DIR})
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
set(LINPHONE_BUILDER_INCLUDED_BUILDERS)

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
	CMAKE_C_FLAGS_DEBUG:STRING
	CMAKE_C_FLAGS_MINSIZEREL:STRING
	CMAKE_C_FLAGS_RELEASE:STRING
	CMAKE_C_FLAGS_RELWITHDEBINFO:STRING
	CMAKE_C_FLAGS:STRING
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
	CMAKE_VERBOSE_MAKEFILE:BOOL
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
if(CMAKE_TOOLCHAIN_FILE)
	list(APPEND LINPHONE_BUILDER_EP_VARS CMAKE_TOOLCHAIN_FILE:PATH)
endif()


function(linphone_builder_get_autotools_configuration)
	if(MSVC)
		set(_generator "MinGW Makefiles")
	else()
		set(_generator "${CMAKE_GENERATOR}")
		if(CMAKE_EXTRA_GENERATOR)
			set(_extra_generator "${CMAKE_EXTRA_GENERATOR}")
		endif()
	endif()
	set(_autotools_command ${CMAKE_COMMAND} -G "${_generator}")
	if(_extra_generator)
		list(APPEND _autotools_command -T "${_extra_generator}")
	endif()
	if(CMAKE_TOOLCHAIN_FILE)
		list(APPEND _autotools_command "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
	endif()
	list(APPEND _autotools_command "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Autotools/")
	execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/Autotools)
	execute_process(COMMAND ${_autotools_command} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Autotools)
	unset(_autotools_command)
	unset(_extra_generator)
	unset(_generator)
	include(${CMAKE_CURRENT_BINARY_DIR}/Autotools/Autotools.cmake)
endfunction(linphone_builder_get_autotools_configuration)


macro(linphone_builder_create_targets_list)
	set(LINPHONE_BUILDER_TARGETS )
	if("${LINPHONE_BUILDER_TARGET}" STREQUAL "belle-sip")
		list(APPEND LINPHONE_BUILDER_TARGETS "belle-sip")
	elseif("${LINPHONE_BUILDER_TARGET}" STREQUAL "ortp")
		list(APPEND LINPHONE_BUILDER_TARGETS "ortp")
	elseif("${LINPHONE_BUILDER_TARGET}" STREQUAL "ms2")
		list(APPEND LINPHONE_BUILDER_TARGETS "ortp" "ms2")
	elseif("${LINPHONE_BUILDER_TARGET}" STREQUAL "ms2-plugins")
		list(APPEND LINPHONE_BUILDER_TARGETS "ortp" "ms2" "ms2-plugins")
	elseif("${LINPHONE_BUILDER_TARGET}" STREQUAL "linphone")
		list(APPEND LINPHONE_BUILDER_TARGETS "belle-sip" "ortp" "ms2" "ms2-plugins" "linphone")
	else()
		message(FATAL_ERROR "Invalid LINPHONE_BUILDER_TARGET '${LINPHONE_BUILDER_TARGET}'")
	endif()
endmacro(linphone_builder_create_targets_list)


macro(linphone_builder_include_builder BUILDER)
	list(FIND LINPHONE_BUILDER_INCLUDED_BUILDERS ${BUILDER} _already_included)
	if(_already_included EQUAL -1)
		message(STATUS "Including builder ${BUILDER}")
		include(${CMAKE_CURRENT_SOURCE_DIR}/builders/${BUILDER}.cmake)
		list(APPEND LINPHONE_BUILDER_INCLUDED_BUILDERS ${BUILDER})
	endif()
	unset(_already_included)
endmacro(linphone_builder_include_builder)


macro(linphone_builder_add_builder_to_target TARGETNAME BUILDER)
	linphone_builder_include_builder(${BUILDER})
	add_dependencies(${TARGETNAME} ${BUILDER})
endmacro(linphone_builder_add_builder_to_target)


macro(linphone_builder_apply_flags)
	foreach(BUILD_CONFIG "" "_DEBUG" "_MINSIZEREL" "_RELEASE" "_RELWITHDEBINFO")
		if(NOT "${LINPHONE_BUILDER_CPPFLAGS}" STREQUAL "")
			set(CMAKE_C_FLAGS${BUILD_CONFIG} "${CMAKE_C_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_CPPFLAGS}")
			set(CMAKE_CXX_FLAGS${BUILD_CONFIG} "${CMAKE_CXX_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_CPPFLAGS}")
			set(AUTOTOOLS_C_FLAGS${BUILD_CONFIG} "${AUTOTOOLS_C_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_CPPFLAGS}")
			set(AUTOTOOLS_CXX_FLAGS${BUILD_CONFIG} "${AUTOTOOLS_CXX_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_CPPFLAGS}")
		endif(NOT "${LINPHONE_BUILDER_CPPFLAGS}" STREQUAL "")
		if(NOT "${LINPHONE_BUILDER_CFLAGS}" STREQUAL "")
			set(CMAKE_C_FLAGS${BUILD_CONFIG} "${CMAKE_C_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_CFLAGS}")
			set(AUTOTOOLS_C_FLAGS${BUILD_CONFIG} "${AUTOTOOLS_C_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_CFLAGS}")
		endif(NOT "${LINPHONE_BUILDER_CFLAGS}" STREQUAL "")
		if(NOT "${LINPHONE_BUILDER_CXXFLAGS}" STREQUAL "")
			set(CMAKE_CXX_FLAGS${BUILD_CONFIG} "${CMAKE_CXX_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_CXXFLAGS}")
			set(AUTOTOOLS_CXX_FLAGS${BUILD_CONFIG} "{AUTOTOOLS_CXX_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_CXX_FLAGS}")
		endif(NOT "${LINPHONE_BUILDER_CXXFLAGS}" STREQUAL "")
		if(NOT "${LINPHONE_BUILDER_OBJCFLAGS}" STREQUAL "")
			set(CMAKE_C_FLAGS${BUILD_CONFIG} "${CMAKE_C_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_OBJCFLAGS}")
			set(AUTOTOOLS_OBJC_FLAGS${BUILD_CONFIG} "${AUTOTOOLS_OBJC_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_OBJCFLAGS}")
		endif(NOT "${LINPHONE_BUILDER_OBJCFLAGS}" STREQUAL "")
		if(NOT "${LINPHONE_BUILDER_LDFLAGS}" STREQUAL "")
			# TODO: The two following lines should not be here
			set(CMAKE_C_FLAGS${BUILD_CONFIG} "${CMAKE_C_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_LDFLAGS}")
			set(CMAKE_CXX_FLAGS${BUILD_CONFIG} "${CMAKE_CXX_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_LDFLAGS}")

			set(CMAKE_EXE_LINKER_FLAGS${BUILD_CONFIG} "${CMAKE_EXE_LINKER_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_LDFLAGS}")
			set(CMAKE_MODULE_LINKER_FLAGS${BUILD_CONFIG} "${CMAKE_MODULE_LINKER_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_LDFLAGS}")
			set(CMAKE_SHARED_LINKER_FLAGS${BUILD_CONFIG} "${CMAKE_SHARED_LINKER_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_LDFLAGS}")
			set(AUTOTOOLS_LINKER_FLAGS${BUILD_CONFIG} "${AUTOTOOLS_LINKER_FLAGS${BUILD_CONFIG}} ${LINPHONE_BUILDER_LDFLAGS}")
		endif(NOT "${LINPHONE_BUILDER_LDFLAGS}" STREQUAL "")
	endforeach(BUILD_CONFIG)
endmacro(linphone_builder_apply_flags)


macro(linphone_builder_apply_cmake_flags_to_autotools_project PROJNAME)
	if("${EP_${PROJNAME}_BUILD_METHOD}" STREQUAL "autotools")
		set(BUILD_TYPES "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
		list(FIND BUILD_TYPES "${CMAKE_BUILD_TYPE}" BUILD_TYPE_FOUND)
		set(BUILD_TYPE_SUFFIX "")
		if(NOT ${BUILD_TYPE_FOUND} EQUAL -1)
			string(TOUPPER "${CMAKE_BUILD_TYPE}" UPPER_BUILD_TYPE)
			set(BUILD_TYPE_SUFFIX "_${UPPER_BUILD_TYPE}")
		endif(NOT ${BUILD_TYPE_FOUND} EQUAL -1)
		set(ep_asflags "${AUTOTOOLS_AS_FLAGS${BUILD_TYPE_SUFFIX}}")
		set(ep_cppflags "${AUTOTOOLS_CPP_FLAGS${BUILD_TYPE_SUFFIX}}")
		set(ep_cflags "${AUTOTOOLS_C_FLAGS${BUILD_TYPE_SUFFIX}}")
		set(ep_cxxflags "${AUTOTOOLS_CXX_FLAGS${BUILD_TYPE_SUFFIX}}")
		set(ep_objcflags "${AUTOTOOLS_OBJC_FLAGS${BUILD_TYPE_SUFFIX}}")
		set(ep_ldflags "${AUTOTOOLS_LINKER_FLAGS${BUILD_TYPE_SUFFIX}}")
	endif()
endmacro(linphone_builder_apply_cmake_flags_to_autotools_project)


macro(linphone_builder_apply_extra_flags PROJNAME)
	if("${EP_${PROJNAME}_BUILD_METHOD}" STREQUAL "autotools")
		set(ep_asflags "${ep_asflags} ${EP_${PROJNAME}_EXTRA_ASFLAGS}")
		set(ep_cppflags "${ep_cppflags} ${EP_${PROJNAME}_EXTRA_CPPFLAGS}")
		set(ep_cflags "${ep_cflags} ${EP_${PROJNAME}_EXTRA_CFLAGS}")
		set(ep_cxxflags "${ep_cxxflags} ${EP_${PROJNAME}_EXTRA_CXXFLAGS}")
		set(ep_objcflags "${ep_objcflags} ${EP_${PROJNAME}_EXTRA_OBJCFLAGS}")
		set(ep_ldflags "${ep_ldflags} ${EP_${PROJNAME}_EXTRA_LDFLAGS}")
	else()
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
	endif()
endmacro(linphone_builder_apply_extra_flags)


macro(linphone_builder_set_ep_directories PROJNAME)
	get_filename_component(CONFIG_NAME ${LINPHONE_BUILDER_CONFIG_FILE} NAME_WE)
	string(REGEX REPLACE "config-" "" CONFIG_NAME ${CONFIG_NAME})
	set(ep_source "${ep_base}/Source/EP_${PROJNAME}")
	set(ep_tmp "${ep_base}/tmp-${CONFIG_NAME}/${PROJNAME}")
	if("${EP_${PROJNAME}_BUILD_IN_SOURCE}" STREQUAL "yes")
		set(ep_build "${ep_source}")
	else("${EP_${PROJNAME}_BUILD_IN_SOURCE}" STREQUAL "yes")
		set(ep_build "${ep_base}/Build-${CONFIG_NAME}/${PROJNAME}")
	endif("${EP_${PROJNAME}_BUILD_IN_SOURCE}" STREQUAL "yes")
	unset(CONFIG_NAME)
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
		set(ep_configure_env "${EP_${PROJNAME}_CONFIGURE_ENV}")
		set(ep_configure_command "${ep_source}/configure ${ep_configure_options}")
	endif("${EP_${PROJNAME}_CONFIGURE_OPTIONS_PASSED_TO_AUTOGEN}" STREQUAL "yes")
endmacro(linphone_builder_create_configure_command)


function(linphone_builder_add_project PROJNAME)
	linphone_builder_set_ep_directories(${PROJNAME})
	linphone_builder_apply_cmake_flags_to_autotools_project(${PROJNAME})
	linphone_builder_apply_extra_flags(${PROJNAME})
	linphone_builder_expand_external_project_vars()

	if("${EP_${PROJNAME}_BUILD_METHOD}" STREQUAL "custom")
		set(BUILD_COMMANDS
			CONFIGURE_COMMAND ${EP_${PROJNAME}_CONFIGURE_COMMAND}
			BUILD_COMMAND ${EP_${PROJNAME}_BUILD_COMMAND}
			INSTALL_COMMAND ${EP_${PROJNAME}_INSTALL_COMMAND}
		)
	elseif("${EP_${PROJNAME}_BUILD_METHOD}" STREQUAL "autotools")
		linphone_builder_create_autogen_command(${PROJNAME})
		linphone_builder_create_configure_command(${PROJNAME})
		if("${EP_${PROJNAME}_CONFIG_H_FILE}" STREQUAL "")
			set(ep_config_h_file config.h)
		else("${EP_${PROJNAME}_CONFIG_H_FILE}" STREQUAL "")
			set(ep_config_h_file ${EP_${PROJNAME}_CONFIG_H_FILE})
		endif("${EP_${PROJNAME}_CONFIG_H_FILE}" STREQUAL "")
		if("${EP_${PROJNAME}_INSTALL_TARGET}" STREQUAL "")
			set(ep_install_target "install")
		else("${EP_${PROJNAME}_INSTALL_TARGET}" STREQUAL "")
			set(ep_install_target "${EP_${PROJNAME}_INSTALL_TARGET}")
		endif("${EP_${PROJNAME}_INSTALL_TARGET}" STREQUAL "")

		if(WIN32)
			set(SCRIPT_EXTENSION bat)
			set(MSVC_PROJNAME ${PROJNAME})
			configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/mingw_configure.bat.cmake ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_configure.bat)
			configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/mingw_build.bat.cmake ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_build.bat)
			configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/mingw_install.bat.cmake ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}_install.bat)
			# Build in source with MinGW as build out-of-source does not work
			set(ep_build ${ep_source})
			set(ep_redirect_to_file "2>&1 >> ${CMAKE_CURRENT_BINARY_DIR}/EP_${PROJNAME}.log")
		else()
			set(SCRIPT_EXTENSION sh)
		endif()

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
	else()
		# Use CMake build method
		if("${CMAKE_VERSION}" VERSION_GREATER "2.8.3")
			set(BUILD_COMMANDS
				CMAKE_ARGS ${EP_${PROJNAME}_CMAKE_OPTIONS} ${EP_${PROJNAME}_LINKING_TYPE}
				CMAKE_CACHE_ARGS ${LINPHONE_BUILDER_EP_ARGS}
			)
		else("${CMAKE_VERSION}" VERSION_GREATER "2.8.3")
			set(BUILD_COMMANDS
				CMAKE_ARGS ${LINPHONE_BUILDER_EP_ARGS} ${EP_${PROJNAME}_CMAKE_OPTIONS} ${EP_${PROJNAME}_LINKING_TYPE}
			)
		endif("${CMAKE_VERSION}" VERSION_GREATER "2.8.3")
	endif()

	if(NOT "${EP_${PROJNAME}_URL}" STREQUAL "")
		set(DOWNLOAD_SOURCE URL ${EP_${PROJNAME}_URL})
		if(NOT "${EP_${PROJNAME}_URL_HASH}" STREQUAL "")
			if("${CMAKE_VERSION}" VERSION_GREATER "2.8.9")
				list(APPEND DOWNLOAD_SOURCE URL_HASH ${EP_${PROJNAME}_URL_HASH})
			else()
				string(REGEX MATCH "^MD5=([A-Fa-f0-9]+)$" _match ${EP_${PROJNAME}_URL_HASH})
				if(NOT "${_match}" STREQUAL "")
					string(REGEX REPLACE "MD5=" "" _match ${_match})
					list(APPEND DOWNLOAD_SOURCE URL_MD5 ${_match})
				endif()
				unset(_match)
			endif()
		endif()
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
		if("${EP_${PROJNAME}_BUILD_METHOD}" STREQUAL "autotools")
			ExternalProject_Add_Step(EP_${PROJNAME} postinstall
				COMMAND ${CMAKE_COMMAND} -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DINSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -P ${CMAKE_CURRENT_SOURCE_DIR}/builders/${PROJNAME}/postinstall.cmake
				COMMENT "Performing post-installation step"
				DEPENDEES mkdir update patch download configure build install
				WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			)
		endif()
	endif(MSVC)
endfunction(linphone_builder_add_project)

function(linphone_builder_add_external_projects)
	foreach(BUILDER ${LINPHONE_BUILDER_INCLUDED_BUILDERS})
		linphone_builder_add_project(${BUILDER})
	endforeach(BUILDER)
endfunction(linphone_builder_add_external_projects)
