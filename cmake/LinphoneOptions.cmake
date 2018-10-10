############################################################################
# LinphoneOptions.cmake
# Copyright (C) 2010-2018 Belledonne Communications, Grenoble France
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

include(CMakeDependentOption)
include(FeatureSummary)


# This macro can be used to add an option. Give it option name and description,
# default value and optionally dependent predicate and value
macro(lcb_add_dependent_option NAME DESCRIPTION DEFAULT_VALUE CONDITION CONDITION_VALUE)
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	cmake_dependent_option(ENABLE_${UPPERCASE_NAME} "${DESCRIPTION}" "${DEFAULT_VALUE}" "${CONDITION}" "${CONDITION_VALUE}")
	add_feature_info("${NAME}" "ENABLE_${UPPERCASE_NAME}" "${DESCRIPTION}")
endmacro()

macro(lcb_add_strict_dependent_option NAME DESCRIPTION DEFAULT_VALUE CONDITION CONDITION_VALUE STRICT_CONDITION ERROR_MSG)
	lcb_add_dependent_option("${NAME}" "${DESCRIPTION}" "${DEFAULT_VALUE}" "${CONDITION}" "${CONDITION_VALUE}")
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	if(${ENABLE_${UPPERCASE_NAME}} AND NOT ${STRICT_CONDITION})
		message(FATAL_ERROR "Trying to enable ${NAME} but ${ERROR_MSG}")
	endif()
endmacro()

macro(lcb_add_option NAME DESCRIPTION DEFAULT_VALUE)
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	option(ENABLE_${UPPERCASE_NAME} "Enable ${NAME}: ${DESCRIPTION}" "${DEFAULT_VALUE}")
	add_feature_info("${NAME}" "ENABLE_${UPPERCASE_NAME}" "${DESCRIPTION}")
endmacro()
