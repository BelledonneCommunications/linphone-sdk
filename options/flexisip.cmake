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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

# Flexisip build options

option(ENABLE_ODB "Enable odb support." ${DEFAULT_VALUE_ENABLE_ODB})
add_feature_info("ODB" ENABLE_ODB "Enable odb support.")

option(ENABLE_ODBC "Enable odbc support." ${DEFAULT_VALUE_ENABLE_ODBC})
add_feature_info("ODBC" ENABLE_ODBC "Enable odbc support.")

option(ENABLE_REDIS "Enable hiredis support." ${DEFAULT_VALUE_ENABLE_REDIS})
add_feature_info("REDIS" ENABLE_REDIS "Enable hiredis support.")


option(ENABLE_PUSHNOTIFICATION "Enable push notification support." ${DEFAULT_VALUE_ENABLE_PUSHNOTIFICATION})
add_feature_info("PUSHNOTIFICATION" ENABLE_PUSHNOTIFICATION "Enable push notification support.")
