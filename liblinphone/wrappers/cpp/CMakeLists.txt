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

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/linphone++/linphone.hh" "${CMAKE_CURRENT_BINARY_DIR}/src/linphone++.cc"
	COMMAND ${CMAKE_COMMAND} -E remove_directory include
	COMMAND ${CMAKE_COMMAND} -E remove_directory src
	COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/genwrapper.py" "${PROJECT_BINARY_DIR}/coreapi/help/doc/doxygen/xml"
	DEPENDS ${PROJECT_SOURCE_DIR}/tools/genapixml.py
	${PROJECT_SOURCE_DIR}/tools/metadoc.py
	${PROJECT_SOURCE_DIR}/tools/metaname.py
	${PROJECT_SOURCE_DIR}/tools/abstractapi.py
	genwrapper.py
	class_header.mustache
	class_impl.mustache
	enums_header.mustache
	main_header.mustache
	${PROJECT_BINARY_DIR}/coreapi/help/doc/doxygen/xml/index.xml
	liblinphone-doc
)

include("${PROJECT_SOURCE_DIR}/coreapi/help/doc/doxygen/cmake_doxygen_sed_fixes.txt")

#Replacing linphone_core_set_tag_100rel_support_level
set(SED_FUNCTION_CASE_1 "${SED_QUOTES}s/linphone_core_set_tag_100rel_support_level/linphone::Core::setTag100RelSupportLevel/g${SED_QUOTES}" )

# Replacing linphone_nat_policy_***
#-r ':a;s/linphone_nat_policy_([a-z0-9_\(\)]*)/linphone::NatPolicy::\l\1\n/g;s/(linphone::NatPolicy::[^()]+)_([a-z0-9_\(\)])/\1\u\2/g;ta'
set(SED_CLASS_CASE_1 "${SED_QUOTES}:a\;s/linphone_nat_policy_([a-z0-9_\\(\\)]*)/linphone::NatPolicy::\\l\\1\\n/g\;s/(linphone::NatPolicy::[^()]+)_([a-z0-9_\\(\\)])/\\1\\u\\2/g\;ta${SED_QUOTES}")
#Replacing linphone_core_***
#-r ':a;s/linphone_core_([a-z0-9_\(\)]*)/linphone::Core::\l\1\n/g;s/(linphone::Core::[^()]+)_([a-z0-9_\(\)])/\1\u\2/g;ta'
set(SED_CLASS_CASE_2 "${SED_QUOTES}:a\;s/linphone_core_([a-z0-9_\\(\\)]*)/linphone::Core::\\l\\1\\n/g\;s/(linphone::Core::[^()]+)_([a-z0-9_\\(\\)])/\\1\\u\\2/g\;ta${SED_QUOTES}")

set(PROVISIONING_DOC_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.dox)
configure_file("${PROJECT_SOURCE_DIR}/coreapi/help/doc/doxygen/provisioning_configuration_key.dox.in" "${PROVISIONING_DOC_OUTPUT_FILE}.raw")

add_custom_command(OUTPUT "${PROVISIONING_DOC_OUTPUT_FILE}"
	COMMAND ${CMAKE_COMMAND} -E remove -f ${PROVISIONING_DOC_OUTPUT_FILE}
	COMMAND sed -r ${SED_BINARY_MODE} 's/\#Linphone\(\[A-Za-z\]+\)/linphone::\\1/g' ${PROVISIONING_DOC_OUTPUT_FILE}.raw > ${PROVISIONING_DOC_OUTPUT_FILE}
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} ${SED_GENERIC_CASE_1} ${PROVISIONING_DOC_OUTPUT_FILE}
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} ${SED_FUNCTION_CASE_1} ${PROVISIONING_DOC_OUTPUT_FILE}
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} ${SED_CLASS_CASE_1} ${PROVISIONING_DOC_OUTPUT_FILE}
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} ${SED_CLASS_CASE_2} ${PROVISIONING_DOC_OUTPUT_FILE}
	DEPENDS ${PROVISIONING_DOC_OUTPUT_FILE}.raw
	liblinphone-doc
)

add_custom_target(liblinphone-cxx-wrapper ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/include/linphone++/linphone.hh" "${CMAKE_CURRENT_BINARY_DIR}/src/linphone++.cc")

add_library(liblinphone++
	object.cc
	tools.cc
	${CMAKE_CURRENT_BINARY_DIR}/src/linphone++.cc
)
add_dependencies(liblinphone++ liblinphone-cxx-wrapper)

if(APPLE)
	set_target_properties(liblinphone++ PROPERTIES OUTPUT_NAME linphone++)
else()
	set_target_properties(liblinphone++ PROPERTIES OUTPUT_NAME liblinphone++)
endif()
set_target_properties(liblinphone++ PROPERTIES PREFIX "")
if(MSVC)
    set_target_properties(liblinphone++ PROPERTIES PDB_NAME liblinphone++)
endif()
target_compile_definitions(liblinphone++ PRIVATE "-DLINPHONECXX_EXPORTS")
target_link_libraries(liblinphone++
	PRIVATE ${BCToolbox_TARGET} ${BelleSIP_TARGET} liblinphone
)

set_target_properties(liblinphone++ PROPERTIES
	FRAMEWORK TRUE
	MACOSX_FRAMEWORK_IDENTIFIER org.linphone.linphonecpp
	MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_SOURCE_DIR}/build/osx/Info.plist.in"
)

