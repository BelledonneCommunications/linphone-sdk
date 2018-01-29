############################################################################
# flexisip.cmake
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

# Flexisip build options

lcb_add_option("REDIS" "Enable hiredis support." "${DEFAULT_VALUE_ENABLE_REDIS}")
lcb_add_option("PushNotification" "Enable push notification support." "${DEFAULT_VALUE_ENABLE_PUSHNOTIFICATION}")
lcb_add_option("Presence" "Enable presence server support." "${DEFAULT_VALUE_ENABLE_PRESENCE}")
lcb_add_option("Conference" "Enable conference server support." "${DEFAULT_VALUE_ENABLE_CONFERENCE}")
lcb_add_option("SNMP" "Enable SNMP support." "${DEFAULT_ENABLE_SNMP}")
lcb_add_option("Transcoder" "Enable transcoder support." "${DEFAULT_ENABLE_TRANSCODER}")
lcb_add_option("PROTOBUF" "Enable protobuf for REDIS." "${DEFAULT_ENABLE_PROTOBUF}" "ENABLE_PROTOBUF" OFF)
