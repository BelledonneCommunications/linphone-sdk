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

set(BCTOOLBOX_C_SOURCE_FILES
	containers/list.c
	logging/logging.c
	parser.c
	utils/port.c
	vconnect.c
	vfs/vfs.c
	vfs/vfs_standard.c
	param_string.c
)

set(BCTOOLBOX_CXX_SOURCE_FILES
	containers/map.cc
	conversion/charconv_encoding.cc
	utils/exception.cc
	utils/regex.cc
	utils/utils.cc
	logging/log-tags.cc
)

set(BCTOOLBOX_PRIVATE_HEADER_FILES
	vfs/vfs_encryption_module.hh
	vfs/vfs_encryption_module_dummy.hh
	vfs/vfs_encryption_module_aes256gcm_sha256.hh
)

if(APPLE)
	set(BCTOOLBOX_OBJC_SOURCE_FILES
		utils/ios_utils.mm
	)
	set(BCTOOLBOX_IOS_OBJC_SOURCE_FILES
		utils/ios_utils_app.mm
	)
endif()
if(WIN32)
	list(APPEND BCTOOLBOX_CXX_SOURCE_FILES utils/win_utils.cc)
endif()

if(ANDROID)
	list(APPEND BCTOOLBOX_CXX_SOURCE_FILES conversion/charconv_android.cc)
elseif(WIN32)
	list(APPEND BCTOOLBOX_CXX_SOURCE_FILES conversion/charconv_windows.cc)
else()
	list(APPEND BCTOOLBOX_CXX_SOURCE_FILES conversion/charconv.cc)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	list(APPEND STRICT_OPTIONS_CXX "-x c++")
endif()
if(WIN32)
	list(APPEND STRICT_OPTIONS_CXX "/EHa")
	add_definitions(-EHa)
endif()
if(MbedTLS_FOUND OR OPENSSL_FOUND)
	list(APPEND BCTOOLBOX_C_SOURCE_FILES crypto/crypto.c)
	list(APPEND BCTOOLBOX_CXX_SOURCE_FILES
		crypto/crypto.cc
		crypto/ecc.cc
		vfs/vfs_encrypted.cc
		vfs/vfs_encryption_module_dummy.cc
		vfs/vfs_encryption_module_aes256gcm_sha256.cc)
endif()
if(OPENSSL_FOUND)
	list(APPEND BCTOOLBOX_C_SOURCE_FILES crypto/openssl.c)
	list(APPEND BCTOOLBOX_CXX_SOURCE_FILES crypto/openssl.cc)
endif()

if(MbedTLS_FOUND)
	list(APPEND BCTOOLBOX_C_SOURCE_FILES crypto/mbedtls.c)
	list(APPEND BCTOOLBOX_CXX_SOURCE_FILES crypto/mbedtls.cc)
endif()

if(ENABLE_TESTS_COMPONENT)
	set(BCTOOLBOX_C_TESTER_SOURCE_FILES tester.c)
endif()

set(BCTOOLBOX_SOURCE_FILES ${BCTOOLBOX_C_SOURCE_FILES} ${BCTOOLBOX_CXX_SOURCE_FILES} ${BCTOOLBOX_OBJC_SOURCE_FILES})
set(BCTOOLBOX_IOS_SOURCE_FILES ${BCTOOLBOX_IOS_OBJC_SOURCE_FILES})
set(BCTOOLBOX_TESTER_SOURCE_FILES ${BCTOOLBOX_C_TESTER_SOURCE_FILES})

