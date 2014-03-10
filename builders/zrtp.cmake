############################################################################
# zrtp.cmake
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

set(EP_zrtp_GIT_REPOSITORY "https://github.com/wernerd/ZRTPCPP.git")
set(EP_zrtp_GIT_TAG "761339643ee278e1f532713b959c6bb212040206") # Branch 'master'
set(EP_zrtp_CMAKE_OPTIONS "-DCORE_LIB=1 -DSDES=0")
set(EP_zrtp_LINKING_TYPE "-DENABLE_STATIC=0")
if(MSVC)
	set(EP_zrtp_EXTRA_LDFLAGS "/SAFESEH:NO")
endif(MSVC)

