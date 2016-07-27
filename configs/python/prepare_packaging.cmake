############################################################################
# prepare_packaging.cmake
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################


if(MSVC)
	string(REGEX REPLACE "Visual Studio ([0-9]+).*" "\\1" MSVC_VERSION "${CMAKE_GENERATOR}")
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(MSVC_DEBUG_SYSTEM_LIBRARIES "d")
	endif()
	# Starting with Visual Studio 2015 (MSVC_VERSION==14) the msvcr dll has been renamed to vcruntime
	find_file(VCRUNTIME_LIB vcruntime${MSVC_VERSION}0${MSVC_DEBUG_SYSTEM_LIBRARIES}.dll PATHS "C:/Windows/System32")
	if(NOT VCRUNTIME_LIB)
		find_file(VCRUNTIME_LIB msvcr${MSVC_VERSION}0${MSVC_DEBUG_SYSTEM_LIBRARIES}.dll PATHS "C:/Windows/System32")
	endif()
endif()

set(OUTPUT_DIR "${OUTPUT_DIR}/build_${PACKAGE_TYPE}/linphone")
file(MAKE_DIRECTORY "${OUTPUT_DIR}/linphone")
configure_file("${INPUT_DIR}/__init__.py.cmake" "${OUTPUT_DIR}/linphone/__init__.py")
file(COPY "${LINPHONE_PYTHON_MODULE}" DESTINATION "${OUTPUT_DIR}/linphone")
if(UNIX AND NOT APPLE)
	foreach(reallib ${LINPHONE_DYNAMIC_LIBRARIES_TO_INSTALL})
		get_filename_component(libpath ${reallib} DIRECTORY)
		get_filename_component(reallibname ${reallib} NAME)
		string(REGEX REPLACE "([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" libname ${reallibname})
		file(COPY ${reallib} DESTINATION "${OUTPUT_DIR}/linphone")
		file(RENAME "${OUTPUT_DIR}/linphone/${reallibname}" "${OUTPUT_DIR}/linphone/${libname}")
	endforeach()
else()
	file(COPY ${LINPHONE_DYNAMIC_LIBRARIES_TO_INSTALL} DESTINATION "${OUTPUT_DIR}/linphone")
endif()
if(MSVC AND NOT MSVC_VERSION STREQUAL "9")
	file(COPY ${VCRUNTIME_LIB} DESTINATION "${OUTPUT_DIR}/linphone")
endif()
get_filename_component(LINPHONE_PYTHON_MODULE_PATH "${LINPHONE_PYTHON_MODULE}" DIRECTORY)
find_file(PDB_FILENAME "linphone.pdb" "${LINPHONE_PYTHON_MODULE_PATH}")
if(PDB_FILENAME)
	file(COPY "${PDB_FILENAME}" DESTINATION "${OUTPUT_DIR}/linphone")
endif()
file(COPY "${LINPHONE_RESOURCES_PREFIX}/share/images" DESTINATION "${OUTPUT_DIR}/linphone/share/")
file(COPY "${LINPHONE_RESOURCES_PREFIX}/share/linphone" DESTINATION "${OUTPUT_DIR}/linphone/share/")
file(COPY "${LINPHONE_RESOURCES_PREFIX}/share/sounds" DESTINATION "${OUTPUT_DIR}/linphone/share/")
file(COPY "${LINPHONE_SOURCE_DIR}/tools/python/unittests" DESTINATION "${OUTPUT_DIR}/linphone/")
file(COPY "${LINPHONE_SOURCE_DIR}/tester/certificates" DESTINATION "${OUTPUT_DIR}/linphone/unittests/")
file(COPY "${LINPHONE_SOURCE_DIR}/tester/images" DESTINATION "${OUTPUT_DIR}/linphone/unittests/")
file(COPY "${LINPHONE_SOURCE_DIR}/tester/rcfiles" DESTINATION "${OUTPUT_DIR}/linphone/unittests/")
file(COPY "${LINPHONE_SOURCE_DIR}/tester/sounds" DESTINATION "${OUTPUT_DIR}/linphone/unittests/")
file(COPY "${LINPHONE_SOURCE_DIR}/tester/tester_hosts" DESTINATION "${OUTPUT_DIR}/linphone/unittests/")
file(GLOB_RECURSE LINPHONE_DATA_FILES RELATIVE "${OUTPUT_DIR}/linphone" "${OUTPUT_DIR}/linphone/*")
if(PACKAGE_TYPE STREQUAL "msi")
	set(BUILD_VERSION ${LINPHONE_VERSION})
else()
	set(BUILD_VERSION ${LINPHONE_GIT_REVISION})
endif()
configure_file("${INPUT_DIR}/setup.py.cmake" "${OUTPUT_DIR}/setup.py" @ONLY)
