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

cmake_minimum_required(VERSION 2.8.12)
project(PYLINPHONE C) # Dummy project

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(PLATFORM_ARCH_NAME "x86_64")
else()
	set(PLATFORM_ARCH_NAME "i386")
endif()

find_package(PythonInterp REQUIRED)
find_package(Git)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_PREFIX_PATH}/share/cmake/Modules)
find_package(Linphone REQUIRED)

set(LINPHONE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../EP_linphone")

if(MSVC)
	string(REGEX REPLACE "Visual Studio ([0-9]+).*" "\\1" MSVC_VERSION "${CMAKE_GENERATOR}")
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	find_file(MSVCR_LIB msvcr${MSVC_VERSION}0d.dll PATHS "C:/Windows/System32")
else()
	find_file(MSVCR_LIB msvcr${MSVC_VERSION}0.dll PATHS "C:/Windows/System32")
endif()

if(GIT_EXECUTABLE)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --always
		OUTPUT_VARIABLE LINPHONE_GIT_REVISION
		OUTPUT_STRIP_TRAILING_WHITESPACE
		WORKING_DIRECTORY ${LINPHONE_SOURCE_DIR}
	)
else()
	set(LINPHONE_GIT_REVISION "0.0.0")
endif()
STRING(REGEX REPLACE "([0-9.]+)-?.*" "\\1" LINPHONE_VERSION "${LINPHONE_GIT_REVISION}")

execute_process(
	COMMAND ${CMAKE_COMMAND} -E echo "#define LINPHONE_GIT_REVISION \"${LINPHONE_GIT_REVISION}\""
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/gitversion.h
)

# Find the doxygen XML directory
file(GLOB XML_DIR "${CMAKE_INSTALL_PREFIX}/share/doc/linphone-*/xml")

# Generate the API in XML format from the doxygen XML files
add_custom_target(api_xml
	${PYTHON_EXECUTABLE} "${LINPHONE_SOURCE_DIR}/tools/genapixml.py" "--pretty" "--outputfile" "${CMAKE_CURRENT_BINARY_DIR}/api.xml" "${XML_DIR}"
	DEPENDS ${XML_FILES} "${LINPHONE_SOURCE_DIR}/tools/genapixml.py"
)

