############################################################################
# v4l.cmake
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

lcb_git_repository("git://linuxtv.org/v4l-utils.git")
lcb_git_tag_latest("master")
lcb_git_tag("v4l-utils-1.0.0")
lcb_external_source_paths("externals/v4l-utils")
lcb_may_be_found_on_system(YES)
lcb_ignore_warnings(YES)

lcb_build_method("autotools")
lcb_use_autoreconf(YES)
lcb_cross_compilation_options(
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
lcb_configure_options(
	"--disable-v4l-utils"
	"--disable-qv4l2"
	"--disable-libdvbv5"
	"--with-udevdir=${CMAKE_INSTALL_PREFIX}/etc"
	"--without-jpeg"
)
lcb_linking_type("--disable-static" "--enable-shared")
