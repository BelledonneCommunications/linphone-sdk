############################################################################
# config-flexisip-rpm.cmake
# Copyright (C) 2014  Belledonne Communications, Grenoble France
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

# Force use of system dependencies to build RPM packages
set(LINPHONE_BUILDER_USE_SYSTEM_DEPENDENCIES YES CACHE BOOL "" FORCE)

# Define default values for the flexisip builder options
set(DEFAULT_VALUE_DISABLE_BC_ANTLR ON)
set(DEFAULT_VALUE_ENABLE_ODBC OFF)
set(DEFAULT_VALUE_ENABLE_PUSHNOTIFICATION ON)
set(DEFAULT_VALUE_ENABLE_REDIS ON)
set(DEFAULT_VALUE_ENABLE_SOCI ON)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS OFF)
set(DEFAULT_VALUE_ENABLE_BC_HIREDIS ON)

set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_STATIC=NO")

# Global configuration
set(LINPHONE_BUILDER_HOST "")
set(RPM_INSTALL_PREFIX "/opt/belledonne-communications")

# Adjust PKG_CONFIG_PATH to include install directory
if(UNIX)
	set(LINPHONE_BUILDER_PKG_CONFIG_PATH "${RPM_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig/:${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig/:$ENV{PKG_CONFIG_PATH}:/usr/${CMAKE_INSTALL_LIBDIR}/pkgconfig/:/usr/${CMAKE_INSTALL_LIBDIR}/x86_64-linux-gnu/pkgconfig/:/usr/share/pkgconfig/:/usr/local/${CMAKE_INSTALL_LIBDIR}/pkgconfig/:/opt/local/${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
	message(STATUS "PKG CONFIG PATH: ${LINPHONE_BUILDER_PKG_CONFIG_PATH}")
	message(STATUS "LIBDIR: ${LIBDIR}")
else() # Windows
	set(LINPHONE_BUILDER_PKG_CONFIG_PATH "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/")
endif()


# we can override the bctoolbox build method before including builders because it doesn't define it.
lcb_builder_build_method(bctoolbox "rpm")
lcb_builder_cmake_options(bctoolbox "-DENABLE_TESTS=NO")
lcb_builder_cmake_options(bctoolbox "-DENABLE_TESTS_COMPONENT=NO")
lcb_builder_rpmbuild_options(bctoolbox
	"--with bc"
	"--without tests_component"
)

# Include builders
include(builders/CMakeLists.txt)

lcb_builder_build_method(soci "rpm")
lcb_builder_rpmbuild_options(soci
	"--with bc"
	"--with sqlite3"
	"--with mysql"
)

lcb_builder_build_method(ortp "rpm")
lcb_builder_rpmbuild_options(ortp "--with bc")

lcb_builder_build_method(ms2 "rpm")
lcb_builder_cmake_options(ms2 "-DENABLE_SRTP=NO") #mainly to avoid issue with old libsrtp (sha1_update conflict with polarssl)
lcb_builder_rpmbuild_options(ms2
	"--with bc"
	"--without video"
	"--without srtp"
)

lcb_builder_build_method(belr "rpm")
lcb_builder_rpmbuild_options(belr "--with bc")

lcb_builder_build_method(bellesip "rpm")
lcb_builder_rpmbuild_options(bellesip "--with bc")

lcb_builder_build_method(linphone "rpm")
lcb_builder_rpmbuild_options(linphone
	"--with bc"
	"--with soci"
	"--without lime"
	"--without video"
)

lcb_builder_build_method(sofiasip "rpm")
lcb_builder_rpmbuild_options(sofiasip
	"--with bc"
	"--without glib"
)

lcb_builder_build_method(odb "custom")

lcb_builder_build_method(flexisip "rpm")
lcb_builder_rpmbuild_options(flexisip
	"--with bc"
	"--with push"
)

lcb_builder_build_method(hiredis "rpm")
lcb_builder_rpmbuild_options(hiredis "--with bc")

if(NOT ENABLE_TRANSCODER)
	lcb_builder_rpmbuild_options(flexisip "--without transcoder")
endif()
if(NOT ENABLE_SOCI)
	lcb_builder_rpmbuild_options(flexisip "--without soci")
endif()

if(ENABLE_PRESENCE)
	lcb_builder_rpmbuild_options(flexisip "--with presence")
endif()
if(ENABLE_CONFERENCE)
	lcb_builder_rpmbuild_options(flexisip "--with conference")
endif()

if(ENABLE_SNMP)
	lcb_builder_rpmbuild_options(flexisip "--with snmp")
endif()

if(ENABLE_BC_ODBC)
	lcb_builder_build_method(unixodbc "rpm")
	lcb_builder_rpmbuild_options(unixodbc "--with bc")

	lcb_builder_build_method(myodbc "rpm")
	lcb_builder_rpmbuild_options(myodbc "--with bc")
	lcb_builder_configure_options(myodbc "--with-unixODBC=${RPM_INSTALL_PREFIX}")

	lcb_builder_rpmbuild_options(flexisip "--with bcodbc")
	lcb_builder_configure_options(flexisip "--with-odbc=${RPM_INSTALL_PREFIX}")
endif()

set(LINPHONE_BUILDER_RPMBUILD_PACKAGE_PREFIX "bc-")

# prepare the RPMBUILD options that we need to pass
set(GLOBAL_RPMBUILD_OPTIONS "--define '_mandir %{_prefix}'")
if(PLATFORM STREQUAL "Debian")
	# dependencies cannot be checked by rpmbuild in debian
	set(GLOBAL_RPMBUILD_OPTIONS "${GLOBAL_RPMBUILD_OPTIONS} --nodeps")

	# dist is not defined in debian for rpmbuild..
	set(GLOBAL_RPMBUILD_OPTIONS "${GLOBAL_RPMBUILD_OPTIONS} --define 'dist .deb'")

	# debian has multi-arch lib dir instead of lib and lib64
	set(GLOBAL_RPMBUILD_OPTIONS "${GLOBAL_RPMBUILD_OPTIONS} --define '_lib lib'")

	# debian has multi-arch lib dir instead of lib and lib64
	set(GLOBAL_RPMBUILD_OPTIONS "${GLOBAL_RPMBUILD_OPTIONS} --define '_libdir %{_prefix}/%{_lib}'")

	# some debians are using dash as shell, which doesn't support "export -n", so we override and use bash
	set(GLOBAL_RPMBUILD_OPTIONS "${GLOBAL_RPMBUILD_OPTIONS} --define '_buildshell /bin/bash'")
endif()

set(LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTIONS ${GLOBAL_RPMBUILD_OPTIONS})
