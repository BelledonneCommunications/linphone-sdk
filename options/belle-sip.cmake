############################################################################
# belle-sip.cmake
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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

# belle-sip build options

lcb_add_option("Tunnel" "Secure tunnel for SIP/RTP (require license)." "${DEFAULT_VALUE_ENABLE_TUNNEL}")
lcb_add_option("RTP Map always in SDP" "Always include rtpmap in SDP." OFF)

option(DISABLE_BC_ANTLR "Do not build Antlr as part of the CMake builder, and instead use the system version" "${DEFAULT_VALUE_DISABLE_BC_ANTLR}")
add_feature_info("Belledonne Antlr" DISABLE_BC_ANTLR "Do not build the Belledonne version of Antlr3c")
