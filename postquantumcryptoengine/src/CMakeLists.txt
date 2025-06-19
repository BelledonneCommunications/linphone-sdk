############################################################################
# CMakeLists.txt
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
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

set(POSTQUANTUMCRYPTOENGINE_C_SOURCE_FILES
	crypto.c
)

set(POSTQUANTUMCRYPTOENGINE_CXX_SOURCE_FILES
	crypto.cc
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	list(APPEND STRICT_OPTIONS_CXX "-x c++")
endif()

set(POSTQUANTUMCRYPTOENGINE_SOURCE_FILES ${POSTQUANTUMCRYPTOENGINE_CXX_SOURCE_FILES} ${POSTQUANTUMCRYPTOENGINE_C_SOURCE_FILES})

bc_apply_compile_flags(POSTQUANTUMCRYPTOENGINE_C_SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
bc_apply_compile_flags(POSTQUANTUMCRYPTOENGINE_CXX_SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)

add_library(postquantumcryptoengine ${POSTQUANTUMCRYPTOENGINE_SOURCE_FILES} ${POSTQUANTUMCRYPTOENGINE_HEADER_FILES})
target_link_libraries(postquantumcryptoengine PRIVATE ${BCToolbox_TARGET} ${liboqs_TARGET})
target_include_directories(postquantumcryptoengine PUBLIC
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)
if(ANDROID)
	target_link_libraries(postquantumcryptoengine INTERFACE log)
endif()
if(WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	target_link_libraries(postquantumcryptoengine INTERFACE "Winmm" "Ws2_32")
endif()
if(liboqs_USE_BUILD_INTERFACE)
	target_compile_definitions(postquantumcryptoengine PRIVATE "LIBOQS_USE_BUILD_INTERFACE")
endif()

if(BUILD_SHARED_LIBS)
	if(WIN32)
		set_target_properties(postquantumcryptoengine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
		target_compile_definitions(postquantumcryptoengine PRIVATE "-DPQCE_EXPORTS")
		if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")#Fix error on 'vccorlib_lib_should_be_specified_before_msvcrt_lib_to_linker''
			list(APPEND BC_LINK_OPTIONS_DEBUG "/nodefaultlib:vccorlibd" "/nodefaultlib:msvcrtd" "vccorlibd.lib" "msvcrtd.lib")
			list(APPEND BC_LINK_OPTIONS_RELEASE "/nodefaultlib:vccorlib" "/nodefaultlib:msvcrt" "vccorlib.lib" "msvcrt.lib")
			target_link_options(postquantumcryptoengine PRIVATE "$<IF:$<CONFIG:Debug>,${BC_LINK_OPTIONS_DEBUG},${BC_LINK_OPTIONS_RELEASE}>")
			if(MSVC)
				set_target_properties(postquantumcryptoengine PROPERTIES VS_WINRT_COMPONENT TRUE)
			else()
				target_compile_options(postquantumcryptoengine PRIVATE -ZW -EHsc)
			endif()
		endif()
	endif()
	set_target_properties(postquantumcryptoengine PROPERTIES LINKER_LANGUAGE "CXX")
	if(NOT ANDROID)
		# Do not version shared library on Android
		set_target_properties(postquantumcryptoengine PROPERTIES SOVERSION ${POSTQUANTUMCRYPTOENGINE_SO_VERSION})
	endif()
	if(MSVC)
		install(FILES $<TARGET_PDB_FILE:postquantumcryptoengine>
			DESTINATION ${CMAKE_INSTALL_BINDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			CONFIGURATIONS Debug RelWithDebInfo
		)
	endif()
else()
	set_target_properties(postquantumcryptoengine PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

install(TARGETS postquantumcryptoengine EXPORT ${PROJECT_NAME}Targets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FRAMEWORK DESTINATION Frameworks
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	COMPONENT core
)

if(MSVC)
	# Disable "was deprecated" warnings of windows compiler (typically using strcpy_s instead of strcpy and stupid things like this).
	target_compile_options(postquantumcryptoengine PRIVATE "/wd4996")
endif()
