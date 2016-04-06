############################################################################
# CMakeLists.txt
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

set(BCTOOLBOX_SOURCE_FILES)
list(APPEND BCTOOLBOX_SOURCE_FILES utils/port.c logging/logging.c containers/list.c containers/map.cc)

if(MBEDTLS_FOUND)
	list(APPEND BCTOOLBOX_SOURCE_FILES crypto/mbedtls.c)
endif()
if(POLARSSL_FOUND)
	if(POLARSSL_VERSION13_OK)
		list(APPEND BCTOOLBOX_SOURCE_FILES crypto/polarssl.c)
	else()
		list(APPEND BCTOOLBOX_SOURCE_FILES crypto/polarssl1.2.c)
	endif()
endif()
if(CUNIT_FOUND)
	set(BCTOOLBOX_TESTER_SOURCE_FILES tester/utils.c)
endif()

if(ENABLE_STATIC)
	add_library(bctoolbox-static STATIC ${BCTOOLBOX_SOURCE_FILES})
	target_link_libraries(bctoolbox-static ${CMAKE_THREAD_LIBS_INIT})
	set_target_properties(bctoolbox-static PROPERTIES OUTPUT_NAME bctoolbox)
	if(CUNIT_FOUND)
		add_library(bctoolbox-tester-static STATIC ${BCTOOLBOX_TESTER_SOURCE_FILES})
		set_target_properties(bctoolbox-tester-static PROPERTIES OUTPUT_NAME bctoolbox-tester)
	endif()
	if(HAVE_LIBRT)
		target_link_libraries(bctoolbox-static rt)
	endif()
endif()
if(ENABLE_SHARED)
	add_library(bctoolbox SHARED ${BCTOOLBOX_SOURCE_FILES})
	target_link_libraries(bctoolbox ${CMAKE_THREAD_LIBS_INIT})
	set_target_properties(bctoolbox PROPERTIES LINKER_LANGUAGE "CXX")
	if(ANDROID)
		set_target_properties(bctoolbox PROPERTIES OUTPUT_NAME "bctoolbox-${CMAKE_SYSTEM_PROCESSOR}")
	else()
		# Do not version shared library on Android
		set_target_properties(bctoolbox PROPERTIES SOVERSION ${BCTOOLBOX_SO_VERSION})
	endif()
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/bctoolbox.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
	if(CUNIT_FOUND)
		add_library(bctoolbox-tester SHARED ${BCTOOLBOX_TESTER_SOURCE_FILES})
		set_target_properties(bctoolbox-tester PROPERTIES LINKER_LANGUAGE "CXX")
		if(ANDROID)
			set_target_properties(bctoolbox-tester PROPERTIES OUTPUT_NAME "bctoolbox-tester-${CMAKE_SYSTEM_PROCESSOR}")
		else()
			# Do not version shared library on Android
			set_target_properties(bctoolbox-tester PROPERTIES SOVERSION ${BCTOOLBOXTESTER_SO_VERSION})
		endif()
		if(MSVC)
			if(CMAKE_BUILD_TYPE STREQUAL "Debug")
				install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/bctoolbox-tester.pdb
					DESTINATION ${CMAKE_INSTALL_BINDIR}
					PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
				)
			endif()
		endif()
	endif()
	if(HAVE_LIBRT)
		target_link_libraries(bctoolbox rt)
	endif()
endif()


if(MBEDTLS_FOUND)
	if(ENABLE_STATIC)
		target_include_directories(bctoolbox-static PUBLIC ${MBEDTLS_INCLUDE_DIRS})
		target_link_libraries(bctoolbox-static ${MBEDTLS_LIBRARIES})
	endif()
	if(ENABLE_SHARED)
		target_include_directories(bctoolbox PUBLIC ${MBEDTLS_INCLUDE_DIRS})
		target_link_libraries(bctoolbox ${MBEDTLS_LIBRARIES})
	endif()
endif()
if(POLARSSL_FOUND)
	if(ENABLE_STATIC)
		target_include_directories(bctoolbox-static PUBLIC ${POLARSSL_INCLUDE_DIRS})
		target_link_libraries(bctoolbox-static ${POLARSSL_LIBRARIES})
	endif()
	if(ENABLE_SHARED)
		target_include_directories(bctoolbox PUBLIC ${POLARSSL_INCLUDE_DIRS})
		target_link_libraries(bctoolbox ${POLARSSL_LIBRARIES})
	endif()
endif()
if(CUNIT_FOUND)
	if(ENABLE_STATIC)
		target_include_directories(bctoolbox-tester-static PUBLIC ${CUNIT_INCLUDE_DIRS})
		target_link_libraries(bctoolbox-tester-static ${CUNIT_LIBRARIES})
	endif()
	if(ENABLE_SHARED)
		target_include_directories(bctoolbox-tester PUBLIC ${CUNIT_INCLUDE_DIRS})
		target_link_libraries(bctoolbox-tester ${CUNIT_LIBRARIES})
	endif()
endif()


if(ENABLE_STATIC)
	install(TARGETS bctoolbox-static EXPORT ${EXPORT_TARGETS_NAME}Targets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		COMPONENT core
	)
	if(CUNIT_FOUND)
		install(TARGETS bctoolbox-tester-static EXPORT ${EXPORT_TARGETS_NAME}Targets
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			COMPONENT tester
		)
	endif()
endif()
if(ENABLE_SHARED)
	install(TARGETS bctoolbox EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		COMPONENT core
	)
	if(CUNIT_FOUND)
		install(TARGETS bctoolbox-tester EXPORT ${EXPORT_TARGETS_NAME}Targets
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			COMPONENT tester
		)
	endif()
endif()
