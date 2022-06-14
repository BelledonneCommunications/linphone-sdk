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

if(ENABLE_STATIC)
	add_library(postquantumcryptoengine STATIC ${POSTQUANTUMCRYPTOENGINE_SOURCE_FILES} ${POSTQUANTUMCRYPTOENGINE_HEADER_FILES})
	set_target_properties(postquantumcryptoengine PROPERTIES POSITION_INDEPENDENT_CODE On)
	if(WIN32)
		target_link_libraries(postquantumcryptoengine INTERFACE "Winmm" "Ws2_32")
	endif()
	#set_target_properties(postquantumcryptoengine PROPERTIES LINKER_LANGUAGE "CXX")
	set_target_properties(postquantumcryptoengine PROPERTIES OUTPUT_NAME postquantumcryptoengine)
	if(ANDROID)
		target_link_libraries(postquantumcryptoengine INTERFACE log)
	endif()
	target_include_directories(postquantumcryptoengine PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	)
	target_link_libraries(postquantumcryptoengine PRIVATE bctoolbox)
	target_link_libraries(postquantumcryptoengine PRIVATE OQS::oqs)
endif()
if(ENABLE_SHARED)
	add_library(postquantumcryptoengine SHARED ${POSTQUANTUMCRYPTOENGINE_SOURCE_FILES} ${POSTQUANTUMCRYPTOENGINE_HEADER_FILES})
	if(APPLE)
		if(IOS)
			set(MIN_OS ${LINPHONE_IOS_DEPLOYMENT_TARGET})
		else()
			set(MIN_OS ${CMAKE_OSX_DEPLOYMENT_TARGET})
		endif()
	endif()
	if(WIN32)
		set_target_properties(postquantumcryptoengine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
		if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
			target_link_libraries(postquantumcryptoengine PRIVATE "Winmm" "Ws2_32")
		endif()
		target_compile_definitions(postquantumcryptoengine PRIVATE "-DPQCE_EXPORTS")
		if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")#Fix error on 'vccorlib_lib_should_be_specified_before_msvcrt_lib_to_linker''
			get_target_property(BC_LINK_FLAGS postquantumcryptoengine LINK_FLAGS)
			if(NOT BC_LINK_FLAGS)
				set(BC_LINK_FLAGS "")
			endif()
			if(CMAKE_BUILD_TYPE STREQUAL "Debug")
				list(APPEND BC_LINK_FLAGS "/nodefaultlib:vccorlibd /nodefaultlib:msvcrtd vccorlibd.lib msvcrtd.lib")
			else()
				list(APPEND BC_LINK_FLAGS "/nodefaultlib:vccorlib /nodefaultlib:msvcrt vccorlib.lib msvcrt.lib")
			endif()
			set_target_properties(postquantumcryptoengine PROPERTIES VS_WINRT_EXTENSIONS TRUE)
			add_definitions(-ZW -EHsc)
			set_target_properties(postquantumcryptoengine PROPERTIES LINK_FLAGS "${BC_LINK_FLAGS}")
		endif()
	endif()
	set_target_properties(postquantumcryptoengine PROPERTIES LINKER_LANGUAGE "CXX")
	if(NOT ANDROID)
		# Do not version shared library on Android
		set_target_properties(postquantumcryptoengine PROPERTIES SOVERSION ${POSTQUANTUMCRYPTOENGINE_SO_VERSION})
	endif()
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES $<TARGET_PDB_FILE:postquantumcryptoengine>
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
	if(ANDROID)
		target_link_libraries(postquantumcryptoengine PRIVATE log)
	endif()
	target_include_directories(postquantumcryptoengine PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	)
	target_link_libraries(postquantumcryptoengine PRIVATE bctoolbox)
	target_link_libraries(postquantumcryptoengine PRIVATE OQS::oqs)
endif()

if(ENABLE_STATIC)
	install(TARGETS postquantumcryptoengine EXPORT ${EXPORT_TARGETS_NAME}Targets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		COMPONENT core
	)
endif()

if(ENABLE_SHARED)
	install(TARGETS postquantumcryptoengine EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		FRAMEWORK DESTINATION Frameworks
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		COMPONENT core
	)
endif()

if(MSVC)
	# Disable "was deprecated" warnings of windows compiler (typically using strcpy_s instead of strcpy and stupid things like this).
	target_compile_options(postquantumcryptoengine PRIVATE "/wd4996")
endif()
