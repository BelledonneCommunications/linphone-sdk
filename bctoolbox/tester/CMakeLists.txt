############################################################################
# CMakeLists.txt
# Copyright (C) 2020  Belledonne Communications, Grenoble France
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

if(ENABLE_UNIT_TESTS AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore" AND NOT ANDROID)
	set(TESTER_SOURCES
		bctoolbox_tester.c
		bctoolbox_tester.h
		containers.cc
		logger.cc
		port.c
		parser.c
		param_string.c
		vfs.c
	)
	if(MbedTLS_FOUND OR OPENSSL_FOUND)
		list(APPEND TESTER_SOURCES
			crypto.cc
			encrypted_vfs.cc)
	endif()
	if(APPLE)
		list(APPEND TESTER_SOURCES ios_utils.cc)
	endif()

    bc_apply_compile_flags(TESTER_SOURCE STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
	add_executable(bctoolbox-tester-exe ${TESTER_SOURCES})
	set_target_properties(bctoolbox-tester-exe PROPERTIES OUTPUT_NAME bctoolbox-tester)
	target_link_libraries(bctoolbox-tester-exe PRIVATE bctoolbox bctoolbox-tester)
	if(MbedTLS_FOUND)
		target_link_libraries(bctoolbox-tester-exe PRIVATE ${MbedTLS_TARGET})
	endif()
	set_target_properties(bctoolbox-tester-exe PROPERTIES XCODE_ATTRIBUTE_WARNING_CFLAGS "")
	add_test(NAME bctoolbox_tester COMMAND bctoolbox_tester --verbose)
	if(NOT IOS)
		install(TARGETS bctoolbox-tester-exe
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)
	endif()
endif()
