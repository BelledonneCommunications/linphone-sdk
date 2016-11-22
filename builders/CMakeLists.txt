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

if(LINPHONE_BUILDER_CHECK_LATEST)

	find_package(Git)

	file(GLOB _builders RELATIVE ${CMAKE_CURRENT_LIST_DIR} "${CMAKE_CURRENT_LIST_DIR}/*.cmake")
	list(SORT _builders)
	foreach(_builder ${_builders})
		include("${CMAKE_CURRENT_LIST_DIR}/${_builder}")
		get_filename_component(_builder_name ${_builder} NAME_WE)
		if(EP_${_builder_name}_GIT_TAG_LATEST)
			execute_process(COMMAND "${GIT_EXECUTABLE}" "ls-remote" "${EP_${_builder_name}_GIT_REPOSITORY}" "${EP_${_builder_name}_GIT_TAG_LATEST}"
				OUTPUT_FILE "${LINPHONE_BUILDER_WORK_DIR}/EP_${_builder_name}_git_latest")
			execute_process(COMMAND "head" "-n1" "${LINPHONE_BUILDER_WORK_DIR}/EP_${_builder_name}_git_latest"
				OUTPUT_VARIABLE _builder_latest)
			string(REPLACE "\t" ";" _builder_latest ${_builder_latest})
			list(GET _builder_latest 0 _builder_latest_git_tag)
			if(EP_${_builder_name}_GIT_TAG STREQUAL "${_builder_latest_git_tag}")
				message(STATUS "${_builder_name} is up-to-date")
			else()
				message(STATUS "${_builder_name} is outdated: ${EP_${_builder_name}_GIT_TAG} instead of ${_builder_latest_git_tag} [${EP_${_builder_name}_GIT_TAG_LATEST}]")
			endif()
		else()
			message(STATUS "${_builder_name} is up-to-date")
		endif()
	endforeach()

else()

	if(NOT DEFAULT_VALUE_CMAKE_PLUGIN_LINKING_TYPE)
		set(DEFAULT_VALUE_CMAKE_PLUGIN_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
	endif()

	# Include targets definitions
	include(${CMAKE_CURRENT_LIST_DIR}/../targets/CMakeLists.txt)

	if(LINPHONE_BUILDER_EXTERNAL_BUILDERS_PATH)
		message(STATUS "Including external builders from ${LINPHONE_BUILDER_EXTERNAL_BUILDERS_PATH}")
		include(${LINPHONE_BUILDER_EXTERNAL_BUILDERS_PATH}/CMakeLists.txt)
	endif()

	# Include builder options definitions according to the targets to be built
	include(${CMAKE_CURRENT_LIST_DIR}/../options/common.cmake)
	foreach(_target IN LISTS TARGET_${LINPHONE_BUILDER_TARGET}_DEPENDENCIES)
		if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../options/${_target}.cmake)
			include(${CMAKE_CURRENT_LIST_DIR}/../options/${_target}.cmake)
		endif()
	endforeach()

	# Check for build tools and show an error if one of them is not present
	include(${CMAKE_CURRENT_LIST_DIR}/../cmake/CheckBuildTools.cmake)

	# Check that the chosen target is valid and declare it with its dependencies
	lcb_check_target()
	lcb_declare_targets()

endif()
