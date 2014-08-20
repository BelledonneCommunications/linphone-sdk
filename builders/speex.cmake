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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

set(EP_speex_GIT_REPOSITORY "git://git.linphone.org/speex.git")
if(LINPHONE_BUILDER_LATEST)
	set(EP_speex_GIT_TAG "linphone")
else()
	set(EP_speex_GIT_TAG "d472c4c38a3105c4d0ca574547c2c4865491df1d")
endif()
set(EP_speex_LINKING_TYPE "-DENABLE_STATIC=0")
if(MSVC)
	set(EP_speex_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
