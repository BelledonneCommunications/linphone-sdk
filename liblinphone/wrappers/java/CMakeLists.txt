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

set(jni_sources "${CMAKE_CURRENT_BINARY_DIR}/src/linphone_jni.cc")

# Remove everything after the last point in LINPHONE_VERSION (4.5.0 --> 4.5) to create STRIPPED_LINPHONE_VERSION
string(REGEX REPLACE "\\.[^.]*$" "" STRIPPED_LINPHONE_VERSION ${LINPHONE_VERSION})

if(ANDROID_BUNDLED)
	set(_wrapper_platform "bundled-android")
elseif(ANDROID)
	set(_wrapper_platform "android")
else()
	set(_wrapper_platform "desktop")
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
	set(SED_BACKSLASH "\\\\\\")
	set(SED_ESCAPE_CHARACTER "\^")
	set(SED_BINARY_MODE "-b")
else()
	set(SED_BACKSLASH "\\\\")
	set(SED_ESCAPE_CHARACTER "")
	set(SED_BINARY_MODE "")
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
	set(SED_IN_PLACE_EDIT_ARG "-i ''")
else()
	set(SED_IN_PLACE_EDIT_ARG "-i")
endif()

configure_file("${PROJECT_SOURCE_DIR}/coreapi/help/doc/doxygen/simple_provisioning_configuration_key.dox.in" "${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache.raw")
configure_file("${PROJECT_SOURCE_DIR}/wrappers/java/overview.mustache.in" "${CMAKE_CURRENT_BINARY_DIR}/overview.mustache.in")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/overview.mustache"
	COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_BINARY_DIR}/overview.mustache
	COMMAND sed -r ${SED_BINARY_MODE} 's/\#Linphone\(\[A-Za-z\]+\)/${SED_ESCAPE_CHARACTER}\<a href\=\"org\\\/linphone\\\/core\\\/\\1.html\"${SED_ESCAPE_CHARACTER}\>Linphone\.\\1${SED_ESCAPE_CHARACTER}\<\\\/a${SED_ESCAPE_CHARACTER}\>/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache.raw > ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/@//' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\/\\\*\\\*//' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\\*\\///' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND sed ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\/\\\*//g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND sed ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\\*\\///g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND sed ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\[\[:space:\]\]*\\\*\[\[:space:\]\]*//g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\[\[:space:\]\]*${SED_BACKSLASH}defgroup\[\[:space:\]\]+\[A-Za-z\\_\]+\[\[:space:\]\]+\(\[A-Za-z\[:space:\]\]+\)/${SED_ESCAPE_CHARACTER}\<h1${SED_ESCAPE_CHARACTER}\>\\1${SED_ESCAPE_CHARACTER}\<\\/h1${SED_ESCAPE_CHARACTER}\>/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/Table ${SED_BACKSLASH}ref \[A-Za-z\\_\]\+ \"\[A-Za-z\[:space:\]\\_\]\+\"/The table below/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\_\\\#\\\#\(\[\[:alnum:\]\]\)/\\U\\1/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/\\_\\\#\(\[\[:alnum:\]\]\)/\\U\\1/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND sed -r ${SED_BINARY_MODE} ${SED_IN_PLACE_EDIT_ARG} 's/linphone::\(\[\[:alnum:\]\]+\)::\(\[\[:alnum:\]\]+\)\(\\\(\\\)\)/${SED_ESCAPE_CHARACTER}\<a href\=\"org\\\/linphone\\\/core\\\/\\1.html\#\\2--\"${SED_ESCAPE_CHARACTER}\>\\1\.\\2\\3${SED_ESCAPE_CHARACTER}\<\\\/a${SED_ESCAPE_CHARACTER}\>/g' ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache
	COMMAND head -n -2 ${CMAKE_CURRENT_BINARY_DIR}/overview.mustache.in >> ${CMAKE_CURRENT_BINARY_DIR}/overview.mustache
	COMMAND cat ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache >> ${CMAKE_CURRENT_BINARY_DIR}/overview.mustache
	COMMAND tail -2 ${CMAKE_CURRENT_BINARY_DIR}/overview.mustache.in >> ${CMAKE_CURRENT_BINARY_DIR}/overview.mustache
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/provisioning_configuration_key.mustache.raw
	${CMAKE_CURRENT_BINARY_DIR}/overview.mustache.in
	liblinphone-doc
)

add_custom_command(OUTPUT "${jni_sources}"
	COMMAND ${CMAKE_COMMAND} -E remove -f java/org/linphone/core/* src/*
	COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/genwrapper.py" "${PROJECT_BINARY_DIR}/coreapi/help/doc/doxygen/xml" "-o" "${CMAKE_CURRENT_BINARY_DIR}" "-v" "${STRIPPED_LINPHONE_VERSION}" "-d" "${LINPHONESDK_STATE}" "-P" "${_wrapper_platform}"
	DEPENDS ${PROJECT_SOURCE_DIR}/tools/genapixml.py ${LINPHONE_HEADER_FILES}
	${PROJECT_SOURCE_DIR}/tools/metaname.py
	${PROJECT_SOURCE_DIR}/tools/metadoc.py
	${PROJECT_SOURCE_DIR}/tools/abstractapi.py
	genwrapper.py
	java_class.mustache
	java_enum.mustache
	java_interface_stub.mustache
	java_interface.mustache
	jni.mustache
	overview.mustache
	proguard.mustache
	liblinphone-doc
	COMMENT "Generating java wrapper"
)

add_custom_target(linphonej ALL DEPENDS "${jni_sources}")

set(LINPHONE_JNI_SOURCES "${jni_sources}" PARENT_SCOPE)

install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/java" DESTINATION "${CMAKE_INSTALL_DATADIR}/linphonej/")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/src/proguard.txt" DESTINATION "${CMAKE_INSTALL_DATADIR}/linphonej/")

#install(DIRECTORY classes/ DESTINATION "${CMAKE_INSTALL_DATADIR}/linphonej/java/org/linphone/core/")

if(ENABLE_JAVA_WRAPPER_TESTER)
	# Install the tests project into the install data dir
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/WrapperTests" DESTINATION "${CMAKE_INSTALL_DATADIR}/linphonej/")
endif()