target_include_directories(liblinphone++
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${PROJECT_BINARY_DIR}/include
)
set_target_properties(liblinphone++ PROPERTIES SOVERSION ${LINPHONE_SO_VERSION})

install(TARGETS liblinphone++ EXPORT LinphoneCxxTargets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FRAMEWORK DESTINATION Frameworks
)
install(FILES object.hh
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/linphone++
)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/linphone++
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/LinphoneCxxVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)
configure_file(LinphoneCxxConfig.cmake.in LinphoneCxxConfig.cmake @ONLY)

install(EXPORT LinphoneCxxTargets
	FILE LinphoneCxxTargets.cmake
	DESTINATION "${CMAKE_INSTALL_DATADIR}/LinphoneCxx/cmake"
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/LinphoneCxxConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/LinphoneCxxVersion.cmake"
	DESTINATION "${CMAKE_INSTALL_DATADIR}/LinphoneCxx/cmake"
)

if(ENABLE_DOC OR ENABLE_CXX_WRAPPER)
	find_package(Doxygen REQUIRED)
	if(DOXYGEN_FOUND)
		set(top_srcdir "${PROJECT_SOURCE_DIR}")
		# Remove everything after the last point in LINPHONE_VERSION (4.5.0 --> 4.5) to create STRIPPED_LINPHONE_VERSION
		string(REGEX REPLACE "\\.[^.]*$" "" STRIPPED_LINPHONE_VERSION ${LINPHONE_VERSION})
		# Replace @LINPHONESDK_STATE@ and @STRIPPED_LINPHONE_VERSION@ in site main page
		set(DOXYFILE_INPUT "")
		string(CONCAT DOXYFILE_INPUT ${DOXYFILE_INPUT} " \"${CMAKE_CURRENT_BINARY_DIR}/include/linphone++\"")
		string(CONCAT DOXYFILE_INPUT ${DOXYFILE_INPUT} " \"${CMAKE_CURRENT_BINARY_DIR}/doxygen.dox\"")
		string(CONCAT DOXYFILE_INPUT ${DOXYFILE_INPUT} " \"${PROVISIONING_DOC_OUTPUT_FILE}\"")
		set(DOXYFILE_OPTIMIZE_OUTPUT_FOR_C NO)
		set(DOXYFILE_FILE_PATTERNS "*.hh *.dox" )
		set(DOXYFILE_EXAMPLE_PATH "")
		set(DOXYFILE_HTML_OUTPUT cpp)
		set(DOXYFILE_GENERATE_XML NO)
		if(DOXYGEN_VERSION VERSION_LESS "1.9.0")
			set(DOXYFILE_ENABLED_SECTIONS "DOXYFILE_USE_COPYBRIEF")
		else()
			set(DOXYFILE_ENABLED_SECTIONS "DOXYFILE_USE_COPYBRIEF DOXYFILE_USE_DETAILS")
		endif()
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doxygen.dox.in ${CMAKE_CURRENT_BINARY_DIR}/doxygen.dox)
		configure_file("${PROJECT_SOURCE_DIR}/coreapi/help/doc/doxygen/custom_design.css.in" custom_design.css @ONLY)
		configure_file(${PROJECT_SOURCE_DIR}/coreapi/help/doc/doxygen/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
		set(DOC_INPUT_FILES ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
			${CMAKE_CURRENT_BINARY_DIR}/doxygen.dox
			${PROVISIONING_DOC_OUTPUT_FILE}
		)
		set(CPP_HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/cpp")
		set(LINPHONE_DOXYGEN_CPP_HTML_DIR ${CPP_HTML_DIR} PARENT_SCOPE)
		add_custom_command(OUTPUT "${CPP_HTML_DIR}/index.html"
			COMMAND ${CMAKE_COMMAND} -E remove -f ${CPP_HTML_DIR}/*
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CPP_HTML_DIR}
			COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
			DEPENDS ${DOC_INPUT_FILES}
			liblinphone-cxx-wrapper
		)
		add_custom_target(liblinphone-cpp-html-doc ALL DEPENDS "${CPP_HTML_DIR}/index.html")
		if(ENABLE_DOC)
			install(DIRECTORY "${CPP_HTML_DIR}"
				DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/liblinphone-${LINPHONE_VERSION}")
		endif()
	endif()
endif()
