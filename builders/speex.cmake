############################################################################
# speex.cmake
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

lcb_git_repository("git://git.linphone.org/speex.git")
lcb_git_tag_latest("linphone")
lcb_git_tag("fc1dd43c3c9d244bca1c300e408ce0373dbd5ed8")
lcb_external_source_paths("speex" "externals/speex")
lcb_may_be_found_on_system(YES)
lcb_ignore_warnings(YES)

lcb_cmake_options(
	"-DENABLE_SPEEX_DSP=YES"
	"-DENABLE_SPEEX_CODEC=${ENABLE_SPEEX}"
)
