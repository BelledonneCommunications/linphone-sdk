############################################################################
# CMakeLists.txt
# Copyright (c) 2010-2022 Belledonne Communications SARL.
#
############################################################################
#
# This file is part of Liblinphone 
# (see https://gitlab.linphone.org/BC/public/liblinphone).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
############################################################################

if(ENABLE_DOC OR ENABLE_CXX_WRAPPER OR ENABLE_CSHARP_WRAPPER OR ENABLE_JAVA_WRAPPER OR ENABLE_SWIFT_WRAPPER OR ENABLE_PYTHON_WRAPPER)
	find_package(Doxygen REQUIRED)
	if(DOXYGEN_FOUND)
		set(top_srcdir "${PROJECT_SOURCE_DIR}")

		# Remove everything after the last point in LINPHONE_VERSION (4.5.0 --> 4.5) to create STRIPPED_LINPHONE_VERSION
		string(REGEX REPLACE "\\.[^.]*$" "" STRIPPED_LINPHONE_VERSION ${LINPHONE_VERSION})
		# Replace @LINPHONESDK_STATE@ and @STRIPPED_LINPHONE_VERSION@ in site main page

		include("cmake_doxygen_sed_fixes.txt")
		set(PROVISIONING_DOC_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.dox)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/provisioning_configuration_key.dox.in ${PROVISIONING_DOC_OUTPUT_FILE}.raw)

		add_custom_command(OUTPUT ${PROVISIONING_DOC_OUTPUT_FILE}
			COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.dox
			COMMAND sed -r ${SED_BINARY_MODE} 's/:://g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.dox.raw > ${PROVISIONING_DOC_OUTPUT_FILE}
			COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} ${SED_GENERIC_CASE_1} ${PROVISIONING_DOC_OUTPUT_FILE}
			DEPENDS ${PROVISIONING_DOC_OUTPUT_FILE}.raw
		)

		set(DOXYFILE_INPUT "")
		foreach (HEADER_FILE ${LINPHONE_HEADER_FILES})
			string(CONCAT DOXYFILE_INPUT ${DOXYFILE_INPUT} " \"${HEADER_FILE}\"")
		endforeach ()
		string(CONCAT DOXYFILE_INPUT ${DOXYFILE_INPUT} " \"${CMAKE_CURRENT_SOURCE_DIR}\"")
		string(CONCAT DOXYFILE_INPUT ${DOXYFILE_INPUT} " \"${PROJECT_SOURCE_DIR}/coreapi/help/examples/C\"")
		string(CONCAT DOXYFILE_INPUT ${DOXYFILE_INPUT} " \"${CMAKE_CURRENT_BINARY_DIR}/doxygen.dox\"")
		string(CONCAT DOXYFILE_INPUT ${DOXYFILE_INPUT} " \"${PROVISIONING_DOC_OUTPUT_FILE}\"")
		set(DOXYFILE_OPTIMIZE_OUTPUT_FOR_C YES)
		set(DOXYFILE_FILE_PATTERNS "*.c *.h *.dox" )
		set(DOXYFILE_EXAMPLE_PATH "${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/coreapi/help/examples/C")
		set(DOXYFILE_HTML_OUTPUT c)
		set(DOXYFILE_GENERATE_XML YES)
		if(DOXYGEN_VERSION VERSION_LESS "1.9.0")
			set(DOXYFILE_ENABLED_SECTIONS "DOXYFILE_USE_COPYBRIEF")
		else()
			set(DOXYFILE_ENABLED_SECTIONS "DOXYFILE_USE_COPYBRIEF DOXYFILE_USE_DETAILS")
		endif()
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doxygen.dox.in ${CMAKE_CURRENT_BINARY_DIR}/doxygen.dox)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/custom_design.css.in custom_design.css @ONLY)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

		set(DOC_INPUT_FILES ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
			${CMAKE_CURRENT_BINARY_DIR}/doxygen.dox
			${LINPHONE_HEADER_FILES}
			${PROVISIONING_DOC_OUTPUT_FILE}
		)
		set(XML_DIR "${CMAKE_CURRENT_BINARY_DIR}/xml")
		set(LINPHONE_DOXYGEN_XML_DIR ${XML_DIR} PARENT_SCOPE)
		set(C_HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/c")
		set(LINPHONE_DOXYGEN_C_HTML_DIR ${C_HTML_DIR} PARENT_SCOPE)
		add_custom_command(OUTPUT "${XML_DIR}/index.xml" "${C_HTML_DIR}/index.html"
			COMMAND ${CMAKE_COMMAND} -E remove -f ${XML_DIR}/*
			COMMAND ${CMAKE_COMMAND} -E make_directory ${XML_DIR}
			COMMAND ${CMAKE_COMMAND} -E remove -f ${C_HTML_DIR}/*
			COMMAND ${CMAKE_COMMAND} -E make_directory ${C_HTML_DIR}
			COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
			DEPENDS ${DOC_INPUT_FILES}
		)
		add_custom_target(liblinphone-doc ALL DEPENDS "${XML_DIR}/index.xml" "${C_HTML_DIR}/index.html")
		if(ENABLE_DOC)
			install(DIRECTORY "${C_HTML_DIR}"
				DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/liblinphone-${LINPHONE_VERSION}")
		endif()
	endif()
endif()

