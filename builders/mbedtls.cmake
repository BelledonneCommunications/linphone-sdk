############################################################################
# mbedtls.cmake
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

set(EP_mbedtls_GIT_REPOSITORY "git://git.linphone.org/mbedtls.git" CACHE STRING "mbedtls repository URL")
set(EP_mbedtls_GIT_TAG_LATEST "linphone" CACHE STRING "mbedtls tag to use when compiling latest version")
set(EP_mbedtls_GIT_TAG "3b88f2749d59e5346de08e121fba1d797c55ddaa" CACHE STRING "mbedtls tag to use")
set(EP_mbedtls_EXTERNAL_SOURCE_PATHS "mbedtls" "externals/mbedtls")

set(EP_mbedtls_LINKING_TYPE "-DUSE_STATIC_MBEDTLS_LIBRARY=NO" "-DUSE_SHARED_MBEDTLS_LIBRARY=YES")
set(EP_mbedtls_CMAKE_OPTIONS "-DENABLE_PROGRAMS=NO" "-DENABLE_TESTING=NO")
if(MSVC)
	set(EP_mbedtls_EXTRA_CFLAGS "-DMBEDTLS_PLATFORM_SNPRINTF_MACRO=_snprintf -DMBEDTLS_EXPORTS")
	set(EP_mbedtls_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
