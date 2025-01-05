############################################################################
# ExternalDependencies.cmake
# Copyright (C) 2010-2023  Belledonne Communications, Grenoble France
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

include(ExternalProject)
include(GNUInstallDirs)


############################################################################
# Define options to choose between building or finding external
# dependencies on the system
############################################################################

cmake_dependent_option(BUILD_AOM "Build aom library source code from submodule instead of searching it in system libraries" ON "ENABLE_AV1" OFF)

cmake_dependent_option(BUILD_BV16 "Build bv16 library source code from submodule instead of searching it in system libraries." ON "ENABLE_BV16" OFF)
cmake_dependent_option(BUILD_BV16_SHARED_LIBS "Choose to build shared or static bv16 library." ${BUILD_SHARED_LIBS} "BUILD_BV16" OFF)

cmake_dependent_option(BUILD_CODEC2 "Build codec2 library source code from submodule instead of searching it in system libraries." ON "ENABLE_CODEC2" OFF)
cmake_dependent_option(BUILD_CODEC2_SHARED_LIBS "Choose to build shared or static codec2 library." ${BUILD_SHARED_LIBS} "BUILD_CODEC2" OFF)

cmake_dependent_option(BUILD_DAV1D "Build dav1d library source code from submodule instead of searching it in system libraries" ON "ENABLE_AV1" OFF)
cmake_dependent_option(BUILD_DAV1D_SHARED_LIBS "Choose to build shared or static dav1d library." OFF "BUILD_DAV1D" OFF)

cmake_dependent_option(BUILD_DECAF "Build decaf library source code from submodule instead of searching it in system libraries." ON "ENABLE_DECAF" OFF)
cmake_dependent_option(BUILD_DECAF_SHARED_LIBS "Choose to build shared or static decaf library." ${BUILD_SHARED_LIBS} "BUILD_DECAF" OFF)

cmake_dependent_option(BUILD_FFMPEG "Build ffmpeg library source code from submodule instead of searching it in system libraries." ON "ENABLE_FFMPEG" OFF)

cmake_dependent_option(BUILD_GSM "Build gsm library source code from submodule instead of searching it in system libraries." ON "ENABLE_GSM" OFF)
cmake_dependent_option(BUILD_GSM_SHARED_LIBS "Choose to build shared or static gsm library." ${BUILD_SHARED_LIBS} "BUILD_GSM" OFF)

cmake_dependent_option(BUILD_JSONCPP "Build jsoncpp library source code from submodule instead of searching it in system libraries." ON "ENABLE_JSONCPP" OFF)
cmake_dependent_option(BUILD_JSONCPP_SHARED_LIBS "Choose to build shared or static jsoncpp library." ${BUILD_SHARED_LIBS} "BUILD_JSONCPP" OFF)

cmake_dependent_option(BUILD_LIBJPEGTURBO "Build jpeg-turbo library source code from submodule instead of searching it in system libraries." ON "ENABLE_JPEG" OFF)
cmake_dependent_option(BUILD_LIBJPEGTURBO_SHARED_LIBS "Choose to build shared or static turbojpeg library." ${BUILD_SHARED_LIBS} "BUILD_LIBJPEGTURBO" OFF)

cmake_dependent_option(BUILD_LIBOQS "Build liboqs library source code from submodule instead of searching it in system libraries." ON "ENABLE_PQCRYPTO" OFF)
cmake_dependent_option(BUILD_LIBOQS_SHARED_LIBS "Choose to build shared or static liboqs library." ${BUILD_SHARED_LIBS} "BUILD_LIBOQS" OFF)

cmake_dependent_option(BUILD_LIBSRTP2 "Build SRTP2 library source code from submodule instead of searching it in system libraries." ON "ENABLE_SRTP" OFF)
cmake_dependent_option(BUILD_LIBSRTP2_SHARED_LIBS "Choose to build shared or static SRTP2 library." ${BUILD_SHARED_LIBS} "BUILD_LIBSRTP2" OFF)

cmake_dependent_option(BUILD_LIBVPX "Build vpx library source code from submodule instead of searching it in system libraries." ON "ENABLE_VPX" OFF)

option(BUILD_LIBXML2 "Build xml2 library source code from submodule instead of searching it in system libraries." ON)
cmake_dependent_option(BUILD_LIBXML2_SHARED_LIBS "Choose to build shared or static xml2 library." ${BUILD_SHARED_LIBS} "BUILD_LIBXML2" OFF)

cmake_dependent_option(BUILD_LIBYUV "Build libyuv library source code from submodule instead of searching it in system libraries." ON "ENABLE_LIBYUV" OFF)
cmake_dependent_option(BUILD_LIBYUV_SHARED_LIBS "Choose to build shared or static libyuv library." ${BUILD_SHARED_LIBS} "BUILD_LIBYUV" OFF)

cmake_dependent_option(BUILD_MBEDTLS "Build mbedtls library source code from submodule instead of searching it in system libraries." ON "ENABLE_MBEDTLS" OFF)
cmake_dependent_option(BUILD_MBEDTLS_SHARED_LIBS "Choose to build shared or static mbedtls library." ${BUILD_SHARED_LIBS} "BUILD_MBEDTLS" OFF)
cmake_dependent_option(BUILD_MBEDTLS_WITH_FATAL_WARNINGS "Allow configuration of MBEDLS_FATAL_WARNINGS option." OFF "BUILD_MBEDTLS" OFF)

cmake_dependent_option(BUILD_OPENSSL "Build openssl library source code from submodule instead of searching it in system libraries." ON "ENABLE_OPENSSL" OFF)
cmake_dependent_option(BUILD_OPENSSL_SHARED_LIBS "Choose to build shared or static openssl library." ${BUILD_SHARED_LIBS} "BUILD_OPENSSL" OFF)

cmake_dependent_option(BUILD_OPENCORE_AMR "Build opencore-amr library source code from submodule instead of searching it in system libraries." ON "ENABLE_AMR" OFF)
cmake_dependent_option(BUILD_OPENCORE_AMR_SHARED_LIBS "Choose to build shared or static opencore-amr library." ${BUILD_SHARED_LIBS} "BUILD_OPENCORE_AMR" OFF)

cmake_dependent_option(BUILD_OPENH264 "Build openh264 library source code from submodule instead of searching it in system libraries." ON "ENABLE_OPENH264" OFF)

cmake_dependent_option(BUILD_OPENLDAP "Build openldap library source code from submodule instead of searching it in system libraries." ON "ENABLE_LDAP" OFF)
cmake_dependent_option(BUILD_OPENLDAP_SHARED_LIBS "Choose to build shared or static openldap library." ${BUILD_SHARED_LIBS} "BUILD_OPENLDAP" OFF)

cmake_dependent_option(BUILD_OPUS "Build opus library source code from submodule instead of searching it in system libraries." ON "ENABLE_OPUS" OFF)
cmake_dependent_option(BUILD_OPUS_SHARED_LIBS "Choose to build shared or static opus library." ${BUILD_SHARED_LIBS} "BUILD_OPUS" OFF)

cmake_dependent_option(BUILD_SOCI "Build soci library source code from submodule instead of searching it in system libraries." ON "ENABLE_SOCI" OFF)
cmake_dependent_option(BUILD_SOCI_SHARED_LIBS "Choose to build shared or static soci library." ${BUILD_SHARED_LIBS} "BUILD_SOCI" OFF)
set(BUILD_SOCI_BACKENDS "sqlite3" CACHE STRING "List of soci backends to build.")

cmake_dependent_option(BUILD_SPEEX "Build speex and speexdsp library source code from submodule instead of searching it in system libraries." ON "ENABLE_SPEEX" OFF)
cmake_dependent_option(BUILD_SPEEX_SHARED_LIBS "Choose to build shared or static speex and speexdsp libraries." ${BUILD_SHARED_LIBS} "BUILD_SPEEX" OFF)

option(BUILD_SQLITE3 "Build sqlite3 library source code from submodule instead of searching it in system libraries." ON)
cmake_dependent_option(BUILD_SQLITE3_SHARED_LIBS "Choose to build shared or static sqlite3 library." ${BUILD_SHARED_LIBS} "BUILD_SQLITE3" OFF)

cmake_dependent_option(BUILD_VO_AMRWBENC "Build vo-amrwbenc library source code from submodule instead of searching it in system libraries." ON "ENABLE_AMRWB" OFF)
cmake_dependent_option(BUILD_VO_AMRWBENC_SHARED_LIBS "Choose to build shared or static vo-amrwbenc library." ${BUILD_SHARED_LIBS} "BUILD_VO_AMRWBENC" OFF)

option(BUILD_XERCESC "Build xercesc library source code from submodule instead of searching it in system libraries." ON)
cmake_dependent_option(BUILD_XERCESC_SHARED_LIBS "Choose to build shared or static xercesc library." ${BUILD_SHARED_LIBS} "BUILD_XERCESC" OFF)

option(BUILD_ZLIB "Build zlib library source code from submodule instead of searching it in system libraries." ON)
cmake_dependent_option(BUILD_ZLIB_SHARED_LIBS "Choose to build shared or static zlib library." ${BUILD_SHARED_LIBS} "BUILD_ZLIB" OFF)