# Generate the Python wrapper source code for the API in XML format
set(SOURCE_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/linphone.c")
add_custom_target(pylinphone_source
	${PYTHON_EXECUTABLE} "${LINPHONE_SOURCE_DIR}/tools/python/apixml2python.py" "--outputfile" "${SOURCE_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/api.xml"
	DEPENDS api_xml "${LINPHONE_SOURCE_DIR}/tools/python/apixml2python.py" "${LINPHONE_SOURCE_DIR}/tools/python/apixml2python/linphone.py"
	WORKING_DIRECTORY "${LINPHONE_SOURCE_DIR}/tools/python"
)

# Generate setup.py file that will be used to generate the package
set(_BUILDS )
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	list(APPEND _BUILDS "debug")
endif()
if(WIN32)
	list(APPEND LINPHONE_CPPFLAGS "-DWIN32")
	list(APPEND LINPHONE_LIBRARIES gcc mingwex)
	file(GLOB LINPHONE_DYNAMIC_LIBRARIES "${CMAKE_INSTALL_PREFIX}/bin/*.dll")
	list(APPEND _BUILDS "exe" "msi" "zip" "wheel")
endif()
if(UNIX)
	set(LINPHONE_STATIC_LIBRARIES )
	set(LINPHONE_DYNAMIC_LIBRARIES )
	foreach(lib ${LINPHONE_LIBRARIES})
		if(${lib} MATCHES "\\.a$")
			list(APPEND LINPHONE_STATIC_LIBRARIES ${lib})
		else()
			list(APPEND LINPHONE_DYNAMIC_LIBRARIES ${lib})
		endif()
	endforeach()
	find_program(LORDER_EXECUTABLE lorder)
	if(NOT LORDER_EXECUTABLE)
		message(FATAL_ERROR "lorder executable is needed.")
	endif()
	find_program(TSORT_EXECUTABLE tsort)
	if(NOT TSORT_EXECUTABLE)
		message(FATAL_ERROR "tsort executable is needed.")
	endif()
	execute_process(COMMAND ${LORDER_EXECUTABLE} ${LINPHONE_STATIC_LIBRARIES} OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/lorder.out")
	execute_process(COMMAND ${TSORT_EXECUTABLE} OUTPUT_VARIABLE LINPHONE_STATIC_LIBRARIES INPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/lorder.out")
	separate_arguments(LINPHONE_STATIC_LIBRARIES UNIX_COMMAND "${LINPHONE_STATIC_LIBRARIES}")
	if(APPLE)
		set(LINPHONE_DYNAMIC_LIBRARIES_TO_INSTALL
			"${CMAKE_INSTALL_PREFIX}/lib/libavcodec.53.61.100.dylib"
			"${CMAKE_INSTALL_PREFIX}/lib/libavutil.51.35.100.dylib"
			"${CMAKE_INSTALL_PREFIX}/lib/libswscale.2.1.100.dylib"
		)
	else()
		set(LINPHONE_DYNAMIC_LIBRARIES_TO_INSTALL
			"${CMAKE_INSTALL_PREFIX}/lib/libavcodec.so.53.61.100"
			"${CMAKE_INSTALL_PREFIX}/lib/libavutil.so.51.35.100"
			"${CMAKE_INSTALL_PREFIX}/lib/libswscale.so.2.1.100"
		)
	endif()
	list(APPEND _BUILDS "zip" "wheel")
endif()
foreach(_BUILD ${_BUILDS})
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone")
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/__init__.py.cmake" "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone/__init__.py")
	if(UNIX)
		foreach(reallib ${LINPHONE_DYNAMIC_LIBRARIES_TO_INSTALL})
			get_filename_component(libpath ${reallib} DIRECTORY)
			get_filename_component(reallibname ${reallib} NAME)
			string(REGEX REPLACE "([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" libname ${reallibname})
			file(COPY ${reallib} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone")
			file(RENAME "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone/${reallibname}" "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone/${libname}")
		endforeach()
	else()
		file(COPY ${LINPHONE_DYNAMIC_LIBRARIES} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone")
	endif()
	if(MSVC AND NOT "${MSVC_VERSION}" STREQUAL "9")
		file(COPY ${MSVCR_LIB} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone")
	endif()
	file(COPY "${CMAKE_INSTALL_PREFIX}/share/images" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone/share/")
	file(COPY "${CMAKE_INSTALL_PREFIX}/share/linphone" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone/share/")
	file(COPY "${CMAKE_INSTALL_PREFIX}/share/sounds" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone/share/")
	file(GLOB_RECURSE LINPHONE_DATA_FILES RELATIVE "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone" "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/linphone/*")
	if(MSVC AND ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug") AND (NOT "${_BUILD}" STREQUAL "debug"))
		set(PDB_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_debug/build/lib.win32-${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}/linphone/linphone.pdb")
		list(APPEND LINPHONE_DATA_FILES ${PDB_FILENAME})
	endif()
	if("${_BUILD}" STREQUAL "msi")
		set(BUILD_VERSION ${LINPHONE_VERSION})
	else()
		set(BUILD_VERSION ${LINPHONE_GIT_REVISION})
	endif()
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/setup.py.cmake" "${CMAKE_CURRENT_BINARY_DIR}/build_${_BUILD}/linphone/setup.py")
endforeach()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	add_custom_target(pylinphone_debug ALL
		${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/build_debug/linphone/setup.py" "build" "--debug"
		DEPENDS pylinphone_source
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_debug/linphone"
	)
else()
	add_custom_target(pylinphone_debug ALL DEPENDS pylinphone_source)
endif()

if(WIN32)

# Generate the installer
set(EXE_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_exe/linphone/dist/linphone-${LINPHONE_GIT_REVISION}.win32-py${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}.exe")
add_custom_target(pylinphone_exe ALL
	${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/build_exe/linphone/setup.py" "bdist_wininst"
	DEPENDS pylinphone_debug
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_exe/linphone"
)
install(FILES ${EXE_FILENAME} DESTINATION ".")

# Generate the msi
set(MSI_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_msi/linphone/dist/linphone-${LINPHONE_VERSION}.win32-py${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}.msi")
add_custom_target(pylinphone_msi ALL
	${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/build_msi/linphone/setup.py" "bdist_msi"
	DEPENDS pylinphone_exe
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_msi/linphone"
)
install(FILES ${MSI_FILENAME} DESTINATION "." RENAME "linphone-${LINPHONE_GIT_REVISION}.win32-py${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}.msi")

# Generate the zip
set(ZIP_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_zip/linphone/dist/linphone-${LINPHONE_GIT_REVISION}.win32.zip")
add_custom_target(pylinphone_zip ALL
	${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/build_zip/linphone/setup.py" "bdist" "--format=zip"
	DEPENDS pylinphone_msi
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_zip/linphone"
)
install(FILES ${ZIP_FILENAME} DESTINATION "." RENAME "linphone-${LINPHONE_GIT_REVISION}.win32-py${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}.zip")

# Generate the wheel package
string(REPLACE "-" "_" LINPHONE_UNDERSCORE_GIT_REVISION ${LINPHONE_GIT_REVISION})
set(WHEEL_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_wheel/linphone/dist/linphone-${LINPHONE_UNDERSCORE_GIT_REVISION}-cp27-none-win32.whl")
add_custom_target(pylinphone_wheel ALL
	${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/build_wheel/linphone/setup.py" "bdist_wheel"
	DEPENDS pylinphone_zip
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_wheel/linphone"
)
install(FILES ${WHEEL_FILENAME} DESTINATION ".")

endif()

if(UNIX)

# Generate the zip
set(ZIP_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_zip/linphone/dist/linphone-${LINPHONE_GIT_REVISION}.linux-${PLATFORM_ARCH_NAME}.zip")
add_custom_target(pylinphone_zip ALL
	${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/build_zip/linphone/setup.py" "bdist" "--format=zip"
	DEPENDS pylinphone_debug
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_zip/linphone"
)
install(FILES ${ZIP_FILENAME} DESTINATION "." RENAME "linphone-${LINPHONE_GIT_REVISION}.linux-${PLATFORM_ARCH_NAME}-py${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}.zip")

# Generate the wheel package
string(REPLACE "-" "_" LINPHONE_UNDERSCORE_GIT_REVISION ${LINPHONE_GIT_REVISION})
set(WHEEL_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_wheel/linphone/dist/linphone-${LINPHONE_UNDERSCORE_GIT_REVISION}-cp27-none-linux_${PLATFORM_ARCH_NAME}.whl")
add_custom_target(pylinphone_wheel ALL
	${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/build_wheel/linphone/setup.py" "bdist_wheel"
	DEPENDS pylinphone_zip
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_wheel/linphone"
)
install(FILES ${WHEEL_FILENAME} DESTINATION ".")

endif()
