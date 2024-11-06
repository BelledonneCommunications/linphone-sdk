# ###########################################################################
# CopyLibs.cmake
# Copyright (C) 2010-2024 Belledonne Communications, Grenoble France
#
# ###########################################################################
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
# ###########################################################################

execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "libs"
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "libs-debug"
)

execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "libs"
	COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "libs-debug"
)

if(WIN32)
file(GLOB_RECURSE _libs "linphone-sdk/java/bin/*.dll")
else()
	file(GLOB_RECURSE _libs "linphone-sdk/java/${CMAKE_INSTALL_LIBDIR}/*.so")
endif()

foreach(_lib ${_libs})
	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${_lib}" "libs/"
		COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${_lib}" "libs-debug/"
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	)
endforeach()

execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "linphone-sdk/java/${CMAKE_INSTALL_LIBDIR}/liblinphone" "libs/liblinphone"
	COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "linphone-sdk/java/${CMAKE_INSTALL_LIBDIR}/mediastreamer" "libs/mediastreamer"
	COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "linphone-sdk/java/${CMAKE_INSTALL_LIBDIR}/liblinphone" "libs-debug/liblinphone"
	COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "linphone-sdk/java/${CMAKE_INSTALL_LIBDIR}/mediastreamer" "libs-debug/mediastreamer"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)
