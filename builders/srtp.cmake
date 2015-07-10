############################################################################
# srtp.cmake
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

set(EP_srtp_GIT_REPOSITORY "git://git.linphone.org/srtp.git")
set(EP_srtp_GIT_TAG_LATEST "master")
set(EP_srtp_GIT_TAG "4caa4843b4bfc54be596d309b5d00a81a35e3276")
set(EP_srtp_EXTERNAL_SOURCE_PATHS "srtp" "externals/srtp")

set(EP_srtp_LINKING_TYPE "${DEFAULT_VALUE_CMAKE_LINKING_TYPE}")
if(MSVC)
	set(EP_srtp_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
