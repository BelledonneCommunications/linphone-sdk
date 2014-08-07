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

find_package(PythonInterp REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_PREFIX_PATH}/share/cmake/Modules)
find_package(Linphone REQUIRED)
message("LINPHONE_LIBRARIES: ${LINPHONE_LIBRARIES}")
message("LINPHONE_INCLUDE_DIRS: ${LINPHONE_INCLUDE_DIRS}")

set(LINPHONE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../EP_linphone")

# Find the doxygen XML directory
file(GLOB XML_DIR "${CMAKE_INSTALL_PREFIX}/share/doc/linphone-*/xml")

# Generate the API in XML format from the doxygen XML files
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/api.xml"
	DEPENDS ${XML_FILES}
	COMMAND ${PYTHON_EXECUTABLE} "${LINPHONE_SOURCE_DIR}/tools/genapixml.py" "--pretty" "--outputfile" "${CMAKE_CURRENT_BINARY_DIR}/api.xml" "${XML_DIR}"
)

# Generate the Python wrapper source code for the API in XML format
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/linphone.c"
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/api.xml"
	COMMAND ${PYTHON_EXECUTABLE} "${LINPHONE_SOURCE_DIR}/tools/python/apixml2python.py" "--outputfile" "${CMAKE_CURRENT_BINARY_DIR}/linphone.c" "${CMAKE_CURRENT_BINARY_DIR}/api.xml"
	WORKING_DIRECTORY "${LINPHONE_SOURCE_DIR}/tools/python"
)

# Generate setup.py file that will be used to generate the package
if(WIN32)
	list(APPEND LINPHONE_CPPFLAGS "-DWIN32")
	list(APPEND LINPHONE_LIBRARIES gcc mingwex)
	file(GLOB LINPHONE_DYNAMIC_LIBRARIES "${CMAKE_INSTALL_PREFIX}/bin/*.dll")
endif()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/linphone")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/linphone/__init__.py" "import linphone")
file(COPY ${LINPHONE_DYNAMIC_LIBRARIES} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/linphone")
file(COPY "${CMAKE_INSTALL_PREFIX}/share/images" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/linphone/share/")
file(COPY "${CMAKE_INSTALL_PREFIX}/share/linphone" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/linphone/share/")
file(COPY "${CMAKE_INSTALL_PREFIX}/share/sounds" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/linphone/share/")
file(GLOB_RECURSE LINPHONE_DATA_FILES RELATIVE "${CMAKE_CURRENT_BINARY_DIR}/linphone" "${CMAKE_CURRENT_BINARY_DIR}/linphone/*")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/setup.py.cmake" "${CMAKE_CURRENT_BINARY_DIR}/setup.py")

# Generate the installer
#set(ENV{VS90COMNTOOLS} "$ENV{VS110COMNTOOLS}")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/linphone.exe"
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/linphone.c"
	COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/setup.py" "bdist_wininst"
)

add_custom_target(custom_pylinphone ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/linphone.exe")
