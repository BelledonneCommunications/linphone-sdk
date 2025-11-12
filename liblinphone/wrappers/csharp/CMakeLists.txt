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

add_custom_command(OUTPUT LinphoneWrapper.cs
	COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_INSTALL_DATADIR}/linphonecs/"
	COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/genwrapper.py" "${PROJECT_BINARY_DIR}/coreapi/help/doc/doxygen/xml"
	DEPENDS ${PROJECT_SOURCE_DIR}/tools/genapixml.py
	${PROJECT_SOURCE_DIR}/tools/metadoc.py
	${PROJECT_SOURCE_DIR}/tools/abstractapi.py
	genwrapper.py
	wrapper_impl.mustache
	${PROJECT_BINARY_DIR}/coreapi/help/doc/doxygen/xml/index.xml
	liblinphone-doc
	COMMENT "Generating C# wrapper"
)

add_custom_target(liblinphone-csharp ALL DEPENDS LinphoneWrapper.cs)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LinphoneWrapper.cs"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/linphonecs/"
)

if(ENABLE_CSHARP_WRAPPER_TESTER)
	# Add the project to the install data dir next to the LinphoneWrapper.cs
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/LinphoneWrapper.Tests"
			DESTINATION "${CMAKE_INSTALL_DATADIR}/linphonecs/"
	)
endif()

if(ENABLE_DOC OR ENABLE_CSHARP_WRAPPER)
	message(CHECK_START "Generate C# wrapper html site")
	find_program(
        DOCFX_EXECUTABLE
        NAMES docfx
    )

	if(DOCFX_EXECUTABLE)
		file(
			COPY "${CMAKE_CURRENT_SOURCE_DIR}/docfx_project"
			DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/" 
		)

		# Remove everything after the last point in LINPHONE_VERSION (4.5.0 --> 4.5) to create STRIPPED_LINPHONE_VERSION
		string(REGEX REPLACE "\\.[^.]*$" "" STRIPPED_LINPHONE_VERSION ${LINPHONE_VERSION})
		# Replace @LINPHONESDK_STATE@ and @STRIPPED_LINPHONE_VERSION@ in site main page
		configure_file(
			"${CMAKE_CURRENT_SOURCE_DIR}/docfx_project/index.md"
			"${CMAKE_CURRENT_BINARY_DIR}/docfx_project/index.md"
		)
		# Replace @LINPHONESDK_STATE@ and @STRIPPED_LINPHONE_VERSION@ version.txt.in, this
		# file will be used for csharp doc upload
		configure_file(
			"${CMAKE_CURRENT_SOURCE_DIR}/version.txt.in"
			"${CMAKE_CURRENT_BINARY_DIR}/version.txt"
		)

		if(WIN32)
			set(SED_BACKSLASH "\\\\\\")
			set(SED_ESCAPE_CHARACTER "\^")
			set(SED_BINARY_MODE "-b")
		else()
			set(SED_BACKSLASH "\\\\")
			set(SED_ESCAPE_CHARACTER "")
			set(SED_BINARY_MODE "")
		endif()

		if(APPLE)
			set(SED_IN_PLACE_EDIT_ARG "-i ''")
		else()
			set(SED_IN_PLACE_EDIT_ARG "-i")
		endif()

		configure_file("${PROJECT_SOURCE_DIR}/coreapi/help/doc/doxygen/provisioning_configuration_key.dox.in" "${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md.raw")
		add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md"
			COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md

			COMMAND sed -r 's/\#Linphone\(\[A-Za-z\]+\)/\@Linphone\.\\1/g' ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md.raw > ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md
			COMMAND sed ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\/\\\*//g' ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md
			COMMAND sed ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\\*\\///g' ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md
			COMMAND sed ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\[\[:space:\]\]*\\\*\[\[:space:\]\]*//g' ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md
			COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\[\[:space:\]\]*${SED_BACKSLASH}defgroup\[\[:space:\]\]+\[A-Za-z\\_\]+\[\[:space:\]\]+\(\[A-Za-z\[:space:\]\]+\)/\# \\1/g' ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md
			COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/${SED_ESCAPE_CHARACTER}\<h2${SED_ESCAPE_CHARACTER}\>\(\.+\)${SED_ESCAPE_CHARACTER}\<\\/h2${SED_ESCAPE_CHARACTER}\>/\\\#\\\# \\1/g' ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md
			COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/${SED_ESCAPE_CHARACTER}\<li${SED_ESCAPE_CHARACTER}\>\(\.\*\)${SED_ESCAPE_CHARACTER}\<\\/li${SED_ESCAPE_CHARACTER}\>/\- \\1/g' ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md
			COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/${SED_ESCAPE_CHARACTER}\<ul${SED_ESCAPE_CHARACTER}\>//g' ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md
			COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/${SED_ESCAPE_CHARACTER}\<\\/ul${SED_ESCAPE_CHARACTER}\>//g' ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md
			COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/Table ${SED_BACKSLASH}ref \[A-Za-z\\_\]\+ \\\"\[A-Za-z\[:space:\]\\_\]\+\\\"/The table below/g' ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md.raw
			liblinphone-doc
		)

		add_custom_command(OUTPUT "docfx_project/_site/index.html"
			COMMAND ${CMAKE_COMMAND} -E copy
					"${CMAKE_CURRENT_BINARY_DIR}/LinphoneWrapper.cs"
					"${CMAKE_CURRENT_BINARY_DIR}/docfx_project/src/LinphoneWrapper.cs"
			COMMAND ${DOCFX_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/docfx_project/docfx.json"
			DEPENDS LinphoneWrapper.cs
			"${CMAKE_CURRENT_BINARY_DIR}/docfx_project/index.md"
			"${CMAKE_CURRENT_BINARY_DIR}/docfx_project/provisioning_configuration_key.md"
		)

		add_custom_target(
			liblinphone-csharp-doc-site 
			ALL 
			DEPENDS liblinphone-csharp "docfx_project/_site/index.html"
		)

		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/version.txt"
			DESTINATION "${CMAKE_INSTALL_DATADIR}/"
		)
		if(ENABLE_DOC)
			install(
				DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docfx_project/_site/"
				DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/liblinphone-${LINPHONE_VERSION}/cs"
				)
		endif()

		message(CHECK_PASS "generation and copy configured")
	else()
		message(CHECK_FAIL "docfx wasn't found, no site generated")
	
	endif()
endif()
