############################################################################
# CMakeLists.txt
# Copyright (c) 2010-2026 Belledonne Communications SARL.
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

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/LinphoneWrapper.cs
		COMMAND ${CMAKE_COMMAND} -E remove_directory include
		COMMAND ${CMAKE_COMMAND} -E remove_directory src
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/genwrapper.py" "${PROJECT_BINARY_DIR}/coreapi/help/doc/doxygen/xml"
		DEPENDS ${PROJECT_SOURCE_DIR}/tools/genapixml.py
		${PROJECT_SOURCE_DIR}/tools/metadoc.py
		${PROJECT_SOURCE_DIR}/tools/metaname.py
		${PROJECT_SOURCE_DIR}/tools/abstractapi.py
		genwrapper.py
		wrapper_impl.mustache
		${PROJECT_BINARY_DIR}/coreapi/help/doc/doxygen/xml/index.xml
		liblinphone-doc
		COMMENT "Generating C# wrapper"
)

include("${PROJECT_SOURCE_DIR}/coreapi/help/doc/doxygen/cmake_doxygen_sed_fixes.txt")

#Replacing linphone_core_set_tag_100rel_support_level
set(SED_FUNCTION_CASE_1 "${SED_QUOTES}s/linphone_core_set_tag_100rel_support_level/Linphone.Core.Tag100RelSupportLevel/g${SED_QUOTES}" )
#Replacing linphone_core_set_refresh_window
set(SED_FUNCTION_CASE_2 "${SED_QUOTES}s/linphone_core_set_refresh_window/Linphone.Core.SetRefreshWindow/g${SED_QUOTES}" )

# Replacing linphone_nat_policy_***
#-r ':a;s/linphone_nat_policy_(set_|get_|)([a-z0-9_\(\)]*)/Linphone.NatPolicy.\u\2/g;s/(Linphone.NatPolicy.[^()]+)_([a-z0-9_\(\)])/\1\u\2/g;ta'
set(SED_CLASS_CASE_1 "${SED_QUOTES}:a\;s/linphone_nat_policy_(set_|get_|)([a-z0-9_\\(\\)]*)/Linphone.NatPolicy.\\u\\2/g\;s/(Linphone.NatPolicy.[^()]+)_([a-z0-9_\\(\\)])/\\1\\u\\2/g\;ta${SED_QUOTES}")
#Replacing linphone_core_***
#-r ':a;s/linphone_core_(set_|get_|)([a-z0-9_\(\)]*)/Linphone.Core.\u\2/g;s/(Linphone.Core.[^()]+)_([a-z0-9_\(\)])/\1\u\2/g;ta'
set(SED_CLASS_CASE_2 "${SED_QUOTES}:a\;s/linphone_core_(set_|get_|)([a-z0-9_\\(\\)]*)/Linphone.Core.\\u\\2/g\;s/(Linphone.Core.[^()]+)_([a-z0-9_\\(\\)])/\\1\\u\\2/g\;ta${SED_QUOTES}")

set(PROVISIONING_DOC_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.dox)
configure_file("${PROJECT_SOURCE_DIR}/coreapi/help/doc/doxygen/provisioning_configuration_key.dox.in" "${PROVISIONING_DOC_OUTPUT_FILE}.raw")

add_custom_command(OUTPUT "${PROVISIONING_DOC_OUTPUT_FILE}"
	COMMAND ${CMAKE_COMMAND} -E remove -f ${PROVISIONING_DOC_OUTPUT_FILE}
	COMMAND sed -r 's/\#Linphone\(\[A-Za-z\]+\)/Linphone\.\\1/g' ${PROVISIONING_DOC_OUTPUT_FILE}.raw > ${PROVISIONING_DOC_OUTPUT_FILE}
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} ${SED_GENERIC_CASE_1} ${PROVISIONING_DOC_OUTPUT_FILE}
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} ${SED_FUNCTION_CASE_1} ${PROVISIONING_DOC_OUTPUT_FILE}
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} ${SED_FUNCTION_CASE_2} ${PROVISIONING_DOC_OUTPUT_FILE}
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} ${SED_CLASS_CASE_1} ${PROVISIONING_DOC_OUTPUT_FILE}
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} ${SED_CLASS_CASE_2} ${PROVISIONING_DOC_OUTPUT_FILE}
	DEPENDS ${PROVISIONING_DOC_OUTPUT_FILE}.raw
	liblinphone-doc
)

