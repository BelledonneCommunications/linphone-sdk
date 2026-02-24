############################################################################
# TasksPython.cmake
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

if (PYTHON_VERSION MATCHES "^Python 3[.]([0-9]+)[.]([0-9]+)$")
    set(Python3_EXECUTABLE "${PYTHON_EXECUTABLE}")
else()
    find_package(Python3 REQUIRED)
endif()

include("cmake/LinphoneSdkUtils.cmake")
linphone_sdk_check_python_module_is_installed(wheel)

# See https://packaging.python.org/guides/distributing-packages-using-setuptools/#id61
function(bc_compute_python_wheel_version OUTPUT_VERSION)
find_program(GIT_EXECUTABLE git NAMES Git CMAKE_FIND_ROOT_PATH_BOTH)
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" "describe"
        OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        WORKING_DIRECTORY "${LINPHONESDK_DIR}"
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
        set(version_prerelease "post")
    endif()

    # format full version
    set(full_version "${version_major}.${version_minor}.${version_patch}")
    if (version_prerelease)
        string(APPEND full_version ".${version_prerelease}")
        if (version_commit)
            string(APPEND full_version "${version_commit}+git.${version_hash}")
        endif()
    endif()

    set(${OUTPUT_VERSION} "${full_version}" CACHE STRING "" FORCE)
endif()
endfunction()
bc_compute_python_wheel_version(WHEEL_LINPHONESDK_VERSION)

set(PYTHON_INSTALL_DIR "${CMAKE_BINARY_DIR}/pylinphone/")
set(PYTHON_INSTALL_MODULE_DIR "${PYTHON_INSTALL_DIR}linphone/")
set(PYTHON_INSTALL_GRAMMARS_DIR "${PYTHON_INSTALL_MODULE_DIR}share/belr/grammars/")

configure_file("cmake/python/setup.cmake" "${CMAKE_BINARY_DIR}/setup.py" @ONLY)

if (APPLE)
    add_custom_target(create_python_module_architecture
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${PYTHON_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PYTHON_INSTALL_GRAMMARS_DIR}"

        # Do not use copy_directory because of symlinks, and copy from full libdir to have proper RPATH set by install target
        COMMAND "cp" "-R" "${CMAKE_INSTALL_FULL_LIBDIR}/*.so*" "${PYTHON_INSTALL_MODULE_DIR}"
        COMMAND "cp" "-R" "${CMAKE_INSTALL_FULL_LIBDIR}/*.dylib*" "${PYTHON_INSTALL_MODULE_DIR}"
        COMMAND "cp" "-R" "${CMAKE_INSTALL_PREFIX}/Frameworks" "${PYTHON_INSTALL_MODULE_DIR}"
        
        # Copy grammar files from each projects, like install target would do
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/belcard/src/vcard_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/belle-sip/src/sdp/sdp_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/belle-sip/src/sip/sip_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/liblinphone/share/identity_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/liblinphone/share/ics_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/liblinphone/share/cpim_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"

        COMMAND "cp" "${CMAKE_SOURCE_DIR}/cmake/python/__init__.py" "${PYTHON_INSTALL_MODULE_DIR}" 

        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        COMMENT "Generating linphone SDK with Python wrapper"
    )
else()
    add_custom_target(create_python_module_architecture
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${PYTHON_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PYTHON_INSTALL_GRAMMARS_DIR}"

        # Do not use copy_directory because of symlinks, and copy from full libdir to have proper RPATH set by install target
        COMMAND "cp" "-R" "${CMAKE_INSTALL_FULL_LIBDIR}/*.so*" "${PYTHON_INSTALL_MODULE_DIR}"
        
        # Copy grammar files from each projects, like install target would do
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/belcard/src/vcard_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/belle-sip/src/sdp/sdp_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/belle-sip/src/sip/sip_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/liblinphone/share/identity_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/liblinphone/share/ics_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"
        COMMAND "cp" "${CMAKE_SOURCE_DIR}/liblinphone/share/cpim_grammar.belr" "${PYTHON_INSTALL_GRAMMARS_DIR}"

        COMMAND "cp" "${CMAKE_SOURCE_DIR}/cmake/python/__init__.py" "${PYTHON_INSTALL_MODULE_DIR}" 

        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        COMMENT "Generating linphone SDK with Python wrapper"
    )
endif()

add_custom_target(wheel
    COMMAND ${Python3_EXECUTABLE} ../setup.py bdist_wheel --dist-dir ${CMAKE_INSTALL_PREFIX}/
    DEPENDS create_python_module_architecture ${CMAKE_BINARY_DIR}/setup.py 
    WORKING_DIRECTORY ${PYTHON_INSTALL_DIR}/
    COMMENT "Packaging linphone SDK with Python wrapper as a wheel"
)