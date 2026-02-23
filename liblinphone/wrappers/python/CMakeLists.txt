############################################################################
# CMakeLists.txt
# Copyright (C) 2020 Belledonne Communications, Grenoble France
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

execute_process(
	COMMAND "${PYTHON_EXECUTABLE}" "--version"
	OUTPUT_VARIABLE PYTHON_VERSION
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (PYTHON_VERSION MATCHES "^Python 3[.]([0-9]+)[.]([0-9]+)$")
	set(Python3_EXECUTABLE "${PYTHON_EXECUTABLE}")
else()
	find_package(Python3 REQUIRED)
endif()

add_custom_command(OUTPUT pylinphone.pyx pylinphone.py pylinphone.pyi
	COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/genwrapper.py" "${PROJECT_BINARY_DIR}/coreapi/help/doc/doxygen/xml"
	DEPENDS ${PROJECT_SOURCE_DIR}/tools/genapixml.py
	${PROJECT_SOURCE_DIR}/tools/metadoc.py
	${PROJECT_SOURCE_DIR}/tools/abstractapi.py
	genwrapper.py
	pylinphone.mustache
	pylinphone_stub.mustache
	liblinphone-doc
	COMMENT "Generating python wrapper"
)

add_custom_command(OUTPUT pylinphone.c
	COMMAND "cython" "--module-name" "pylinphone" "$<$<CONFIG:Debug,RelWithDebInfo>:--gdb>" "pylinphone.pyx"
	DEPENDS pylinphone.pyx
)

list(APPEND PYLINPHONE_DEPENDENCIES "pylinphone.pyx" "pylinphone.pyi" "pylinphone.c")
if (ENABLE_DOC)

	if (WIN32)
		set(SED_BACKSLASH "\\\\\\")
		set(SED_ESCAPE_CHARACTER "\^")
		set(SED_BINARY_MODE "-b")
	else()
		set(SED_BACKSLASH "\\\\")
		set(SED_ESCAPE_CHARACTER "")
		set(SED_BINARY_MODE "")
	endif()

	if (APPLE)
		set(SED_IN_PLACE_EDIT_ARG "-i ''")
	else()
		set(SED_IN_PLACE_EDIT_ARG "-i")
	endif()

	string(REGEX REPLACE "\\.[^.]*$" "" STRIPPED_LINPHONE_VERSION ${LINPHONE_VERSION})
	configure_file(README.cmake README.md)

	configure_file(${PROJECT_SOURCE_DIR}/coreapi/help/doc/doxygen/provisioning_configuration_key.dox.in ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md.raw)
	add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md"
		COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} 's/\#Linphone\(\[A-Za-z\]+\)/\`\\1\`/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md.raw > ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\/\\\*//g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\\*\\///g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\[\[:space:\]\]*\\\*\[\[:space:\]\]*//g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\[\[:space:\]\]*${SED_BACKSLASH}defgroup\[\[:space:\]\]+\[A-Za-z\\_\]+\[\[:space:\]\]+\(\[A-Za-z\[:space:\]\]+\)/\\\# \\1/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/${SED_ESCAPE_CHARACTER}\<h2${SED_ESCAPE_CHARACTER}\>\(\.+\)${SED_ESCAPE_CHARACTER}\<\\/h2${SED_ESCAPE_CHARACTER}\>/\\\#\\\# \\1/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/${SED_ESCAPE_CHARACTER}\<li${SED_ESCAPE_CHARACTER}\>\(\.\*\)${SED_ESCAPE_CHARACTER}\<\\/li${SED_ESCAPE_CHARACTER}\>/\- \\1/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/${SED_ESCAPE_CHARACTER}\<ul${SED_ESCAPE_CHARACTER}\>//g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/${SED_ESCAPE_CHARACTER}\<\\/ul${SED_ESCAPE_CHARACTER}\>//g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/Table ${SED_BACKSLASH}ref \[A-Za-z\\_\]\+ \\\"\[A-Za-z\[:space:\]\\_\]\+\\\"/The table below/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/${SED_ESCAPE_CHARACTER}\<i${SED_ESCAPE_CHARACTER}\>\(\.+\)${SED_ESCAPE_CHARACTER}\<\\/i${SED_ESCAPE_CHARACTER}\>/\\1/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\_\\\#\\\#\(\[\[:alnum:\]\]\)/\\U\\1/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\_\\\#/\_/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/linphone::\(\[\[:alnum:\]\]+\)::\\_\(\[\[:alnum:\]\\_\]+\)\(\\\(\\\)\)/\`\\1\.\\2\\3\`/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/set_//g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/get_//g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.md.raw
		liblinphone-doc
	)

	add_custom_command(OUTPUT pylinphone.html
		COMMAND pdoc --output-dir ${CMAKE_INSTALL_PREFIX}/ --docformat restructuredtext --no-show-source --no-browser --no-include-undocumented --logo "https://linphone.org/linphone_logo.png" --logo-link "https://linphone.org" ./pylinphone.py
		DEPENDS pylinphone.py
		README.md
		provisioning_configuration_key.md
	)
	list(APPEND PYLINPHONE_DEPENDENCIES "pylinphone.html")
endif()

add_custom_target(liblinphone-python-wrapper ALL DEPENDS ${PYLINPHONE_DEPENDENCIES})

add_library(pylinphone MODULE pylinphone.c)
add_dependencies(pylinphone liblinphone-python-wrapper)

set_target_properties(pylinphone PROPERTIES OUTPUT_NAME pylinphone)
set_target_properties(pylinphone PROPERTIES PREFIX "")

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
include_directories(${Python3_INCLUDE_DIRS})

target_link_libraries(pylinphone
	PRIVATE ${BCToolbox_TARGET} ${BelleSIP_TARGET} liblinphone ${Python3_LIBRARIES}
)

target_include_directories(pylinphone
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${PROJECT_BINARY_DIR}/include
)
if (NOT APPLE)
	# see https://github.com/dealii/dealii/issues/2915
	set_target_properties(pylinphone PROPERTIES SOVERSION ${LINPHONE_SO_VERSION})
endif()

install(TARGETS pylinphone
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FRAMEWORK DESTINATION Frameworks
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pylinphone.pyi"
	DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(ENABLE_PYTHON_WRAPPER_TESTER)
	# Install the tests project into the install data dir
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/wrapper-tests" DESTINATION "${CMAKE_INSTALL_DATADIR}/pylinphone/")
endif()
