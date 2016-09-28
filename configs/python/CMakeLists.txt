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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

cmake_minimum_required(VERSION 2.8.12)
project(PYLINPHONE C CXX) # Dummy project

set(Python_ADDITIONAL_VERSIONS 2.7)
find_package(PythonInterp REQUIRED)
find_package(PythonLibs 2.7 REQUIRED)
find_package(Git)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
find_package(Linphone REQUIRED)
find_package(XML2 REQUIRED)

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
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/api.xml"
	COMMAND ${PYTHON_EXECUTABLE} "${LINPHONE_SOURCE_DIR}/tools/genapixml.py" "--pretty" "--outputfile" "${CMAKE_CURRENT_BINARY_DIR}/api.xml" "${XML_DIR}"
	DEPENDS ${XML_FILES} "${LINPHONE_SOURCE_DIR}/tools/genapixml.py"
)
add_custom_target(api_xml DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/api.xml")

# Generate the Python wrapper source code for the API in XML format
set(SOURCE_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/linphone.c")
add_custom_command(OUTPUT "${SOURCE_FILENAME}"
	COMMAND ${PYTHON_EXECUTABLE} "${LINPHONE_SOURCE_DIR}/tools/python/apixml2python.py" "--outputfile" "${SOURCE_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/api.xml"
	DEPENDS api_xml "${LINPHONE_SOURCE_DIR}/tools/python/apixml2python.py" "${LINPHONE_SOURCE_DIR}/tools/python/apixml2python/linphone.py"
	WORKING_DIRECTORY "${LINPHONE_SOURCE_DIR}/tools/python"
)
add_custom_target(pylinphone_source DEPENDS "${SOURCE_FILENAME}")

if(WIN32)
	set(PACKAGES_TO_BUILD "exe" "msi" "zip" "wheel")
	set(PY_MODULE_EXT ".pyd")
	string(REGEX REPLACE "Visual Studio ([0-9]+).*" "\\1" MSVC_VERSION "${CMAKE_GENERATOR}")
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(MSVC_DEBUG_SYSTEM_LIBRARIES "d")
	endif()
	find_file(MSVCP_LIB msvcp${MSVC_VERSION}0${MSVC_DEBUG_SYSTEM_LIBRARIES}.dll PATHS "C:/Windows/System32")
	# Starting with Visual Studio 2015 (MSVC_VERSION==14) the msvcr dll has been renamed to vcruntime
	find_file(VCRUNTIME_LIB vcruntime${MSVC_VERSION}0${MSVC_DEBUG_SYSTEM_LIBRARIES}.dll PATHS "C:/Windows/System32")
	if(NOT VCRUNTIME_LIB)
		find_file(VCRUNTIME_LIB msvcr${MSVC_VERSION}0${MSVC_DEBUG_SYSTEM_LIBRARIES}.dll PATHS "C:/Windows/System32")
	endif()
	if(NOT MSVCP_LIB OR NOT VCRUNTIME_LIB)
		message(FATAL_ERROR "You need to install the Visual Studio C++ Redistributable libraries!")
	endif()
	file(COPY ${MSVCP_LIB} ${VCRUNTIME_LIB} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
	file(GLOB LINPHONE_DYNAMIC_LIBRARIES_TO_INSTALL "${CMAKE_INSTALL_PREFIX}/bin/*.dll")
else()
	set(PACKAGES_TO_BUILD "zip" "wheel")
	set(PY_MODULE_EXT ".so")
	set(LINPHONE_DYNAMIC_LIBRARIES_TO_INSTALL )
	if(APPLE)
		if(ENABLE_FFMPEG)
			list(APPEND LINPHONE_DYNAMIC_LIBRARIES_TO_INSTALL
				"${CMAKE_INSTALL_PREFIX}/lib/libavcodec.53.61.100.dylib"
				"${CMAKE_INSTALL_PREFIX}/lib/libavutil.51.35.100.dylib"
				"${CMAKE_INSTALL_PREFIX}/lib/libswscale.2.1.100.dylib"
			)
		endif()
	else()
		if(ENABLE_FFMPEG)
			list(APPEND LINPHONE_DYNAMIC_LIBRARIES_TO_INSTALL
				"${CMAKE_INSTALL_PREFIX}/lib/libavcodec.so.53.61.100"
				"${CMAKE_INSTALL_PREFIX}/lib/libavutil.so.51.35.100"
				"${CMAKE_INSTALL_PREFIX}/lib/libswscale.so.2.1.100"
			)
		endif()
	endif()
endif()

if(UNIX)
	find_program(LORDER_EXECUTABLE lorder HINTS "${CMAKE_CURRENT_SOURCE_DIR}")
	if(NOT LORDER_EXECUTABLE)
		message(FATAL_ERROR "lorder executable is needed.")
	endif()
	find_program(TSORT_EXECUTABLE tsort)
	if(NOT TSORT_EXECUTABLE)
		message(FATAL_ERROR "tsort executable is needed.")
	endif()
	execute_process(COMMAND ${LORDER_EXECUTABLE} ${LINPHONE_LIBRARIES} OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/lorder.out")
	execute_process(COMMAND ${TSORT_EXECUTABLE} OUTPUT_VARIABLE LINPHONE_LIBRARIES INPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/lorder.out")
	separate_arguments(LINPHONE_LIBRARIES UNIX_COMMAND "${LINPHONE_LIBRARIES}")
endif()

if(APPLE)
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-flat_namespace,-undefined,dynamic_lookup")
endif()

add_library(pylinphone MODULE "${SOURCE_FILENAME}")
if(UNIX)
	target_compile_options(pylinphone PRIVATE "-Werror" "-Wno-deprecated-declarations")
endif()
if(ENABLE_OPENH264)
	find_library(MSOPENH264_LIBRARY msopenh264 HINTS "${CMAKE_INSTALL_PREFIX}/lib/mediastreamer/plugins")
	find_library(OPENH264_LIBRARY openh264)
	target_compile_definitions(pylinphone PRIVATE "ENABLE_OPENH264=1")
	target_link_libraries(pylinphone ${MSOPENH264_LIBRARY} ${OPENH264_LIBRARY})
endif()
if(ENABLE_WASAPI)
	find_library(MSWASAPI_LIBRARY mswasapi HINTS "${CMAKE_INSTALL_PREFIX}/lib/mediastreamer/plugins")
	target_compile_definitions(pylinphone PRIVATE "ENABLE_WASAPI=1")
	target_link_libraries(pylinphone ${MSWASAPI_LIBRARY})
endif()
target_compile_options(pylinphone PUBLIC ${LINPHONE_CPPFLAGS})
target_include_directories(pylinphone PUBLIC ${LINPHONE_INCLUDE_DIRS} ${XML2_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIRS} PRIVATE "${LINPHONE_SOURCE_DIR}/coreapi" "${LINPHONE_SOURCE_DIR}/include")
set_target_properties(pylinphone PROPERTIES LINKER_LANGUAGE CXX)
set(PYLINPHONE_LINK_LIBRARIES ${LINPHONE_LIBRARIES} ${XML2_LIBRARIES})
if(WIN32)
	find_library(GCC_LIBRARIES gcc)
	find_library(MINGWEX_LIBRARIES mingwex)
	list(APPEND PYLINPHONE_LINK_LIBRARIES ${PYTHON_LIBRARIES} ${GCC_LIBRARIES} ${MINGWEX_LIBRARIES})
endif()
target_link_libraries(pylinphone ${PYLINPHONE_LINK_LIBRARIES})
string(REPLACE ";" " " LINPHONE_LDFLAGS_STR "${LINPHONE_LDFLAGS}")
set(PYLINPHONE_LDFLAGS "${LINPHONE_LDFLAGS_STR}")
if(PYLINPHONE_LDFLAGS)
	set_target_properties(pylinphone PROPERTIES LINK_FLAGS "${PYLINPHONE_LDFLAGS}")
endif()
set_target_properties(pylinphone PROPERTIES OUTPUT_NAME linphone PREFIX "" SUFFIX "${PY_MODULE_EXT}")
if(UNIX)
	if(APPLE)
		set_target_properties(pylinphone PROPERTIES INSTALL_NAME_DIR "@executable_path")
	else()
		set_target_properties(pylinphone PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE)
	endif()
endif()
foreach(PACKAGE_TO_BUILD ${PACKAGES_TO_BUILD})
	add_custom_command(TARGET pylinphone POST_BUILD
		COMMAND "${CMAKE_COMMAND}" "-DPACKAGE_NAME=${PACKAGE_NAME}" "-DLINPHONE_VERSION=${LINPHONE_VERSION}" "-DLINPHONE_GIT_REVISION=${LINPHONE_GIT_REVISION}"
			"-DLINPHONE_PYTHON_MODULE=$<TARGET_FILE:pylinphone>" "-DLINPHONE_RESOURCES_PREFIX=${CMAKE_INSTALL_PREFIX}"
			"-DLINPHONE_DYNAMIC_LIBRARIES_TO_INSTALL=\"${LINPHONE_DYNAMIC_LIBRARIES_TO_INSTALL}\"" "-DPACKAGE_TYPE=${PACKAGE_TO_BUILD}"
			"-DINPUT_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}" "-DLINPHONE_SOURCE_DIR=${LINPHONE_SOURCE_DIR}"
			"-P" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_packaging.cmake"
	)
	if(APPLE)
		add_custom_command(TARGET pylinphone POST_BUILD
			COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/mac_rpath.py" "${CMAKE_CURRENT_BINARY_DIR}/build_${PACKAGE_TO_BUILD}/linphone/linphone"
		)
	endif()
endforeach()


if(WIN32)
	set(PLATFORM_TAG "win32")
	set(HOST_PLATFORM_TAG "${PLATFORM_TAG}")
	set(UNDERSCORE_PLATFORM_TAG "${PLATFORM_TAG}")
else()
	if(CMAKE_CROSSCOMPILING)
		string(TOLOWER "${CMAKE_SYSTEM_NAME}" PLATFORM_OS)
		string(TOLOWER "${CMAKE_HOST_SYSTEM_NAME}" HOST_PLATFORM_OS)
		set(PLATFORM_TAG "${PLATFORM_OS}-${CMAKE_SYSTEM_PROCESSOR}")
		set(HOST_PLATFORM_TAG "${HOST_PLATFORM_OS}-${CMAKE_HOST_SYSTEM_PROCESSOR}")
		set(UNDERSCORE_PLATFORM_TAG "any")
	else()
		set(PLATFORM_ARCH_NAME "${CMAKE_SYSTEM_PROCESSOR}")
		if(APPLE)
			if(CMAKE_OSX_ARCHITECTURES)
				set(PLATFORM_ARCH_NAME "${CMAKE_OSX_ARCHITECTURES}")
			endif()
			if(CMAKE_OSX_DEPLOYMENT_TARGET)
				set(DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
			else()
				set(DEPLOYMENT_TARGET "10.7")
			endif()
			set(PLATFORM_OS "macosx-${DEPLOYMENT_TARGET}")
		else()
			set(PLATFORM_OS "linux")
		endif()
		string(REPLACE "-" "_" UNDERSCORE_PLATFORM_OS ${PLATFORM_OS})
		string(REPLACE "." "_" UNDERSCORE_PLATFORM_OS ${UNDERSCORE_PLATFORM_OS})
		set(PLATFORM_TAG "${PLATFORM_OS}-${PLATFORM_ARCH_NAME}")
		set(HOST_PLATFORM_TAG "${PLATFORM_TAG}")
		set(UNDERSCORE_PLATFORM_TAG "${UNDERSCORE_PLATFORM_OS}_${PLATFORM_ARCH_NAME}")
	endif()
endif()
string(REPLACE "-" "_" LINPHONE_UNDERSCORE_GIT_REVISION ${LINPHONE_GIT_REVISION})


if(WIN32)

# Generate the installer
set(EXE_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_exe/linphone/dist/${PACKAGE_NAME}-${LINPHONE_GIT_REVISION}.${PLATFORM_TAG}.exe")
add_custom_command(OUTPUT "${EXE_FILENAME}"
	COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/build_exe/linphone/setup.py" "bdist_wininst"
	DEPENDS pylinphone
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_exe/linphone"
)
add_custom_target(pylinphone_exe ALL DEPENDS "${EXE_FILENAME}")
install(FILES ${EXE_FILENAME} DESTINATION "." RENAME "${PACKAGE_NAME}-${LINPHONE_GIT_REVISION}.${PLATFORM_TAG}-py${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}.exe")

# Generate the msi
set(MSI_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_msi/linphone/dist/${PACKAGE_NAME}-${LINPHONE_VERSION}.win32.msi")
add_custom_command(OUTPUT "${MSI_FILENAME}"
	COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/build_msi/linphone/setup.py" "bdist_msi"
	DEPENDS pylinphone
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_msi/linphone"
)
add_custom_target(pylinphone_msi ALL DEPENDS "${MSI_FILENAME}")
install(FILES ${MSI_FILENAME} DESTINATION "." RENAME "${PACKAGE_NAME}-${LINPHONE_GIT_REVISION}.${PLATFORM_TAG}-py${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}.msi")

endif()

# Generate the zip
set(ZIP_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_zip/linphone/dist/${PACKAGE_NAME}-${LINPHONE_GIT_REVISION}.${HOST_PLATFORM_TAG}.zip")
add_custom_command(OUTPUT "${ZIP_FILENAME}"
	COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/build_zip/linphone/setup.py" "bdist" "--format=zip"
	DEPENDS pylinphone
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_zip/linphone"
)
add_custom_target(pylinphone_zip ALL DEPENDS "${ZIP_FILENAME}")
install(FILES ${ZIP_FILENAME} DESTINATION "." RENAME "${PACKAGE_NAME}-${LINPHONE_GIT_REVISION}.${PLATFORM_TAG}-py${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}.zip")

# Generate the wheel package
set(WHEEL_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_wheel/linphone/dist/${PACKAGE_NAME}-${LINPHONE_UNDERSCORE_GIT_REVISION}-py2-none-any.whl")
add_custom_command(OUTPUT "${WHEEL_FILENAME}"
	COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/build_wheel/linphone/setup.py" "bdist_wheel"
	DEPENDS pylinphone
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_wheel/linphone"
)
add_custom_target(pylinphone_wheel ALL DEPENDS "${WHEEL_FILENAME}")
install(FILES ${WHEEL_FILENAME} DESTINATION "." RENAME "${PACKAGE_NAME}-${LINPHONE_UNDERSCORE_GIT_REVISION}-cp27-none-${UNDERSCORE_PLATFORM_TAG}.whl")
