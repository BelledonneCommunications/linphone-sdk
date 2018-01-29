############################################################################
# config-flexisip-rpm.cmake
# Copyright (C) 2014-2018  Belledonne Communications, Grenoble France
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

include(GNUInstallDirs)
include(${CMAKE_SOURCE_DIR}/cmake/FindLinuxPlatform.cmake)

# Check if we have everything to compile correctly

function(CHECK_LIBRARY libname)
	find_library(${libname}_LIBRARY
		NAMES ${libname}
		PATHS /usr/lib/mysql/
	)
	if(NOT ${libname}_LIBRARY)
		message(FATAL_ERROR "Could not find the ${libname} library, which is needed for RPMBuild of flexisip")
	else()
		message(STATUS "Found ${libname} : ${${libname}_LIBRARY}.")
	endif()
endfunction()

set(FLEXISIP_LIBDEPS ssl mysqlclient_r mysqlclient)

foreach(LIBNAME ${FLEXISIP_LIBDEPS})
	check_library(${LIBNAME})
endforeach()

# Force build of RPM packages
set(LINPHONE_BUILDER_ENABLE_RPM_PACKAGING YES CACHE BOOL "" FORCE)

# Force use of system dependencies to build RPM packages
set(LINPHONE_BUILDER_USE_SYSTEM_DEPENDENCIES YES CACHE BOOL "" FORCE)

# Define default values for the flexisip builder options
set(DEFAULT_VALUE_ENABLE_CXX_WRAPPER ON)
set(DEFAULT_VALUE_ENABLE_PUSHNOTIFICATION ON)
set(DEFAULT_VALUE_ENABLE_REDIS ON)
set(DEFAULT_VALUE_ENABLE_SOCI ON)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS OFF)
set(ENABLE_SRTP NO CACHE BOOL "" FORCE) # Mainly to avoid issues with old libsrtp (sha1_update conflict with polarssl)

set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_SHARED=YES" "-DENABLE_STATIC=NO")

# Global configuration
set(LINPHONE_BUILDER_HOST "")
set(RPM_INSTALL_PREFIX "/opt/belledonne-communications")
set(LINPHONE_BUILDER_RPMBUILD_PACKAGE_PREFIX "bc-")

# Adjust PKG_CONFIG_PATH to include install directory
if(UNIX)
	set(LINPHONE_BUILDER_PKG_CONFIG_PATH "${RPM_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig/:${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig/:$ENV{PKG_CONFIG_PATH}:/usr/${CMAKE_INSTALL_LIBDIR}/pkgconfig/:/usr/${CMAKE_INSTALL_LIBDIR}/x86_64-linux-gnu/pkgconfig/:/usr/share/pkgconfig/:/usr/local/${CMAKE_INSTALL_LIBDIR}/pkgconfig/:/opt/local/${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
	message(STATUS "PKG CONFIG PATH: ${LINPHONE_BUILDER_PKG_CONFIG_PATH}")
	message(STATUS "LIBDIR: ${LIBDIR}")
endif()


# Include builders
include(builders/CMakeLists.txt)

lcb_builder_cmake_options(soci "-DWITH_POSTGRESQL=YES")
lcb_builder_rpmbuild_options(soci
	"--with bc"
	"--with sqlite3"
	"--with mysql"
	"--with postgresql"
)

lcb_builder_cmake_options(linphone
	"-DENABLE_CONSOLE_UI=NO"
	"-DENABLE_DAEMON=NO"
)

# prepare the RPMBUILD options that we need to pass
set(LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS "--define '_mandir %{_prefix}'")
if(PLATFORM STREQUAL "Debian")
	# dependencies cannot be checked by rpmbuild in debian
	set(LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS "${LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS} --nodeps")

	# dist is not defined in debian for rpmbuild..
	set(LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS "${LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS} --define 'dist .deb'")

	# debian has multi-arch lib dir instead of lib and lib64
	set(LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS "${LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS} --define '_lib lib'")

	# debian has multi-arch lib dir instead of lib and lib64
	set(LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS "${LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS} --define '_libdir %{_prefix}/%{_lib}'")

	# some debians are using dash as shell, which doesn't support "export -n", so we override and use bash
	set(LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS "${LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS} --define '_buildshell /bin/bash'")
endif()
