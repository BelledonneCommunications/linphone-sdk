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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

set(BCTOOLBOX_C_SOURCE_FILES
	containers/list.c
	logging/logging.c
	utils/port.c
	vfs.c
)

set(BCTOOLBOX_CXX_SOURCE_FILES containers/map.cc)

if(HAVE_EXECINFO) 
	list(APPEND BCTOOLBOX_CXX_SOURCE_FILES utils/exception.cc)
endif()

if(MBEDTLS_FOUND)
	list(APPEND BCTOOLBOX_C_SOURCE_FILES crypto/mbedtls.c)
endif()
if(POLARSSL_FOUND)
	if(POLARSSL_VERSION13_OK)
		list(APPEND BCTOOLBOX_C_SOURCE_FILES crypto/polarssl.c)
	else()
		list(APPEND BCTOOLBOX_C_SOURCE_FILES crypto/polarssl1.2.c)
	endif()
endif()
if(BCUNIT_FOUND)
	set(BCTOOLBOX_C_TESTER_SOURCE_FILES tester.c)
endif()

set(BCTOOLBOX_SOURCE_FILES ${BCTOOLBOX_C_SOURCE_FILES} ${BCTOOLBOX_CXX_SOURCE_FILES})
set(BCTOOLBOX_TESTER_SOURCE_FILES ${BCTOOLBOX_C_TESTER_SOURCE_FILES})

apply_compile_flags(BCTOOLBOX_C_SOURCE_FILES_C "CPP" "C")
apply_compile_flags(BCTOOLBOX_C_TESTER_SOURCE_FILES_C "CPP" "C")
apply_compile_flags(BCTOOLBOX_CXX_SOURCE_FILES_C "CPP" "CXX")

if(ENABLE_STATIC)
	add_library(bctoolbox-static STATIC ${BCTOOLBOX_SOURCE_FILES} ${BCTOOLBOX_HEADER_FILES})
	target_link_libraries(bctoolbox-static ${CMAKE_THREAD_LIBS_INIT})
	if(WIN32)
		target_compile_definitions(bctoolbox-static PUBLIC "-DBCTBX_STATIC")
	endif()
	set_target_properties(bctoolbox-static PROPERTIES LINKER_LANGUAGE "CXX")
	set_target_properties(bctoolbox-static PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
	set_target_properties(bctoolbox-static PROPERTIES OUTPUT_NAME bctoolbox)
	if(HAVE_LIBRT)
		target_link_libraries(bctoolbox-static rt)
	endif()
	if(HAVE_LIBDL)
		target_link_libraries(bctoolbox-static dl)
	endif()
	if(BCUNIT_FOUND)
		add_library(bctoolbox-tester-static STATIC ${BCTOOLBOX_TESTER_SOURCE_FILES})
		set_target_properties(bctoolbox-tester-static PROPERTIES OUTPUT_NAME bctoolbox-tester)
		target_link_libraries(bctoolbox-tester-static bctoolbox-static)
		if(WIN32)
			target_compile_definitions(bctoolbox-tester-static PUBLIC "-DBCTBX_STATIC")
		endif()
	endif()
endif()
if(ENABLE_SHARED)
	add_library(bctoolbox SHARED ${BCTOOLBOX_SOURCE_FILES} ${BCTOOLBOX_HEADER_FILES})
	target_link_libraries(bctoolbox PRIVATE ${CMAKE_THREAD_LIBS_INIT})
	if(WIN32)
		target_link_libraries(bctoolbox PRIVATE "Winmm" "Ws2_32")
		target_compile_definitions(bctoolbox PRIVATE "-DBCTBX_EXPORTS")
	endif()
	set_target_properties(bctoolbox PROPERTIES LINKER_LANGUAGE "CXX")
	set_target_properties(bctoolbox PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
	if(ANDROID)
		set_target_properties(bctoolbox PROPERTIES OUTPUT_NAME "bctoolbox-${NDK_ARCHITECTURE}")
	else()
		# Do not version shared library on Android
		set_target_properties(bctoolbox PROPERTIES SOVERSION ${BCTOOLBOX_SO_VERSION})
	endif()
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bctoolbox.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
	if(HAVE_LIBRT)
		target_link_libraries(bctoolbox PRIVATE rt)
	endif()
	if(HAVE_LIBDL)
		target_link_libraries(bctoolbox PRIVATE dl)
	endif()
	if(BCUNIT_FOUND)
		add_library(bctoolbox-tester SHARED ${BCTOOLBOX_TESTER_SOURCE_FILES})
		set_target_properties(bctoolbox-tester PROPERTIES LINKER_LANGUAGE "CXX")
		target_link_libraries(bctoolbox-tester PRIVATE bctoolbox)
		if(WIN32)
			target_compile_definitions(bctoolbox-tester PRIVATE "-DBCTBX_EXPORTS")
		endif()
		if(ANDROID)
			set_target_properties(bctoolbox-tester PROPERTIES OUTPUT_NAME "bctoolbox-tester-${NDK_ARCHITECTURE}")
		else()
			# Do not version shared library on Android
			set_target_properties(bctoolbox-tester PROPERTIES SOVERSION ${BCTOOLBOXTESTER_SO_VERSION})
		endif()
		if(MSVC)
			if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
				install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bctoolbox-tester.pdb
					DESTINATION ${CMAKE_INSTALL_BINDIR}
					PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
				)
			endif()
		endif()
	endif()
endif()


if(MBEDTLS_FOUND)
	if(ENABLE_STATIC)
		target_include_directories(bctoolbox-static PUBLIC ${MBEDTLS_INCLUDE_DIRS})
		target_link_libraries(bctoolbox-static ${MBEDTLS_LIBRARIES})
	endif()
	if(ENABLE_SHARED)
		target_include_directories(bctoolbox PUBLIC ${MBEDTLS_INCLUDE_DIRS})
		target_link_libraries(bctoolbox PRIVATE ${MBEDTLS_LIBRARIES})
	endif()
endif()
if(POLARSSL_FOUND)
	if(ENABLE_STATIC)
		target_include_directories(bctoolbox-static PUBLIC ${POLARSSL_INCLUDE_DIRS})
		target_link_libraries(bctoolbox-static ${POLARSSL_LIBRARIES})
	endif()
	if(ENABLE_SHARED)
		target_include_directories(bctoolbox PUBLIC ${POLARSSL_INCLUDE_DIRS})
		target_link_libraries(bctoolbox PRIVATE ${POLARSSL_LIBRARIES})
	endif()
endif()
if(BCUNIT_FOUND)
	if(ENABLE_STATIC)
		target_include_directories(bctoolbox-tester-static PUBLIC ${BCUNIT_INCLUDE_DIRS})
		target_link_libraries(bctoolbox-tester-static bctoolbox-static ${BCUNIT_LIBRARIES})
	endif()
	if(ENABLE_SHARED)
		target_include_directories(bctoolbox-tester PUBLIC ${BCUNIT_INCLUDE_DIRS})
		target_link_libraries(bctoolbox-tester PRIVATE bctoolbox ${BCUNIT_LIBRARIES})
	endif()
endif()


if(ENABLE_STATIC)
	install(TARGETS bctoolbox-static EXPORT ${EXPORT_TARGETS_NAME}Targets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		COMPONENT core
	)
	if(BCUNIT_FOUND)
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
	if(BCUNIT_FOUND)
		install(TARGETS bctoolbox-tester EXPORT ${EXPORT_TARGETS_NAME}Targets
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			COMPONENT tester
		)
	endif()
endif()