cmake_dependent_option(BUILD_ZXINGCPP "Build zxing-cpp library source code from submodule instead of searching it in system libraries." ON "ENABLE_QRCODE" OFF)
cmake_dependent_option(BUILD_ZXINGCPP_SHARED_LIBS "Choose to build shared or static zxing-cpp library." ${BUILD_SHARED_LIBS} "BUILD_ZXINGCPP" OFF)


############################################################################
# Define utility functions
############################################################################

function(convert_to_string INPUT_LIST OUTPUT_STRING)
	set(VALUE "")
	foreach(INPUT ${INPUT_LIST})
		set(VALUE "${VALUE} \"${INPUT}\"")
	endforeach()
	set("${OUTPUT_STRING}" "${VALUE}" PARENT_SCOPE)
endfunction()

function(generate_autotools_configuration)
	if(MSVC)
		set(GENERATOR "MSYS Makefiles")
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		    set(MAKE_PROGRAM "make")
		    set(MINGW_SHELL_TYPE "mingw64")
		else()
		    set(MAKE_PROGRAM "mingw32-make")
		    set(MINGW_SHELL_TYPE "mingw32")
		endif()
		# On some environnements, MSVC compilers are still used. Force them to gcc and let cmake find them in PATH
		set(AUTOTOOLS_COMMAND ${CMAKE_COMMAND} -G "${GENERATOR}"
			"-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}.exe"
			"-DCMAKE_C_COMPILER=gcc"
			"-DCMAKE_CXX_COMPILER=gcc"
			"-DCMAKE_RC_COMPILER=rc"
		)
	else()
		set(GENERATOR "${CMAKE_GENERATOR}")
		set(AUTOTOOLS_COMMAND ${CMAKE_COMMAND} -G "${GENERATOR}")
	endif()
	
	if(CMAKE_TOOLCHAIN_FILE)
		list(APPEND AUTOTOOLS_COMMAND "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
	endif()
	if(CMAKE_OSX_ARCHITECTURES)
		list(APPEND AUTOTOOLS_COMMAND "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}")
	endif()
	if(CMAKE_C_COMPILER_LAUNCHER)
		list(APPEND AUTOTOOLS_COMMAND "-DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}")
	endif()
	if(CMAKE_CXX_COMPILER_LAUNCHER)
		list(APPEND AUTOTOOLS_COMMAND "-DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}")
	endif()
	list(APPEND AUTOTOOLS_COMMAND
		"-DAUTOTOOLS_AS_FLAGS=${AUTOTOOLS_AS_FLAGS}"
		"-DAUTOTOOLS_C_FLAGS=${AUTOTOOLS_C_FLAGS}"
		"-DAUTOTOOLS_CPP_FLAGS=${AUTOTOOLS_CPP_FLAGS}"
		"-DAUTOTOOLS_CXX_FLAGS=${AUTOTOOLS_CXX_FLAGS}"
		"-DAUTOTOOLS_OBJC_FLAGS=${AUTOTOOLS_OBJC_FLAGS}"
		"-DAUTOTOOLS_LINKER_FLAGS=${AUTOTOOLS_LINKER_FLAGS}"
	)
	list(APPEND AUTOTOOLS_COMMAND "${PROJECT_SOURCE_DIR}/cmake/Autotools/")
	execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/Autotools)
	execute_process(COMMAND ${AUTOTOOLS_COMMAND} WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/Autotools)
endfunction()

# Compute the relative path to the source directory from the build directory
function(get_relative_source_path SOURCE_PATH BUILD_PATH RELATIVE_PATH_VAR)
	set(COMMON TRUE)
	set(NOT_FINISHED TRUE)
	set(IDX 1)
	string(LENGTH "${SOURCE_PATH}" SOURCE_PATH_LENGTH)
	string(LENGTH "${BUILD_PATH}" BUILD_PATH_LENGTH)
	while(COMMON AND NOT_FINISHED)
		string(SUBSTRING "${SOURCE_PATH}" 0 ${IDX} COMMON_SOURCE_PATH)
		string(SUBSTRING "${BUILD_PATH}" 0 ${IDX} COMMON_BUILD_PATH)
		math(EXPR IDX "${IDX}+1")
		if(NOT COMMON_SOURCE_PATH STREQUAL COMMON_BUILD_PATH)
			set(COMMON FALSE)
		endif()
		if((IDX EQUAL SOURCE_PATH_LENGTH) OR (IDX EQUAL BUILD_PATH_LENGTH))
			set(NOT_FINISHED FALSE)
		endif()
	endwhile()
	math(EXPR IDX "${IDX}-2")
	math(EXPR RELATIVE_SOURCE_PATH_LENGTH "${SOURCE_PATH_LENGTH}-${IDX}")
	math(EXPR RELATIVE_BUILD_PATH_LENGTH "${BUILD_PATH_LENGTH}-${IDX}")
	string(SUBSTRING "${SOURCE_PATH}" ${IDX} ${RELATIVE_SOURCE_PATH_LENGTH} RELATIVE_SOURCE_PATH)
	string(SUBSTRING "${BUILD_PATH}" ${IDX} ${RELATIVE_BUILD_PATH_LENGTH} RELATIVE_BUILD_PATH)
	set(UPDIRS "")
	string(FIND "${RELATIVE_BUILD_PATH}" "/" IDX)
	while(IDX GREATER -1)
		string(CONCAT UPDIRS "${UPDIRS}" "../")
		math(EXPR IDX "${IDX}+1")
		math(EXPR RELATIVE_BUILD_PATH_LENGTH "${RELATIVE_BUILD_PATH_LENGTH}-${IDX}")
		string(SUBSTRING "${RELATIVE_BUILD_PATH}" ${IDX} ${RELATIVE_BUILD_PATH_LENGTH} RELATIVE_BUILD_PATH)
		string(FIND "${RELATIVE_BUILD_PATH}" "/" IDX)
	endwhile()
	if(RELATIVE_BUILD_PATH)
		string(CONCAT UPDIRS "${UPDIRS}" "../")
	endif()
	set("${RELATIVE_PATH_VAR}" "${UPDIRS}/${RELATIVE_SOURCE_PATH}" PARENT_SCOPE)
endfunction()


############################################################################
# Prepare the build system for the inclusion of external projects
############################################################################

if(BUILD_FFMPEG OR BUILD_LIBVPX OR BUILD_OPENH264)
	generate_autotools_configuration()
endif()


############################################################################
# Add external dependencies as subdirectories or external projects
#
# This process uses functions to prevent polluting the top level CMake
# scope with variables that are only meant to be defined for the external
# projects
############################################################################

if(ANDROID)
	function(add_cpufeatures)
		add_subdirectory("cmake/Android/cpufeatures")
		add_dependencies(sdk cpufeatures)
	endfunction()
	add_cpufeatures()

	if(CMAKE_ANDROID_NDK_VERSION LESS 26)
		function(add_support)
			add_subdirectory("cmake/Android/support")
			add_dependencies(sdk support)
		endfunction()
		add_support()
	endif()
endif()

