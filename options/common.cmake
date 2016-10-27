############################################################################
# common.cmake
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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

# Helper macros to help define options

# This macro can be used to add an option. Give it option name and description,
# default value and optionally dependent predicate and value
macro(add_dependent_option NAME DESCRIPTION DEFAULT_VALUE CONDITION CONDITION_VALUE)
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	cmake_dependent_option(ENABLE_${UPPERCASE_NAME} "${DESCRIPTION}" "${DEFAULT_VALUE}" "${CONDITION}" "${CONDITION_VALUE}")
	add_feature_info("${NAME}" "ENABLE_${UPPERCASE_NAME}" "${DESCRIPTION}")
endmacro()

macro(add_strict_dependent_option NAME DESCRIPTION DEFAULT_VALUE CONDITION CONDITION_VALUE STRICT_CONDITION ERROR_MSG)
	add_dependent_option("${NAME}" "${DESCRIPTION}" "${DEFAULT_VALUE}" "${CONDITION}" "${CONDITION_VALUE}")
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	if(${ENABLE_${UPPERCASE_NAME}} AND NOT ${STRICT_CONDITION})
		message(FATAL_ERROR "Trying to enable ${NAME} but ${ERROR_MSG}")
	endif()
endmacro()

macro(add_option NAME DESCRIPTION DEFAULT_VALUE)
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	option(ENABLE_${UPPERCASE_NAME} "Enable ${NAME}: ${DESCRIPTION}" "${DEFAULT_VALUE}")
	add_feature_info("${NAME}" "ENABLE_${UPPERCASE_NAME}" "${DESCRIPTION}")
endmacro()


# Common build options

option(ENABLE_UNIT_TESTS "Enable unit tests support with BCUnit library." "${DEFAULT_VALUE_ENABLE_UNIT_TESTS}")
add_feature_info("Unit tests" ENABLE_UNIT_TESTS "Build unit tests programs for belle-sip, mediastreamer2 and linphone.")
option(ENABLE_DEBUG_LOGS "Enable debug level logs in libinphone and mediastreamer2." NO)

option(ENABLE_PACKAGING "Enable packaging" "${DEFAULT_VALUE_ENABLE_PACKAGING}")

option(ENABLE_DOC "Enable documentation generation with Doxygen." YES)
add_feature_info("Documentation" ENABLE_DOC "Enable documentation generation with Doxygen.")

option(ENABLE_TOOLS "Enable tools binary compilation." YES)
add_feature_info("Tools" ENABLE_DOC "Enable tools binary compilation.")
