############################################################################
# additional_steps.cmake
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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

# Packaging
if(ENABLE_PACKAGING)
	get_cmake_property(_varnames VARIABLES)
	set(ENABLE_VARIABLES )
	foreach(_varname ${_varnames})
		if(_varname MATCHES "^ENABLE_.*")
			list(APPEND ENABLE_VARIABLES -D${_varname}=${${_varname}})
	    endif()
	endforeach()

	linphone_builder_apply_flags()
	linphone_builder_set_ep_directories(package)
	linphone_builder_expand_external_project_vars()
	ExternalProject_Add(TARGET_linphone_package
		DEPENDS TARGET_linphone_builder
		TMP_DIR ${ep_tmp}
		BINARY_DIR ${ep_build}
		SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/package"
		DOWNLOAD_COMMAND ""
		CMAKE_GENERATOR ${CMAKE_GENERATOR}
		CMAKE_ARGS ${LINPHONE_BUILDER_EP_ARGS} -DLINPHONE_OUTPUT_DIR=${CMAKE_INSTALL_PREFIX} -DLINPHONE_SOURCE_DIR=${EP_linphone_SOURCE_DIR} ${ENABLE_VARIABLES} -DRASPBERRY_VERSION=${RASPBERRY_VERSION}
	)
endif()
