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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

lcb_git_repository("git://git.linphone.org/mbedtls.git")
lcb_git_tag_latest("linphone")
lcb_git_tag("3b88f2749d59e5346de08e121fba1d797c55ddaa")
lcb_external_source_paths("mbedtls" "externals/mbedtls")
lcb_may_be_found_on_system(YES)
lcb_ignore_warnings(YES)

lcb_linking_type("-DUSE_STATIC_MBEDTLS_LIBRARY=NO" "-DUSE_SHARED_MBEDTLS_LIBRARY=YES")
lcb_cmake_options("-DENABLE_PROGRAMS=NO" "-DENABLE_TESTING=NO")
if(MSVC)
	lcb_extra_cflags("-DMBEDTLS_PLATFORM_SNPRINTF_MACRO=_snprintf -DMBEDTLS_EXPORTS")
endif()
