############################################################################
# FindHidAPI.cmake
# Copyright (C) 2018-2026  Belledonne Communications, Grenoble France
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
#
# Find the HidAPI library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  HidAPI - If the HidAPI library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  HidAPI_FOUND - The HidAPI library has been found
#  HidAPI_TARGET - The name of the CMake target for the HidAPI library
#
# This module may set the following variable:
#
#  HidAPI_USE_BUILD_INTERFACE - If the HidAPI library is used from its build directory


include(FindPackageHandleStandardArgs)

macro(hidapi_set_include_directories)
    get_target_property(HidAPI_HEADERS ${HidAPI_TARGET} PUBLIC_HEADER)
    list(GET HidAPI_HEADERS 0 HidAPI_HEADER)
    get_filename_component(HidAPI_INCLUDES "${HidAPI_HEADER}" DIRECTORY)
    set_target_properties(${HidAPI_TARGET} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${HidAPI_INCLUDES}>")
endmacro()

set(_HidAPI_REQUIRED_VARS HidAPI_TARGET)
set(_HidAPI_CACHE_VARS ${_HidAPI_REQUIRED_VARS})

if(TARGET hidapi_hidraw)

    set(HidAPI_TARGET hidapi_hidraw)
    set(HidAPI_USE_BUILD_INTERFACE TRUE)
    hidapi_set_include_directories()

elseif(TARGET hidapi_darwin)

    set(HidAPI_TARGET hidapi_darwin)
    set(HidAPI_USE_BUILD_INTERFACE TRUE)
    hidapi_set_include_directories()

elseif(TARGET hidapi_winapi)

    set(HidAPI_TARGET hidapi_winapi)
    set(HidAPI_USE_BUILD_INTERFACE TRUE)
    hidapi_set_include_directories()

else()

    find_path(_HidAPI_INCLUDE_DIRS
            NAMES
            hidapi/hidapi.h
            PATH_SUFFIXES include
    )

    find_library(_HidAPI_LIBRARY
            NAMES hidapi-hidraw
            PATH_SUFFIXES Frameworks bin lib lib64
    )

    if(_HidAPI_INCLUDE_DIRS AND _HidAPI_LIBRARY)
        add_library(HidAPI UNKNOWN IMPORTED)
        if(WIN32)
            set_target_properties(HidAPI PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${_HidAPI_INCLUDE_DIRS}"
                    IMPORTED_IMPLIB "${_HidAPI_LIBRARY}"
            )
        else()
            set_target_properties(HidAPI PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${_HidAPI_INCLUDE_DIRS}"
                    IMPORTED_LOCATION "${_HidAPI_LIBRARY}"
            )
        endif()

        set(HidAPI_TARGET HidAPI)
    endif()

endif()

find_package_handle_standard_args(HidAPI
        REQUIRED_VARS ${_HidAPI_REQUIRED_VARS}
)
mark_as_advanced(${_HidAPI_CACHE_VARS})