if(BUILD_AOM)
	function(add_aom)
		# Use an ExternalProject here instead of adding the subdirectory because aom has a weird way of defining options
		# and some of them are set in cache with a default value not corresponding to reality that conflicts with other projects.

		if(CCACHE_PROGRAM)
			set(ENABLE_CCACHE ON)
		else()
			set(ENABLE_CCACHE OFF)
		endif()

		if(WIN32)
			set(AOM_LOCATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/aom.lib")
		else()
			set(AOM_LOCATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libaom.a")
		endif()
		set(AOM_BYPRODUCTS ${AOM_LOCATION})

		ExternalProject_Add(libaom
			SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/aom"
			BINARY_DIR "${PROJECT_BINARY_DIR}/external/aom"
			CMAKE_ARGS
			"-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_CROSSCOMPILING=${CMAKE_CROSSCOMPILING}" "-DCMAKE_NO_SYSTEM_FROM_IMPORTED=${CMAKE_NO_SYSTEM_FROM_IMPORTED}"
			"-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}" "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}" "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
			"-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}" "-DCMAKE_INSTALL_DEFAULT_LIBDIR=lib"
			"-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
			"-DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}" "-DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}" "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
			"-DENABLE_CCACHE=${ENABLE_CCACHE}" "-DENABLE_DOCS=OFF" "-DENABLE_EXAMPLES=OFF" "-DENABLE_TESTS=OFF" "-DENABLE_TOOLS=OFF" "-DCONFIG_AV1_DECODER=0" "-DCONFIG_REALTIME_ONLY=1"
			BUILD_BYPRODUCTS ${AOM_BYPRODUCTS}
		)

		file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/include")
		add_library(aom UNKNOWN IMPORTED)
		set_target_properties(aom PROPERTIES IMPORTED_LOCATION ${AOM_LOCATION} INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/include")
		add_dependencies(aom libaom)
	endfunction()
	add_aom()
endif()

if(BUILD_BV16)
	function(add_bv16)
		set(BUILD_SHARED_LIBS ${BUILD_BV16_SHARED_LIBS})

		add_subdirectory("external/bv16-floatingpoint")
		add_dependencies(sdk bv16)
	endfunction()
	add_bv16()
endif()

if(BUILD_CODEC2)
	function(add_codec2)
		set(BUILD_SHARED_LIBS ${BUILD_CODEC2_SHARED_LIBS})
		if(ANDROID)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffast-math")
		endif()

		set(CMAKE_POLICY_DEFAULT_CMP0075 NEW)
		add_subdirectory("external/codec2")
		add_dependencies(sdk codec2)
	endfunction()
	add_codec2()
endif()

if(BUILD_DAV1D)
	function(add_dav1d)
		set(EP_BUILD_DIR "${PROJECT_BINARY_DIR}/external/dav1d")
		get_relative_source_path("${PROJECT_SOURCE_DIR}/external/dav1d" "${EP_BUILD_DIR}" "EP_SOURCE_DIR_RELATIVE_TO_BUILD_DIR")

		set(EP_BUILD_TYPE "release")
		set(EP_PROGRAM_PATH "$PATH")

		set(EP_ADDITIONAL_OPTIONS "-Denable_tools=false -Denable_tests=false")
		if(NOT BUILD_DAV1D_SHARED_LIBS)
			set(EP_ADDITIONAL_OPTIONS "${EP_ADDITIONAL_OPTIONS} --default-library=static")
		endif()

		if(ANDROID)
			set(EP_PROGRAM_PATH "${EP_PROGRAM_PATH}:${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}/bin/")

			if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7-a")
				set(EP_ADDITIONAL_OPTIONS "${EP_ADDITIONAL_OPTIONS} --cross-file ${PROJECT_SOURCE_DIR}/external/dav1d/package/crossfiles/arm-android.meson")
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
				set(EP_ADDITIONAL_OPTIONS "${EP_ADDITIONAL_OPTIONS} --cross-file ${PROJECT_SOURCE_DIR}/external/dav1d/package/crossfiles/aarch64-android.meson")
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
				set(EP_ADDITIONAL_OPTIONS "${EP_ADDITIONAL_OPTIONS} --cross-file ${PROJECT_SOURCE_DIR}/external/dav1d/package/crossfiles/x86_64-android.meson")
			else()
				set(EP_ADDITIONAL_OPTIONS "${EP_ADDITIONAL_OPTIONS} --cross-file ${PROJECT_SOURCE_DIR}/external/dav1d/package/crossfiles/x86-android.meson")
			endif()
		elseif(APPLE)
			# If we are cross compiling generate the corresponding file to use with meson
			if(IOS OR NOT CMAKE_SYSTEM_PROCESSOR STREQUAL CMAKE_HOST_SYSTEM_PROCESSOR)
				string(TOLOWER "${CMAKE_SYSTEM_NAME}" EP_SYSTEM_NAME)

				if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
					set(EP_SYSTEM_ENDIAN "big")
				else()
					set(EP_SYSTEM_ENDIAN "little")
				endif()

				if(NOT IOS AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
					set(EP_SYSTEM_PROCESSOR "aarch64")
				else()
					set(EP_SYSTEM_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")
				endif()

				if(IOS)
					if(PLATFORM STREQUAL "Simulator")
						set(EP_OSX_DEPLOYMENT_TARGET "-miphonesimulator-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
					else()
						set(EP_OSX_DEPLOYMENT_TARGET "-mios-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
					endif()

					string(REGEX MATCH "^(arm*|aarch64)" ARM_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
					if(ARM_ARCH AND NOT ${XCODE_VERSION} VERSION_LESS 7)
						#set(EP_ADDITIONAL_FLAGS ", '-fembed-bitcode'")
					endif()
				else()
					set(EP_OSX_DEPLOYMENT_TARGET "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
				endif()

				configure_file("cmake/Meson/crossfile-apple.meson.cmake" ${PROJECT_BINARY_DIR}/EP_dav1d_crossfile.meson)
				set(EP_ADDITIONAL_OPTIONS "${EP_ADDITIONAL_OPTIONS} --cross-file ${PROJECT_BINARY_DIR}/EP_dav1d_crossfile.meson")
			endif()
		endif()

		configure_file("cmake/Meson/configure.sh.cmake" "${PROJECT_BINARY_DIR}/EP_dav1d_configure.sh")
		configure_file("cmake/Meson/build.sh.cmake" "${PROJECT_BINARY_DIR}/EP_dav1d_build.sh")
		configure_file("cmake/Meson/install.sh.cmake" "${PROJECT_BINARY_DIR}/EP_dav1d_install.sh")

		if(BUILD_DAV1D_SHARED_LIBS)
			if(APPLE)
				set(EP_LIBRARY_NAME "libdav1d.dylib")
			elseif(WIN32)
				set(EP_LIBRARY_NAME "dav1d_dll.lib")
			else()
				set(EP_LIBRARY_NAME "libdav1d.so")
			endif()
		else()
			set(EP_LIBRARY_NAME "libdav1d.a")
		endif()

		file(MAKE_DIRECTORY "${EP_BUILD_DIR}")

		ExternalProject_Add(dav1d
			SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/dav1d"
			CONFIGURE_COMMAND "sh" "${PROJECT_BINARY_DIR}/EP_dav1d_configure.sh"
			BUILD_COMMAND "sh" "${PROJECT_BINARY_DIR}/EP_dav1d_build.sh"
			INSTALL_COMMAND "sh" "${PROJECT_BINARY_DIR}/EP_dav1d_install.sh"
			CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
			LOG_BUILD TRUE
			LOG_INSTALL TRUE
			LOG_OUTPUT_ON_FAILURE TRUE
			BUILD_BYPRODUCTS "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/${EP_LIBRARY_NAME}"
		)

		file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/include")
		add_library(libdav1d UNKNOWN IMPORTED)
		set_target_properties(libdav1d PROPERTIES IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/${EP_LIBRARY_NAME}" INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/include")
		add_dependencies(libdav1d dav1d)
	endfunction()
	add_dav1d()
endif()

if(BUILD_DECAF)
	function(add_decaf)
		if(BUILD_DECAF_SHARED_LIBS)
			set(ENABLE_SHARED ON)
			set(ENABLE_STATIC OFF)
		else()
			set(ENABLE_SHARED OFF)
			set(ENABLE_STATIC ON)
		endif()
		set(ENABLE_STRICT OFF)

		set(CMAKE_POLICY_DEFAULT_CMP0077 NEW) # Prevent project from overriding the options we just set here
		add_subdirectory("external/decaf")
		if(ENABLE_SHARED)
			add_dependencies(sdk decaf)
		else()
			add_dependencies(sdk decaf-static)
		endif()
	endfunction()
	add_decaf()
endif()

if(BUILD_FFMPEG)
	function(add_ffmpeg)
		include("${PROJECT_BINARY_DIR}/Autotools/Autotools.cmake")

		set(EP_BUILD_DIR "${PROJECT_BINARY_DIR}/external/ffmpeg")
		set(EP_CONFIG_H_FILE "config.h")
		set(EP_CONFIGURE_COMMAND "${PROJECT_SOURCE_DIR}/external/ffmpeg/configure")
		set(EP_INSTALL_TARGET "install")
		set(EP_CONFIGURE_OPTIONS)
		set(EP_CROSS_COMPILATION_OPTIONS)
		set(EP_MAKE_OPTIONS)
		set(EP_ASFLAGS)
		set(EP_CPPFLAGS)
		set(EP_CFLAGS)
		set(EP_CXXFLAGS)
		set(EP_OBJCFLAGS)
		set(EP_LDFLAGS)
		list(APPEND EP_CONFIGURE_OPTIONS
			"--disable-doc"
			"--disable-zlib"
			"--disable-bzlib"
			"--disable-ffplay"
			"--disable-ffprobe"
			"--disable-ffserver"
			"--disable-avdevice"
			"--disable-avfilter"
			"--disable-network"
			"--disable-avformat"
			"--disable-everything"
			"--enable-decoder=mjpeg"
			"--enable-encoder=mjpeg"
			# Disable video acceleration support for compatibility with older Mac OS X versions (vda, vaapi, vdpau).
			"--disable-vda"
			"--disable-vaapi"
			"--disable-vdpau"
			"--ar=\$AR"
			"--nm=\$NM"
			"--extra-cflags=\$CFLAGS -w"
			"--extra-cxxflags=\$CXXFLAGS"
			"--extra-ldflags=\$LDFLAGS"
		)
		if(NOT WIN32)
			list(APPEND EP_CONFIGURE_OPTIONS "--cc=\$CC")
		else()
			list(APPEND EP_CONFIGURE_OPTIONS "--cc=gcc")
		endif()
		if(ENABLE_H263 OR IOS)
			list(APPEND EP_CONFIGURE_OPTIONS "--enable-decoder=h263" "--enable-encoder=h263")
		endif()
		if(ENABLE_H263P OR IOS)
			list(APPEND EP_CONFIGURE_OPTIONS "--enable-encoder=h263p")
		endif()
		if(ENABLE_MPEG4 OR IOS)
			list(APPEND EP_CONFIGURE_OPTIONS "--enable-decoder=mpeg4" "--enable-encoder=mpeg4")
		endif()
		set(EP_LINKING_TYPE "--disable-static" "--enable-shared")
		set(EP_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
		if(WIN32)
			set(EP_TARGET_OS "mingw32")
			set(EP_ARCH "i386")
			if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
				set(EP_ARCH "x86_64")
			endif()
			list(APPEND EP_CFLAGS "-include windows.h")
			list(APPEND EP_LDFLAGS "-static-libgcc")
			list(APPEND EP_CONFIGURE_OPTIONS "--enable-runtime-cpudetect")
			if(CMAKE_BUILD_PARALLEL_LEVEL)
				list(APPEND EP_MAKE_OPTIONS "-j${CMAKE_BUILD_PARALLEL_LEVEL}")
			endif()
		else()
			if(APPLE)
				if(IOS)
					set(EP_TARGET_OS "darwin")
					list(APPEND EP_CONFIGURE_OPTIONS
						"--enable-decoder=h264"
						"--disable-iconv"
						"--disable-mmx"
						"--enable-cross-compile"
						"--cross-prefix=${SDK_BIN_PATH}/"
						"--sysroot=${CMAKE_OSX_SYSROOT}"
					)
					list(APPEND EP_MAKE_OPTIONS "RANLIB=\"\$RANLIB\"")
					if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
						set(EP_ARCH "arm64")
					else()
						set(EP_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
					endif()
					if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7")
						list(APPEND EP_CONFIGURE_OPTIONS "--enable-neon" "--cpu=cortex-a8" "--disable-armv5te" "--enable-armv6" "--enable-armv6t2")
					endif()
				else()
					set(EP_TARGET_OS "macos")
					if(CMAKE_OSX_DEPLOYMENT_TARGET)
						set(FLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
					else()
						set(FLAGS "")
					endif()
					list(APPEND EP_CFLAGS "--target=${CMAKE_C_COMPILER_TARGET} ${FLAGS}")
					list(APPEND EP_CPPFLAGS "--target=${CMAKE_C_COMPILER_TARGET} ${FLAGS}")
					list(APPEND EP_CXXFLAGS "--target=${CMAKE_C_COMPILER_TARGET} ${FLAGS}")
					list(APPEND EP_LDFLAGS "--target=${CMAKE_C_COMPILER_TARGET} ${FLAGS}")
					if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
						set(EP_TARGET_OS "macos11")
						list(APPEND EP_CONFIGURE_OPTIONS "--disable-asm") #because of gas-preprocessor error
						list(APPEND EP_CONFIGURE_OPTIONS "--cc=$CC")
					endif()
					list(APPEND EP_CONFIGURE_OPTIONS
						"--enable-runtime-cpudetect"
						"--sysroot=${CMAKE_OSX_SYSROOT}"
					)
				endif()
			elseif(ANDROID)
				get_filename_component(TOOLCHAIN_PATH "${CMAKE_LINKER}" DIRECTORY)
				list(APPEND EP_CONFIGURE_OPTIONS
					"--enable-decoder=h264"
					"--disable-iconv"
					"--disable-mmx"
					"--enable-cross-compile"
					"--cross-prefix=${TOOLCHAIN_PATH}/"
					"--sysroot=${CMAKE_SYSROOT}"
				)
				set(EP_TARGET_OS "linux")
				set(EP_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
				list(APPEND EP_MAKE_OPTIONS "RANLIB=\"\$RANLIB\"")
				if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7-a")
					list(APPEND EP_CONFIGURE_OPTIONS "--enable-neon" "--cpu=cortex-a8" "--disable-armv5te" "--enable-armv6" "--enable-armv6t2")
				else()
					list(APPEND EP_CONFIGURE_OPTIONS "--disable-mmx" "--disable-sse2" "--disable-ssse3" "--disable-asm")
				endif()
				if(CMAKE_C_COMPILER_TARGET) # When building with clang
					list(APPEND EP_CONFIGURE_OPTIONS "--extra-cflags=--target=${CMAKE_C_COMPILER_TARGET} --gcc-toolchain=${_ANDROID_TOOL_C_COMPILER_EXTERNAL_TOOLCHAIN}")
					list(APPEND EP_CONFIGURE_OPTIONS "--extra-ldflags=--target=${CMAKE_C_COMPILER_TARGET} --gcc-toolchain=${_ANDROID_TOOL_C_COMPILER_EXTERNAL_TOOLCHAIN}")
				endif()
			else()
				set(EP_TARGET_OS "linux")
				list(APPEND EP_CONFIGURE_OPTIONS "--enable-runtime-cpudetect")
				if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7")
					list(APPEND EP_CONFIGURE_OPTIONS "--cpu=cortex-a8" "--enable-fft")
					list(APPEND EP_CFLAGS "-mfpu=neon")
					list(APPEND EP_CXXFLAGS "-mfpu=neon")
				endif()
			endif()
		endif()

		if(WIN32)
			list(APPEND EP_CROSS_COMPILATION_OPTIONS
				"--prefix=${CMAKE_INSTALL_PREFIX}"
				"--libdir=${CMAKE_INSTALL_FULL_LIBDIR}"
				"--shlibdir=${CMAKE_INSTALL_FULL_BINDIR}"
				"--arch=${EP_ARCH}"
				"--target-os=${EP_TARGET_OS}"
			)
		else()
			list(APPEND EP_CROSS_COMPILATION_OPTIONS
				"--prefix=${CMAKE_INSTALL_PREFIX}"
				"--libdir=${CMAKE_INSTALL_FULL_LIBDIR}"
				"--shlibdir=${CMAKE_INSTALL_FULL_LIBDIR}"
				"--arch=${EP_ARCH}"
				"--target-os=${EP_TARGET_OS}"
			)
		endif()

		convert_to_string("${EP_CROSS_COMPILATION_OPTIONS}" EP_CROSS_COMPILATION_OPTIONS)
		convert_to_string("${EP_LINKING_TYPE}" EP_LINKING_TYPE)
		convert_to_string("${EP_CONFIGURE_OPTIONS}" EP_CONFIGURE_OPTIONS)
		convert_to_string("${EP_MAKE_OPTIONS}" EP_MAKE_OPTIONS)

		configure_file("cmake/Autotools/configure.sh.cmake" "${PROJECT_BINARY_DIR}/EP_ffmpeg_configure.sh")
		configure_file("cmake/Autotools/build.sh.cmake" "${PROJECT_BINARY_DIR}/EP_ffmpeg_build.sh")
		configure_file("cmake/Autotools/install.sh.cmake" "${PROJECT_BINARY_DIR}/EP_ffmpeg_install.sh")

		if(APPLE)
			set(AVCODEC_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libavcodec.dylib")
			set(AVUTIL_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libavutil.dylib")
			set(SWRESAMPLE_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libswresample.dylib")
			set(SWSCALE_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libswscale.dylib")
		else()
			set(AVCODEC_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libavcodec.so")
			set(AVUTIL_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libavutil.so")
			set(SWRESAMPLE_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libswresample.so")
			set(SWSCALE_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libswscale.so")
		endif()
		set(FFMPEG_BYPRODUCTS
			"${AVCODEC_IMPORTED_LOCATION}" "${AVUTIL_IMPORTED_LOCATION}"
			"${SWRESAMPLE_IMPORTED_LOCATION}" "${SWSCALE_IMPORTED_LOCATION}"
		)

		ExternalProject_Add(ffmpeg
			SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/ffmpeg"
			CONFIGURE_COMMAND "sh" "${PROJECT_BINARY_DIR}/EP_ffmpeg_configure.sh"
			BUILD_COMMAND "sh" "${PROJECT_BINARY_DIR}/EP_ffmpeg_build.sh"
			INSTALL_COMMAND "sh" "${PROJECT_BINARY_DIR}/EP_ffmpeg_install.sh"
			LOG_CONFIGURE TRUE
			LOG_BUILD TRUE
			LOG_INSTALL TRUE
			LOG_OUTPUT_ON_FAILURE TRUE
			BUILD_BYPRODUCTS ${FFMPEG_BYPRODUCTS}
		)
		file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/include")
		add_library(avcodec UNKNOWN IMPORTED)
		add_library(avutil UNKNOWN IMPORTED)
		add_library(swresample UNKNOWN IMPORTED)
		add_library(swscale UNKNOWN IMPORTED)
		set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION "${AVCODEC_IMPORTED_LOCATION}" INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/include")
		set_target_properties(avutil PROPERTIES IMPORTED_LOCATION "${AVUTIL_IMPORTED_LOCATION}" INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/include")
		set_target_properties(swresample PROPERTIES IMPORTED_LOCATION "${SWRESAMPLE_IMPORTED_LOCATION}" INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/include")
		set_target_properties(swscale PROPERTIES IMPORTED_LOCATION "${SWSCALE_IMPORTED_LOCATION}" INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/include")
		add_dependencies(avcodec ffmpeg)
		add_dependencies(avutil ffmpeg)
		add_dependencies(swresample ffmpeg)
		add_dependencies(swscale ffmpeg)
	endfunction()
	add_ffmpeg()
endif()

if(BUILD_GSM)
	function(add_gsm)
		set(BUILD_SHARED_LIBS ${BUILD_GSM_SHARED_LIBS})

		add_subdirectory("external/gsm")
		add_dependencies(sdk gsm)
	endfunction()
	add_gsm()
endif()

if(BUILD_JSONCPP)
	function(add_jsoncpp)
		if(BUILD_JSONCPP_SHARED_LIBS)
			set(BUILD_SHARED_LIBS ON)
			set(BUILD_STATIC_LIBS OFF)
			set(BUILD_OBJECT_LIBS OFF)
		else()
			set(BUILD_SHARED_LIBS OFF)
			set(BUILD_STATIC_LIBS ON)
			set(BUILD_OBJECT_LIBS OFF)
		endif()
		set(JSONCPP_WITH_TESTS OFF)
		set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF)
		set(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF)

		add_subdirectory("external/jsoncpp")
		if(BUILD_JSONCPP_SHARED_LIBS)
			add_dependencies(sdk jsoncpp_lib)
		else()
			add_dependencies(sdk jsoncpp_static)
		endif()
	endfunction()
	add_jsoncpp()
endif()

if(BUILD_LIBJPEGTURBO)
	function(add_libjpegturbo)
		set(ENABLE_TOOLS OFF)
		if(BUILD_LIBJPEGTURBO_SHARED_LIBS)
			set(BUILD_SHARED_LIBS ON)
		else()
			set(BUILD_SHARED_LIBS OFF)
			set(CMAKE_POSITION_INDEPENDENT_CODE ON)
		endif()
		add_subdirectory("external/libjpeg-turbo")
		add_dependencies(sdk turbojpeg)
	endfunction()
	add_libjpegturbo()
endif()

if(BUILD_LIBOQS)
	function(add_liboqs)
		set(BUILD_SHARED_LIBS ${BUILD_LIBOQS_SHARED_LIBS})
		set(OQS_BUILD_ONLY_LIB ON)
		set(OQS_DIST_LIB ON)
		set(OQS_USE_OPENSSL OFF)
		set(OQS_MINIMAL_BUILD "KEM_ML_KEM;KEM_kyber;KEM_hqc;OQS_ENABLE_KEM_kyber_512;OQS_ENABLE_KEM_kyber_768;OQS_ENABLE_KEM_kyber_1024;OQS_ENABLE_KEM_hqc_128;OQS_ENABLE_KEM_hqc_192;OQS_ENABLE_KEM_hqc_256;OQS_ENABLE_KEM_ml_kem_512;OQS_ENABLE_KEM_ml_kem_768;OQS_ENABLE_KEM_ml_kem_1024;" CACHE STRING "")
		message(MESSAGE "CMAKE_CROSSCOMPILING :" ${CMAKE_CROSSCOMPILING})
		set(CMAKE_POLICY_DEFAULT_CMP0077 NEW) # Prevent project from overriding the options we just set here
		add_subdirectory("external/liboqs")
		add_dependencies(sdk oqs)
	endfunction()
	add_liboqs()
endif()



if(BUILD_LIBVPX)
	function(add_libvpx)

		include("${PROJECT_BINARY_DIR}/Autotools/Autotools.cmake")

		set(EP_SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/libvpx")
		set(EP_BUILD_DIR "${PROJECT_BINARY_DIR}/external/libvpx")
		set(EP_CONFIG_H_FILE "vpx_config.h")
		set(EP_CONFIGURE_COMMAND "${EP_SOURCE_DIR}/configure")
		set(EP_INSTALL_TARGET "install")
		set(EP_CONFIGURE_OPTIONS)
		set(EP_CROSS_COMPILATION_OPTIONS)
		# BUILD_ROOT is set by Xcode, but we still need the current build root.
		# See https://gitlab.linphone.org/BC/public/external/libvpx/blob/v1.7.0-linphone/build/make/Makefile
		set(EP_MAKE_OPTIONS "BUILD_ROOT=.")
		set(EP_ASFLAGS)
		set(EP_CPPFLAGS)
		set(EP_CFLAGS)
		set(EP_CXXFLAGS)
		set(EP_OBJCFLAGS)
		set(EP_LDFLAGS)
		list(APPEND EP_CONFIGURE_OPTIONS
			"--enable-error-concealment"
			"--enable-multithread"
			"--enable-realtime-only"
			"--enable-spatial-resampling"
			"--enable-vp8"
			"--disable-vp9"
			"--enable-libs"
			"--disable-install-docs"
			"--disable-debug-libs"
			"--disable-examples"
			"--disable-unit-tests"
			"--disable-tools"
			"--as=yasm"
		)
		string(FIND "${CMAKE_C_COMPILER_LAUNCHER}" "ccache" CCACHE_ENABLED)
		if(NOT "${CCACHE_ENABLED}" STREQUAL "-1")
			list(APPEND EP_CONFIGURE_OPTIONS "--enable-ccache")
		endif()


		if(WIN32)
			if(MSVC)
				if(CMAKE_GENERATOR MATCHES "^Visual Studio")
					string(REPLACE " " ";" GENERATOR_LIST "${CMAKE_GENERATOR}")
					list(GET GENERATOR_LIST 2 VS_VERSION)
				else()
					if("${MSVC_TOOLSET_VERSION}" STREQUAL "143")
						set(VS_VERSION "17")
					elseif("${MSVC_TOOLSET_VERSION}" STREQUAL "142")
						set(VS_VERSION "16")
					elseif( "${MSVC_TOOLSET_VERSION}" STREQUAL "141")
						set(VS_VERSION "15")
					elseif("${MSVC_TOOLSET_VERSION}" STREQUAL "140")
						set(VS_VERSION "14")
					else()
						set(VS_VERSION "15")
					endif()
				endif()
				if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			    set(EP_TARGET "x86_64-win64-vs${VS_VERSION}")
					set(EP_INSTALL_SUBDIR "x64")
				else()
			    set(EP_TARGET "x86-win32-vs${VS_VERSION}")
					set(EP_INSTALL_SUBDIR "Win32")
				endif()
				message(STATUS "Build VPX with configuration: ${EP_TARGET}")
				execute_process(COMMAND "cmd.exe" "/c" "${PROJECT_SOURCE_DIR}/cmake/Windows/windows_env.bat" "${VS_VERSION}"
					WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
				)
				file(READ "${PROJECT_BINARY_DIR}/windowsenv_include.txt" EP_ENV_INCLUDE)
				string(REPLACE "\n" "" EP_ENV_INCLUDE "${EP_ENV_INCLUDE}")
				file(READ "${PROJECT_BINARY_DIR}/windowsenv_lib.txt" EP_ENV_LIB)
				string(REPLACE "\n" "" EP_ENV_LIB "${EP_ENV_LIB}")
				file(READ "${PROJECT_BINARY_DIR}/windowsenv_libpath.txt" EP_ENV_LIBPATH)
				string(REPLACE "\n" "" EP_ENV_LIBPATH "${EP_ENV_LIBPATH}")
			else()
		    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
					set(EP_TARGET "x86_64-win64-gcc")
		    else()
					set(EP_TARGET "x86-win32-gcc")
		    endif()
			endif()
			set(EP_LINKING_TYPE "--enable-static" "--disable-shared" "--enable-pic")
		elseif(APPLE)
			if(IOS)
				if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
					set(EP_TARGET "arm64-darwin-gcc")
				elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7")
					set(EP_TARGET "armv7-darwin-gcc")
				elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
					set(EP_TARGET "x86_64-iphonesimulator-gcc")
				else()
					set(EP_TARGET "x86-iphonesimulator-gcc")
				endif()
			else()
				# Select darwin version
				string(REPLACE "." ";" VERSION_LIST ${CMAKE_OSX_DEPLOYMENT_TARGET})
				list(GET VERSION_LIST 0 _VERSION_MAJOR)
				list(GET VERSION_LIST 1 _VERSION_MINOR)
				if(_VERSION_MAJOR STREQUAL "12")
					set(DARWIN "darwin21")
				elseif(_VERSION_MAJOR STREQUAL "11")
					set(DARWIN "darwin20")
				elseif(_VERSION_MAJOR STREQUAL "10")
					math(EXPR _DARWIN_VERSION "4+${_VERSION_MINOR}")
					set(DARWIN "darwin${_DARWIN_VERSION}")
					message(STATUS "Darwin version guess : ${CMAKE_OSX_DEPLOYMENT_TARGET} osx => ${DARWIN}")
				elseif(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_GREATER_EQUAL 11)
					math(EXPR _DARWIN_VERSION "20+${_VERSION_MAJOR}-11")
					set(DARWIN "darwin${_DARWIN_VERSION}")
					message(STATUS "Darwin version guess : ${CMAKE_OSX_DEPLOYMENT_TARGET} osx => ${DARWIN}")
				else()
					message(WARNING "CMAKE_OSX_DEPLOYMENT_TARGET is not found. Build on Darwin10 by default.")
					set(DARWIN "darwin10")
				endif()
				if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
					set(EP_TARGET "x86_64-${DARWIN}-gcc")
				elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
					set(EP_TARGET "arm64-${DARWIN}-gcc")
					list(APPEND EP_CFLAGS "-arch arm64")
					list(APPEND EP_CXXGLAGS "-arch arm64")
					list(APPEND EP_CPPFLAGS "-arch arm64")
					list(APPEND EP_LDFLAGS "-arch arm64")
				else()
					set(EP_TARGET "x86-${DARWIN}-gcc")
				endif()
			endif()
			set(EP_LINKING_TYPE "--enable-static" "--disable-shared" "--enable-pic")
		elseif(ANDROID)
			if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv5te")
				message(FATAL_ERROR "VPX cannot be built on arm.")
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7-a")
				set(EP_TARGET "armv7-android-gcc")
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
				set(EP_TARGET "arm64-android-gcc")
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
				set(EP_TARGET "x86_64-android-gcc")
			else()
				set(EP_TARGET "x86-android-gcc")
			endif()
			list(APPEND EP_CONFIGURE_OPTIONS
				"--sdk-path=${CMAKE_ANDROID_NDK}/"
				"--android_ndk_api=${ANDROID_NATIVE_API_LEVEL}"
			)
			set(EP_LINKING_TYPE "--enable-static" "--disable-shared" "--enable-pic")
			list(APPEND EP_CROSS_COMPILATION_OPTIONS "--extra-cflags=-fPIC" "--extra-cxxflags=-fPIC")
		elseif(QNX)
			set(EP_TARGET "armv7-qnx-gcc")
			list(APPEND EP_CONFIGURE_OPTIONS
				"--libc=${QNX_TARGET}/${ROOT_PATH_SUFFIX}"
				"--force-target=armv7-qnx-gcc"
				"--disable-runtime-cpu-detect"
			)
			list(REMOVE_ITEM EP_CONFIGURE_OPTIONS "--enable-multithread")
		else()
			if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7l")
				set(EP_TARGET "armv7-linux-gcc")
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
				# A bit hacky, but CMAKE_SYSTEM_PROCESSOR sometimes doesn't include abi version so assume `armv7` by default
				set(EP_TARGET "armv7-linux-gcc")
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
				set(EP_TARGET "arm64-linux-gcc")
			else()
				if(CMAKE_SIZEOF_VOID_P EQUAL 8)
					set(EP_TARGET "x86_64-linux-gcc")
				else()
					set(EP_TARGET "x86-linux-gcc")
				endif()
			endif()
			set(EP_LINKING_TYPE "--disable-static" "--enable-shared")
		endif()
		list(APPEND EP_CROSS_COMPILATION_OPTIONS "--prefix=${CMAKE_INSTALL_PREFIX}")
		list(APPEND EP_CROSS_COMPILATION_OPTIONS "--libdir=${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" "--target=${EP_TARGET}")
		if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND CMAKE_C_COMPILER_VERSION VERSION_LESS "4.0")
			list(APPEND EP_CONFIGURE_OPTIONS "--disable-avx512")
		endif()
		set(EP_CONFIGURE_ENV "CC=$CC_NO_LAUNCHER LD=$CC_NO_LAUNCHER ASFLAGS=$ASFLAGS CFLAGS=$CFLAGS LDFLAGS=$LDFLAGS")

		convert_to_string("${EP_CROSS_COMPILATION_OPTIONS}" EP_CROSS_COMPILATION_OPTIONS)
		convert_to_string("${EP_LINKING_TYPE}" EP_LINKING_TYPE)
		convert_to_string("${EP_CONFIGURE_OPTIONS}" EP_CONFIGURE_OPTIONS)
		convert_to_string("${EP_MAKE_OPTIONS}" EP_MAKE_OPTIONS)

		if(WIN32)
			configure_file("cmake/Windows/vpx_configure.sh.cmake" "${PROJECT_BINARY_DIR}/EP_vpx_configure.sh")
			configure_file("cmake/Windows/vpx_build.sh.cmake" "${PROJECT_BINARY_DIR}/EP_vpx_build.sh")
			configure_file("cmake/Windows/vpx_install.sh.cmake" "${PROJECT_BINARY_DIR}/EP_vpx_install.sh")
		else()
			configure_file("cmake/Autotools/configure.sh.cmake" "${PROJECT_BINARY_DIR}/EP_vpx_configure.sh")
			configure_file("cmake/Autotools/build.sh.cmake" "${PROJECT_BINARY_DIR}/EP_vpx_build.sh")
			configure_file("cmake/Autotools/install.sh.cmake" "${PROJECT_BINARY_DIR}/EP_vpx_install.sh")
		endif()

		if(WIN32)
			set(VPX_IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/${EP_INSTALL_SUBDIR}/vpxmd.lib")
		elseif(APPLE OR ANDROID)
			set(VPX_IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libvpx.a")
		else()
			set(VPX_IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libvpx.so")
		endif()
		set(VPX_BYPRODUCTS "${VPX_IMPORTED_LOCATION}")

		ExternalProject_Add(vpx
			SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/libvpx"
			CONFIGURE_COMMAND "sh" "${PROJECT_BINARY_DIR}/EP_vpx_configure.sh"
			BUILD_COMMAND "sh" "${PROJECT_BINARY_DIR}/EP_vpx_build.sh"
			INSTALL_COMMAND "sh" "${PROJECT_BINARY_DIR}/EP_vpx_install.sh"
			CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
			LOG_CONFIGURE TRUE
			LOG_BUILD TRUE
			LOG_INSTALL TRUE
			LOG_OUTPUT_ON_FAILURE TRUE
			CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
			BUILD_BYPRODUCTS ${VPX_BYPRODUCTS}
		)
		file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/include")
		add_library(libvpx UNKNOWN IMPORTED)
		set_target_properties(libvpx PROPERTIES IMPORTED_LOCATION "${VPX_IMPORTED_LOCATION}" INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/include")
		add_dependencies(libvpx vpx)
		add_dependencies(sdk libvpx)
	endfunction()
	add_libvpx()
endif()

if(BUILD_LIBXML2)
	function(add_xml2)
		set(BUILD_SHARED_LIBS ${BUILD_LIBXML2_SHARED_LIBS})

		add_subdirectory("external/libxml2")
		add_dependencies(sdk xml2)
	endfunction()
	add_xml2()
endif()

if(BUILD_LIBYUV)
	function(add_libyuv)
		set(BUILD_SHARED_LIBS ${BUILD_LIBYUV_SHARED_LIBS})
		set(ENABLE_TEST OFF)

		set(CMAKE_POLICY_DEFAULT_CMP0077 NEW) # Prevent project from overriding the options we just set here
		add_subdirectory("external/libyuv")
		add_dependencies(sdk yuv)
	endfunction()
	add_libyuv()
endif()

if(BUILD_MBEDTLS)
	function(add_mbedtls)
		if(BUILD_MBEDTLS_SHARED_LIBS)
			set(USE_SHARED_MBEDTLS_LIBRARY ON)
			set(USE_STATIC_MBEDTLS_LIBRARY OFF)
		else()
			set(USE_SHARED_MBEDTLS_LIBRARY OFF)
			set(USE_STATIC_MBEDTLS_LIBRARY ON)
		endif()
		set(ENABLE_PROGRAMS OFF)
		set(ENABLE_TESTING OFF)
		set(MBEDTLS_FATAL_WARNINGS ${BUILD_MBEDTLS_WITH_FATAL_WARNINGS})

		add_subdirectory("external/mbedtls")
		add_dependencies(sdk mbedtls)
	endfunction()
	add_mbedtls()
endif()

if(BUILD_OPENSSL)
	function(add_openssl)
		if(BUILD_OPENSSL_SHARED_LIBS)
			list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "shared")
			set(OPENSSL_LIBRARY_TYPE SHARED)
			if(WIN32)
				set(OPENSSL_SSL_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/ssl.lib")
				set(OPENSSL_CRYPTO_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/crypto.lib")
			elseif(APPLE)
				set(OPENSSL_SSL_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libssl.dylib")
				set(OPENSSL_CRYPTO_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libcrypto.dylib")
			else()
				set(OPENSSL_SSL_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libssl.so")
				set(OPENSSL_CRYPTO_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libcrypto.so")
			endif()
		else()
			list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "no-shared")
			set(OPENSSL_LIBRARY_TYPE STATIC)
			if(WIN32)
				set(OPENSSL_SSL_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/ssl.lib")
				set(OPENSSL_CRYPTO_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/crypto.lib")
			else()
				set(OPENSSL_SSL_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libssl.a")
				set(OPENSSL_CRYPTO_IMPORTED_LOCATION "${CMAKE_INSTALL_FULL_LIBDIR}/libcrypto.a")
			endif()
		endif()

		if(APPLE)
			list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "-lm")
		endif()

		if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
			set(OPENSSL_TARGET_ARCH "arm64")
		elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
			set(OPENSSL_TARGET_ARCH "x86_64")
		endif()

		if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
			list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "VC-WIN64A")
		elseif(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
			list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "VC-WIN32")
		elseif(APPLE)
			if (IOS)
				if(PLATFORM STREQUAL "OS")
					list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "ios-xcrun" "-fembed-bitcode")
				elseif(PLATFORM STREQUAL "Simulator")
					list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "iossimulator-xcrun")
				endif()
				list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "-arch ${OPENSSL_TARGET_ARCH}")
				list(APPEND OPENSSL_CONFIGURE_ARGUMENTS no-asm no-hw no-async)
			else()
				list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "darwin64-${OPENSSL_TARGET_ARCH}-cc")
			endif()
		elseif(ANDROID)
			list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "android-${OPENSSL_TARGET_ARCH}")
		else()
			list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "linux-${OPENSSL_TARGET_ARCH}")
		endif()
		include(GNUInstallDirs)
		list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "--prefix=${CMAKE_INSTALL_PREFIX}")
		list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "--openssldir=${CMAKE_INSTALL_PREFIX}")
		list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "--libdir=lib")
		list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "-L${CMAKE_INSTALL_FULL_LIBDIR}")
		list(APPEND OPENSSL_CONFIGURE_ARGUMENTS "-I${CMAKE_INSTALL_FULL_INCLUDEDIR}")

		set(OPENSSL_SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/openssl")
		set(OPENSSL_BINARY_DIR "${PROJECT_BINARY_DIR}/openssl-ep-prefix/src/openssl-ep-build")

		if(WIN32)
			set(OPENSSL_MAKE nmake)
		else()
			set(OPENSSL_MAKE make -C ${OPENSSL_BINARY_DIR} -j)
		endif()

		if (EXISTS ${OPENSSL_BINARY_DIR}/Makefile)
			message(INFO "Skipping Openssl build configuration command because a makefile already exists: ${OPENSSL_BINARY_DIR}/Makefile")
			set(OPENSSL_CONFIGURE_COMMAND echo "Openssl build already configured")
		else ()
			if(WIN32)
				set(OPENSSL_CONFIGURE perl ${OPENSSL_SOURCE_DIR}/Configure)
			else()
				set(OPENSSL_CONFIGURE ${OPENSSL_SOURCE_DIR}/Configure)
			endif()
			set(OPENSSL_CONFIGURE_COMMAND ${OPENSSL_CONFIGURE} ${OPENSSL_CONFIGURE_ARGUMENTS})
		endif()

		ExternalProject_Add(openssl-ep
			SOURCE_DIR "${OPENSSL_SOURCE_DIR}"
			GIT_REPOSITORY "https://github.com/openssl/openssl.git"
			GIT_TAG "openssl-3.0.12"
			GIT_SHALLOW TRUE
			GIT_PROGRESS TRUE
			CONFIGURE_COMMAND ${OPENSSL_CONFIGURE_COMMAND}
			BUILD_COMMAND ${OPENSSL_MAKE} build_sw
			INSTALL_COMMAND ${OPENSSL_MAKE} install_sw
			LOG_CONFIGURE TRUE
			LOG_BUILD TRUE
			LOG_INSTALL TRUE
			LOG_OUTPUT_ON_FAILURE TRUE
			BUILD_BYPRODUCTS ${OPENSSL_SSL_IMPORTED_LOCATION} ${OPENSSL_CRYPTO_IMPORTED_LOCATION}
		)
		include(CMakePackageConfigHelpers)
		set(OPENSSL_INCLUDE_DIR ${CMAKE_INSTALL_FULL_INCLUDEDIR})
		set(OPENSSL_VERSION "3.0.12")
		configure_package_config_file(
				${PROJECT_SOURCE_DIR}/cmake/PackageConfig/OpenSSLConfig.cmake.in
				${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/OpenSSLConfig.cmake
				INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OpenSSL
				PATH_VARS OPENSSL_INCLUDE_DIR OPENSSL_CRYPTO_IMPORTED_LOCATION OPENSSL_SSL_IMPORTED_LOCATION
		)
		write_basic_package_version_file(
				${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/OpenSSLConfigVersion.cmake
				VERSION ${OPENSSL_VERSION}
				COMPATIBILITY SameMajorVersion
		)
		set(OpenSSL_ROOT "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}")
		add_library(OpenSSL::Crypto ${OPENSSL_LIBRARY_TYPE} IMPORTED GLOBAL)
		add_library(OpenSSL::SSL ${OPENSSL_LIBRARY_TYPE} IMPORTED GLOBAL)

		# Some cmake distributions will error if the directory passed to INTERFACE_INCLUDE_DIRECTORIES is non-existent
		file(MAKE_DIRECTORY "${OPENSSL_INCLUDE_DIR}")

		set_target_properties(
			OpenSSL::Crypto
			PROPERTIES
				IMPORTED_LOCATION ${OPENSSL_CRYPTO_IMPORTED_LOCATION}
				INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
		)
		set_target_properties(
			OpenSSL::SSL
			PROPERTIES
				IMPORTED_LOCATION ${OPENSSL_SSL_IMPORTED_LOCATION}
				INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
				IMPORTED_LINK_INTERFACE_LIBRARIES OpenSSL::Crypto
		)

		add_dependencies(OpenSSL::SSL openssl-ep)
		add_dependencies(OpenSSL::Crypto openssl-ep)
	endfunction()
	add_openssl()
endif()

# Depends on mbedtls or openssl so add it after
if(BUILD_LIBSRTP2)
	function(add_srtp)
		set(BUILD_SHARED_LIBS ${BUILD_LIBSRTP2_SHARED_LIBS})
		set(TEST_APPS OFF)

		add_subdirectory("external/srtp")
		add_dependencies(sdk srtp2)
	endfunction()
	add_srtp()
endif()

if(BUILD_OPENCORE_AMR)
	function(add_opencore_amr)
		set(BUILD_SHARED_LIBS ${BUILD_OPENCORE_AMR_SHARED_LIBS})
		if(ENABLE_AMRNB)
			set(ENABLE_AMRNB_ENCODER ON)
			set(ENABLE_AMRNB_DECODER ON)
		else()
			set(ENABLE_AMRNB_ENCODER OFF)
			set(ENABLE_AMRNB_DECODER OFF)
		endif()
		if(ENABLE_AMRWB)
			set(ENABLE_AMRWB_DECODER ON)
		else()
			set(ENABLE_AMRWB_DECODER OFF)
		endif()

		add_subdirectory("external/opencore-amr")
		if(ENABLE_AMRNB)
			add_dependencies(sdk opencore-amrnb)
		endif()
		if(ENABLE_AMRWB)
			add_dependencies(sdk opencore-amrwb)
		endif()
	endfunction()
	add_opencore_amr()
endif()

if(BUILD_OPENH264)
	function(add_openh264)
		find_program(NASM_PROGRAM
			NAMES nasm nasm.exe
		)
		if(NOT NASM_PROGRAM)
			if(WIN32)
				message(FATAL_ERROR "Could not find the nasm.exe program. Please install it from http://www.nasm.us/")
			else()
				message(FATAL_ERROR "Could not find the nasm program.")
			endif()
		endif()

		include("${PROJECT_BINARY_DIR}/Autotools/Autotools.cmake")

		set(EP_BUILD_DIR "${PROJECT_BINARY_DIR}/external/openh264")
		set(EP_INSTALL_TARGET "install")
		set(EP_CROSS_COMPILATION_OPTIONS)
		set(EP_MAKE_OPTIONS)
		set(EP_ASFLAGS)
		set(EP_CPPFLAGS)
		set(EP_CFLAGS)
		set(EP_CXXFLAGS)
		set(EP_OBJCFLAGS)
		set(EP_LDFLAGS)

		set(EP_BUILD_TYPE "Release") # Always use Release build type, otherwise the codec is too slow...
		if(WIN32)
			set(EP_LINKING_TYPE "shared")
		else()
			if(ENABLE_EMBEDDED_OPENH264)
				set(EP_LINKING_TYPE "static")
			else()
				set(EP_LINKING_TYPE "shared")
			endif()
		endif()
		if(WIN32)
			if(MSVC)
				if(CMAKE_SIZEOF_VOID_P EQUAL 8)
					set(EP_ADDITIONAL_MAKE_OPTIONS "OS=\"msvc\" ARCH=\"x86_64\"")
				else()
					set(EP_ADDITIONAL_MAKE_OPTIONS "OS=\"msvc\" ARCH=\"i386\"")
				endif()
			endif()
		elseif(ANDROID)
			# Temporary fix for 4.3 release. The ANDROID_PLATFORM_LEVEL (and derivatives...) aren't well passed here, so we hardcode it
			# (Waiting for proper fix in our android toolchain wrapper to be merged)
			if(CMAKE_ANDROID_NDK_VERSION VERSION_LESS 19 AND CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7-a")
				set(ANDROID_PLATFORM_LEVEL "17")
				set(ANDROID_PLATFORM "android-17")
				set(CMAKE_ANDROID_API "17")
				set(NDK_TOOLCHAIN_VERSION "gcc")
			else()
				set(ANDROID_PLATFORM_LEVEL "21")
				set(ANDROID_PLATFORM "android-21")
				set(CMAKE_ANDROID_API "21")
				set(NDK_TOOLCHAIN_VERSION "clang")
			endif()
			set(EP_ADDITIONAL_MAKE_OPTIONS "TOOLCHAINPREFIX=\"${ANDROID_TOOLCHAIN_PREFIX}\" OS=\"android\" NDKROOT=\"${CMAKE_ANDROID_NDK}\" NDKLEVEL=${ANDROID_PLATFORM_LEVEL} TARGET=\"android-${CMAKE_ANDROID_API}\" NDK_TOOLCHAIN_VERSION=${NDK_TOOLCHAIN_VERSION}")
			if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7-a")
				set(EP_ADDITIONAL_MAKE_OPTIONS "${EP_ADDITIONAL_MAKE_OPTIONS} ARCH=\"arm\" INCLUDE_PREFIX=\"arm-linux-androideabi\"")
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
				set(EP_ADDITIONAL_MAKE_OPTIONS "${EP_ADDITIONAL_MAKE_OPTIONS} ARCH=\"arm64\" INCLUDE_PREFIX=\"aarch64-linux-android\"")
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
				set(EP_ADDITIONAL_MAKE_OPTIONS "${EP_ADDITIONAL_MAKE_OPTIONS} ARCH=\"x86_64\" ENABLEPIC=\"Yes\" INCLUDE_PREFIX=\"i686-linux-android\"")
			else()
				set(EP_ADDITIONAL_MAKE_OPTIONS "${EP_ADDITIONAL_MAKE_OPTIONS} ARCH=\"x86\" ENABLEPIC=\"Yes\" INCLUDE_PREFIX=\"i686-linux-android\"")
			endif()
		elseif(APPLE)
			if(IOS)
				if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
					set(EP_ADDITIONAL_MAKE_OPTIONS "OS=\"ios\" ARCH=\"arm64\"")
					# XCode7 allows bitcode
					if(NOT ${XCODE_VERSION} VERSION_LESS 7)
						#set(EP_CFLAGS "-fembed-bitcode")
					endif()
				elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7")
					set(EP_ADDITIONAL_MAKE_OPTIONS "OS=\"ios\" ARCH=\"armv7\"")
					# XCode7 allows bitcode
					if (NOT ${XCODE_VERSION} VERSION_LESS 7)
						#set(EP_CFLAGS "-fembed-bitcode")
					endif()
				elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
					set(EP_ADDITIONAL_MAKE_OPTIONS "OS=\"ios\" ARCH=\"x86_64\"")
				else()
					set(EP_ADDITIONAL_MAKE_OPTIONS "OS=\"ios\" ARCH=\"i386\"")
				endif()
			else()
				set(MAC_ARCH "--target=${CMAKE_C_COMPILER_TARGET}")
				set(FLAGS "${MAC_ARCH} -isysroot ${CMAKE_OSX_SYSROOT} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
				if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
					set(EP_ADDITIONAL_MAKE_OPTIONS "ARCH=\"x86_64\"")
				elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
					set(EP_ADDITIONAL_MAKE_OPTIONS "ARCH=\"arm64\"")
				else()
					set(EP_ADDITIONAL_MAKE_OPTIONS "ARCH=\"x86\"")
				endif()
				set(EP_CFLAGS "${FLAGS}")
				set(EP_CXXFLAGS "${FLAGS}")
				set(EP_CPPFLAGS "${FLAGS}")
				set(EP_LDFLAGS "${MAC_ARCH} -L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib/") # -L: Hack for clang to find libc++ on "Macos Big Sur"
			endif()
		endif()

		get_relative_source_path("${PROJECT_SOURCE_DIR}/external/openh264" "${EP_BUILD_DIR}" "EP_SOURCE_DIR_RELATIVE_TO_BUILD_DIR")
		set(EP_MAKEFILE_PATH "-f \"${EP_SOURCE_DIR_RELATIVE_TO_BUILD_DIR}/Makefile\"")
		set(EP_BUILD_TARGET "libraries")
		set(EP_MAKE_OPTIONS "PREFIX=\"${CMAKE_INSTALL_PREFIX}\" BUILDTYPE=\"${EP_BUILD_TYPE}\" ASM=\"${NASM_PROGRAM}\" ${EP_ADDITIONAL_MAKE_OPTIONS}")
		configure_file("cmake/Autotools/build.sh.cmake" "${PROJECT_BINARY_DIR}/EP_openh264_build.sh")

		set(EP_INSTALL_TARGET "install-${EP_LINKING_TYPE}")
		set(EP_MAKE_OPTIONS "DESTDIR=\"${CMAKE_INSTALL_PREFIX}/\" PREFIX=\"\" SHAREDLIB_DIR=\"${CMAKE_INSTALL_LIBDIR}\" BUILDTYPE=\"${EP_BUILD_TYPE}\" ${EP_ADDITIONAL_MAKE_OPTIONS}")
		configure_file("cmake/Autotools/install.sh.cmake" "${PROJECT_BINARY_DIR}/EP_openh264_install.sh")

		if(EP_LINKING_TYPE STREQUAL "shared")
			if(APPLE)
				set(EP_LIBRARY_NAME "libopenh264.dylib")
			elseif(WIN32)
				set(EP_LIBRARY_NAME "openh264_dll.lib")
			else()
				set(EP_LIBRARY_NAME "libopenh264.so")
			endif()
		else()
			set(EP_LIBRARY_NAME "libopenh264.a")
		endif()
		file(MAKE_DIRECTORY "${EP_BUILD_DIR}")
		ExternalProject_Add(openh264
			SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/openh264"
			CONFIGURE_COMMAND ""
			BUILD_COMMAND "sh" "${PROJECT_BINARY_DIR}/EP_openh264_build.sh"
			INSTALL_COMMAND "sh" "${PROJECT_BINARY_DIR}/EP_openh264_install.sh"
			CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
			LOG_BUILD TRUE
			LOG_INSTALL TRUE
			LOG_OUTPUT_ON_FAILURE TRUE
			BUILD_BYPRODUCTS "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/${EP_LIBRARY_NAME}"
		)
		file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/include")
		add_library(libopenh264 UNKNOWN IMPORTED)
		set_target_properties(libopenh264 PROPERTIES IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/${EP_LIBRARY_NAME}" INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/include")
		add_dependencies(libopenh264 openh264)
		add_dependencies(sdk libopenh264)
	endfunction()
	add_openh264()
endif()

if(BUILD_OPENLDAP)
	function(add_openldap)
		set(BUILD_SHARED_LIBS ${BUILD_OPENLDAP_SHARED_LIBS})

		if(WIN32)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /sdl-")
		endif()
		add_subdirectory("external/openldap")
		add_dependencies(sdk ldap)
	endfunction()
	add_openldap()
endif()

if(BUILD_OPUS)
	function(add_opus)
		set(LINPHONESDK_INTEGRATED_BUILD ON)
		if(WIN32)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W0")
		else()
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
		endif()
		if(ANDROID OR IOS OR UWP)
			set(OPUS_FIXED_POINT ON)
		endif()
		set(BUILD_SHARED_LIBS ${BUILD_OPUS_SHARED_LIBS})

		set(CMAKE_POLICY_DEFAULT_CMP0077 NEW) # Prevent project from overriding the options we just set here
		add_subdirectory("external/opus")
		add_dependencies(sdk opus)
	endfunction()
	add_opus()
endif()

# SOCI may depend on sqlite3 so add it before
if(BUILD_SQLITE3)
	function(add_sqlite3)
		set(BUILD_SHARED_LIBS ${BUILD_SQLITE3_SHARED_LIBS})

		add_subdirectory("external/sqlite3")
		add_dependencies(sdk sqlite3)
	endfunction()
	add_sqlite3()
endif()

if(BUILD_SOCI)
	function(add_soci)
		if(BUILD_SOCI_SHARED_LIBS)
			set(SOCI_SHARED ON)
			set(SOCI_STATIC OFF)
		else()
			set(SOCI_SHARED OFF)
			set(SOCI_STATIC ON)
		endif()
		set(SOCI_INSTALL_BACKEND_TARGETS OFF)
		set(SOCI_TESTS OFF)
		if(LINPHONESDK_BUILD_TYPE STREQUAL "Flexisip")
			set(SOCI_FRAMEWORK OFF)
		endif()
		foreach(_BACKEND ${BUILD_SOCI_BACKENDS})
			string(TOUPPER "${_BACKEND}" _BACKEND)
			set(WITH_${_BACKEND} ON)
		endforeach()

		add_subdirectory("external/soci")
		if(BUILD_SOCI_SHARED_LIBS)
			add_dependencies(sdk soci_core)
		else()
			add_dependencies(sdk soci_core_static)
		endif()
	endfunction()
	add_soci()
endif()

if(BUILD_SPEEX)
	function(add_speex)
		set(BUILD_SHARED_LIBS ${BUILD_SPEEX_SHARED_LIBS})

		add_subdirectory("external/speex")
		add_dependencies(sdk speex)
	endfunction()
	add_speex()
endif()

if(BUILD_VO_AMRWBENC)
	function(add_vo_amrwbenc)
		set(BUILD_SHARED_LIBS ${BUILD_VO_AMRWBENC_SHARED_LIBS})
		add_subdirectory("external/vo-amrwbenc")
		add_dependencies(sdk vo-amrwbenc)
	endfunction()
	add_vo_amrwbenc()
endif()

if(BUILD_XERCESC)
	function(add_xercesc)
		set(BUILD_SHARED_LIBS ${BUILD_XERCESC_SHARED_LIBS})

		add_subdirectory("external/xerces-c")
		add_dependencies(sdk xerces-c)
	endfunction()
	add_xercesc()
endif()

if(BUILD_ZLIB)
	function(add_zlib)
		set(BUILD_SHARED_LIBS ${BUILD_ZLIB_SHARED_LIBS})

		add_subdirectory("external/zlib")
		add_dependencies(sdk zlib)
	endfunction()
	add_zlib()
endif()

if(BUILD_ZXINGCPP)
	function(add_zxingcpp)
		set(BUILD_SHARED_LIBS ${BUILD_ZXINGCPP_SHARED_LIBS})

		add_subdirectory("external/zxing-cpp")
		add_dependencies(sdk ZXing)
	endfunction()
	add_zxingcpp()
endif()
