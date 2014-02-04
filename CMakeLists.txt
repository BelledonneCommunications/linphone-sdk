############################################################################
# CMakeLists.txt
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

cmake_minimum_required(VERSION 2.8)

if(WIN32)
	set(LIB_INSTALL_DIR "bin")
else(WIN32)
	set(LIB_INSTALL_DIR "lib")
endif(WIN32)


set(PKG_CONFIG_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")


set(GENERAL_EP_VARS)

macro(_expand_external_project_vars)
  set(GENERAL_EP_ARGS "")
  set(GENERAL_EP_VARNAMES "")
  foreach(arg ${GENERAL_EP_VARS})
    string(REPLACE ":" ";" varname_and_vartype ${arg})
    set(target_info_list ${target_info_list})
    list(GET varname_and_vartype 0 _varname)
    list(GET varname_and_vartype 1 _vartype)
    list(APPEND GENERAL_EP_ARGS -D${_varname}:${_vartype}=${${_varname}})
    list(APPEND GENERAL_EP_VARNAMES ${_varname})
  endforeach()
endmacro()

list(APPEND GENERAL_EP_VARS
	CMAKE_SKIP_RPATH:BOOL
	CMAKE_MODULE_PATH:PATH
	CMAKE_BUILD_TYPE:STRING
	CMAKE_CXX_COMPILER:PATH
	CMAKE_CXX_FLAGS:STRING
	CMAKE_CXX_FLAGS_DEBUG:STRING
	CMAKE_CXX_FLAGS_MINSIZEREL:STRING
	CMAKE_CXX_FLAGS_RELEASE:STRING
	CMAKE_CXX_FLAGS_RELWITHDEBINFO:STRING
	CMAKE_C_COMPILER:PATH
	CMAKE_C_FLAGS:STRING
	CMAKE_C_FLAGS_DEBUG:STRING
	CMAKE_C_FLAGS_MINSIZEREL:STRING
	CMAKE_C_FLAGS_RELEASE:STRING
	CMAKE_C_FLAGS_RELWITHDEBINFO:STRING
	CMAKE_EXE_LINKER_FLAGS:STRING
	CMAKE_EXE_LINKER_FLAGS_DEBUG:STRING
	CMAKE_EXE_LINKER_FLAGS_MINSIZEREL:STRING
	CMAKE_EXE_LINKER_FLAGS_RELEASE:STRING
	CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO:STRING
	CMAKE_MODULE_LINKER_FLAGS:STRING
	CMAKE_MODULE_LINKER_FLAGS_DEBUG:STRING
	CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL:STRING
	CMAKE_MODULE_LINKER_FLAGS_RELEASE:STRING
	CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO:STRING
	CMAKE_SHARED_LINKER_FLAGS:STRING
	CMAKE_SHARED_LINKER_FLAGS_DEBUG:STRING
	CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL:STRING
	CMAKE_SHARED_LINKER_FLAGS_RELEASE:STRING
	CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO:STRING
	CMAKE_TOOLCHAIN_FILE:STRING
	CMAKE_GENERATOR:STRING
	CMAKE_EXTRA_GENERATOR:STRING
	CMAKE_INSTALL_PREFIX:PATH
	CMAKE_LIBRARY_OUTPUT_DIRECTORY:PATH
	CMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH
	CMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH
	CMAKE_BUNDLE_OUTPUT_DIRECTORY:PATH
	CMAKE_SHARED_LINKER_FLAGS:STRING
	CMAKE_EXE_LINKER_FLAGS:STRING
	CMAKE_MODULE_LINKER_FLAGS:STRING
	LIB_INSTALL_DIR:STRING
	PKG_CONFIG_LIBDIR:STRING
	AUTOTOOLS_CONFIG_SITE:STRING
)
if(APPLE)
	list(APPEND GENERAL_EP_VARS
		CMAKE_OSX_DEPLOYMENT_TARGET:STRING
		CMAKE_OSX_ARCHITECTURES:STRING
	)
endif(APPLE)

_expand_external_project_vars()

include(builders/CMakeLists.txt)
