############################################################################
# sofiasip.cmake
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

lcb_git_repository("git://git.linphone.org/sofia-sip.git")
lcb_git_tag_latest("bc")
lcb_git_tag("dcdc8efab5d164ec55c8706f978a827af04459e4")
lcb_external_source_paths("externals/sofia-sip")

lcb_build_method("autotools")
lcb_use_autogen(YES)
lcb_build_in_source_tree(YES)
lcb_linking_type("--disable-static" "--enable-shared")

if (ENABLE_MDNS)
	lcb_configure_options("--enable-mdns")
endif()


lcb_cross_compilation_options(
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)

lcb_spec_file("packages/sofia-sip-*.spec")
lcb_rpmbuild_name("sofia-sip")
lcb_rpmbuild_options(
	"--with bc"
	"--without glib"
)
lcb_use_autotools_for_rpm(YES)
