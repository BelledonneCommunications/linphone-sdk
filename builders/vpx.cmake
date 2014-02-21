############################################################################
# vpx.cmake
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

set(EP_vpx_GIT_REPOSITORY "https://chromium.googlesource.com/webm/libvpx")
set(EP_vpx_GIT_TAG "v1.3.0")
set(EP_vpx_AUTOTOOLS "yes")
set(EP_vpx_CONFIGURE_OPTIONS "--enable-pic --enable-error-concealment --enable-realtime-only --enable-spatial-resampling --enable-vp8 --disable-vp9 --enable-libs --disable-install-docs --disable-debug-libs --disable-examples --disable-unit-tests")
set(EP_vpx_LINKING_TYPE "--enable-static --disable-shared")

if(APPLE)
	set(EP_vpx_TARGET "x86-darwin10-gcc")
else(APPLE)
	set(EP_vpx_TARGET "generic-gnu")
endif(APPLE)