bc_apply_compile_flags(BCTOOLBOX_C_SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
bc_apply_compile_flags(BCTOOLBOX_C_TESTER_SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
bc_apply_compile_flags(BCTOOLBOX_CXX_SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)
bc_apply_compile_flags(BCTOOLBOX_OBJC_SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_OBJC)
bc_apply_compile_flags(BCTOOLBOX_IOS_OBJC_SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_OBJC)

# Fix for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105562 and https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105616
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
		AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12
		AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
	set_source_files_properties(utils/regex.cc PROPERTIES COMPILE_OPTIONS "-Wno-error=maybe-uninitialized")
endif()


add_library(bctoolbox ${BCTOOLBOX_SOURCE_FILES} ${BCTOOLBOX_HEADER_FILES} ${BCTOOLBOX_PRIVATE_HEADER_FILES})
set_target_properties(bctoolbox PROPERTIES LINKER_LANGUAGE "CXX")
target_link_libraries(bctoolbox PRIVATE ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(bctoolbox PUBLIC
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)
if(NOT BUILD_SHARED_LIBS)
	set_target_properties(bctoolbox PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
endif()

if(APPLE)
	if(IOS)
		target_link_libraries(bctoolbox PRIVATE "-framework UIKit" "-framework Foundation" "-framework AVFoundation")
	else()
		target_link_libraries(bctoolbox PRIVATE "-framework Foundation" "-framework AVFoundation")
	endif()
	target_link_libraries(bctoolbox PRIVATE Iconv::Iconv)

	set_target_properties(bctoolbox PROPERTIES
		FRAMEWORK TRUE
		MACOSX_FRAMEWORK_IDENTIFIER org.linphone.bctoolbox
		MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_SOURCE_DIR}/build/osx/Info.plist.in"
		PUBLIC_HEADER "${BCTOOLBOX_HEADER_FILES}"
	)
endif()

if(WIN32)
	set_target_properties(bctoolbox PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
	if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
		target_link_libraries(bctoolbox PRIVATE "Winmm" "Ws2_32")
	endif()
	target_compile_definitions(bctoolbox PRIVATE "BCTBX_EXPORTS")
	if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore") # Fix error on 'vccorlib_lib_should_be_specified_before_msvcrt_lib_to_linker''
		list(APPEND BC_LINK_OPTIONS_DEBUG "/nodefaultlib:vccorlibd" "/nodefaultlib:msvcrtd" "vccorlibd.lib" "msvcrtd.lib")
		list(APPEND BC_LINK_OPTIONS_RELEASE "/nodefaultlib:vccorlib" "/nodefaultlib:msvcrt" "vccorlib.lib" "msvcrt.lib")
		target_link_options(bctoolbox PUBLIC "$<IF:$<CONFIG:Debug>,${BC_LINK_OPTIONS_DEBUG},${BC_LINK_OPTIONS_RELEASE}>")
		if(MSVC)
			set_target_properties(bctoolbox PROPERTIES VS_WINRT_COMPONENT TRUE)
		else()
			target_compile_options(bctoolbox PRIVATE -ZW -EHsc)
		endif()
	endif()
endif()
if(MSVC AND BUILD_SHARED_LIBS)
	install(FILES $<TARGET_PDB_FILE:bctoolbox>
		DESTINATION ${CMAKE_INSTALL_BINDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		CONFIGURATIONS Debug RelWithDebInfo
	)
endif()

if(HAVE_LIBRT)
	target_link_libraries(bctoolbox PRIVATE rt)
endif()

if(HAVE_LIBDL)
	target_link_libraries(bctoolbox PRIVATE dl)
endif()

if(ANDROID)
	target_link_libraries(bctoolbox PRIVATE log)
else()
	# Do not version shared library on Android
	set_target_properties(bctoolbox PROPERTIES SOVERSION ${BCTOOLBOX_SO_VERSION})	
endif()


if(ENABLE_TESTS_COMPONENT)
	add_library(bctoolbox-tester ${BCTOOLBOX_TESTER_SOURCE_FILES})
	set_target_properties(bctoolbox-tester PROPERTIES LINKER_LANGUAGE "CXX")
	target_link_libraries(bctoolbox-tester PUBLIC bctoolbox PRIVATE ${BCUnit_TARGET})
	target_include_directories(bctoolbox-tester PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	)
	if(APPLE)
		set_target_properties(bctoolbox-tester PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER com.belledonne-communications.bctoolbox-tester
			MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_SOURCE_DIR}/build/osx/Info.plist.in"
			PUBLIC_HEADER "${BCTOOLBOX_HEADER_FILES}"
		)
	endif()
	if(WIN32)
		target_compile_definitions(bctoolbox-tester PRIVATE "BCTBX_EXPORTS")
	endif()
	if(NOT ANDROID)
		# Do not version shared library on Android
		set_target_properties(bctoolbox-tester PROPERTIES SOVERSION ${BCTOOLBOXTESTER_SO_VERSION})
	endif()
	if(MSVC AND BUILD_SHARED_LIBS)
		install(FILES $<TARGET_PDB_FILE:bctoolbox-tester>
			DESTINATION ${CMAKE_INSTALL_BINDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			CONFIGURATIONS Debug RelWithDebInfo
		)
	endif()
endif()

if(APPLE AND IOS)
	add_library(bctoolbox-ios SHARED ${BCTOOLBOX_IOS_SOURCE_FILES})
	target_link_libraries(bctoolbox-ios PRIVATE bctoolbox)
	target_link_libraries(bctoolbox-ios PRIVATE "-framework UIKit" "-framework Foundation" "-framework CoreFoundation")
	set_target_properties(bctoolbox-ios PROPERTIES
		FRAMEWORK TRUE
		MACOSX_FRAMEWORK_IDENTIFIER org.linphone.bctoolbox-ios
		MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_SOURCE_DIR}/build/osx/Info.plist.in"
	)
endif()

if(MbedTLS_FOUND)
	target_link_libraries(bctoolbox PRIVATE ${MbedTLS_TARGET} ${MbedX509_TARGET} ${MbedCrypto_TARGET})
endif()
if(Decaf_FOUND)
	target_link_libraries(bctoolbox PRIVATE ${Decaf_TARGET})
endif()
if (OPENSSL_FOUND)
	target_link_libraries(bctoolbox PRIVATE OpenSSL::SSL)
endif ()

install(TARGETS bctoolbox EXPORT ${PROJECT_NAME}Targets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FRAMEWORK DESTINATION Frameworks
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	COMPONENT core
)
if(ENABLE_TESTS_COMPONENT)
	install(TARGETS bctoolbox-tester EXPORT ${PROJECT_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		FRAMEWORK DESTINATION Frameworks
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		COMPONENT tester
	)
endif()

if(APPLE AND IOS)
	install(TARGETS bctoolbox-ios EXPORT ${PROJECT_NAME}Targets
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
	target_compile_options(bctoolbox PRIVATE "/wd4996")
	if(ENABLE_TESTS_COMPONENT)
		target_compile_options(bctoolbox-tester PRIVATE "/wd4996")
	endif()
endif()