add_custom_target(liblinphone-csharp-wrapper ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/LinphoneWrapper.cs")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LinphoneWrapper.cs" DESTINATION "${CMAKE_INSTALL_DATADIR}/linphonecs/")
if(ENABLE_CSHARP_WRAPPER_TESTER)
	# Add the project to the install data dir next to the LinphoneWrapper.cs
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/LinphoneWrapper.Tests"
			DESTINATION "${CMAKE_INSTALL_DATADIR}/linphonecs/"
	)
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
		"${CMAKE_CURRENT_BINARY_DIR}/LinphoneCsVersion.cmake"
		VERSION ${PROJECT_VERSION}
		COMPATIBILITY AnyNewerVersion
)

install(FILES
		"${CMAKE_CURRENT_BINARY_DIR}/LinphoneCsVersion.cmake"
		DESTINATION "${CMAKE_INSTALL_DATADIR}/LinphoneCs/cmake"
)


if(ENABLE_DOC OR ENABLE_CSHARP_WRAPPER)
	find_package(Doxygen REQUIRED)
	if(DOXYGEN_FOUND)
		set(top_srcdir "${PROJECT_SOURCE_DIR}")

		# Remove everything after the last point in LINPHONE_VERSION (4.5.0 --> 4.5) to create STRIPPED_LINPHONE_VERSION
		string(REGEX REPLACE "\\.[^.]*$" "" STRIPPED_LINPHONE_VERSION ${LINPHONE_VERSION})
		# Replace @LINPHONESDK_STATE@ and @STRIPPED_LINPHONE_VERSION@ in site main page

		set(DOXYFILE_INPUT "")
		string(CONCAT DOXYFILE_INPUT ${DOXYFILE_INPUT} " \"${CMAKE_CURRENT_BINARY_DIR}/LinphoneWrapper.cs\"")
		string(CONCAT DOXYFILE_INPUT ${DOXYFILE_INPUT} " \"${CMAKE_CURRENT_BINARY_DIR}/doxygen.dox\"")
		string(CONCAT DOXYFILE_INPUT ${DOXYFILE_INPUT} " \"${PROVISIONING_DOC_OUTPUT_FILE}\"")
		set(DOXYFILE_OPTIMIZE_OUTPUT_FOR_C NO)
		set(DOXYFILE_FILE_PATTERNS "*.hh *.dox" )
		set(DOXYFILE_EXAMPLE_PATH "")
		set(DOXYFILE_HTML_OUTPUT cs)
		set(DOXYFILE_GENERATE_XML NO)
		if(DOXYGEN_VERSION VERSION_LESS "1.9.0")
			set(DOXYFILE_ENABLED_SECTIONS "")
		else()
			set(DOXYFILE_ENABLED_SECTIONS "DOXYFILE_USE_COPYBRIEF DOXYFILE_USE_DETAILS")
		endif()
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doxygen.dox.in ${CMAKE_CURRENT_BINARY_DIR}/doxygen.dox)
		configure_file("${PROJECT_SOURCE_DIR}/coreapi/help/doc/doxygen/custom_design.css.in" custom_design.css @ONLY)
		configure_file(${PROJECT_SOURCE_DIR}/coreapi/help/doc/doxygen/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
		set(DOC_INPUT_FILES ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
				"${CMAKE_CURRENT_BINARY_DIR}/LinphoneWrapper.cs"
				${CMAKE_CURRENT_BINARY_DIR}/doxygen.dox
				${PROVISIONING_DOC_OUTPUT_FILE}
		)

		set(CSHARP_HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/cs")
		set(LINPHONE_DOXYGEN_CSHARP_HTML_DIR ${CSHARP_HTML_DIR} PARENT_SCOPE)
		add_custom_command(OUTPUT "${CSHARP_HTML_DIR}/index.html"
				COMMAND ${CMAKE_COMMAND} -E remove -f ${CSHARP_HTML_DIR}/*
				COMMAND ${CMAKE_COMMAND} -E make_directory ${CSHARP_HTML_DIR}
				COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
				DEPENDS ${DOC_INPUT_FILES}
				liblinphone-csharp-wrapper
		)
		add_custom_target(liblinphone-csharp-html-doc ALL DEPENDS "${CSHARP_HTML_DIR}/index.html")
		if(ENABLE_DOC)
			install(DIRECTORY "${CSHARP_HTML_DIR}"
					DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/liblinphone-${LINPHONE_VERSION}")
		endif()
	endif()
endif()
