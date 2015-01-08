############################################################################
# tunnel.cmake
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

set(EP_tunnel_GIT_REPOSITORY "gitosis@git.linphone.org:tunnel.git") # Private repository
set(EP_tunnel_GIT_TAG_LATEST "master")
set(EP_tunnel_GIT_TAG "1182be7fdefcf9717451fc2a8f67f645c5e4d2c9")

set(EP_tunnel_LINKING_TYPE "-DENABLE_STATIC=0")
set(EP_tunnel_CMAKE_OPTIONS "-DENABLE_SERVERS=0")
set(EP_tunnel_DEPENDENCIES EP_polarssl)
