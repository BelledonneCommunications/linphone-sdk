############################################################################
# linphone.cmake
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

# Linphone build options

lcb_add_option("GTK UI" "Enable the GTK user interface of Linphone." "${DEFAULT_VALUE_ENABLE_GTK_UI}")
lcb_add_option("NLS" "Enable internationalization of Linphone and Liblinphone." ON)
lcb_add_option("VCARD" "Enable vCard 4 support in Linphone and Liblinphone." "${DEFAULT_VALUE_ENABLE_VCARD}")

if(UNIX AND NOT IOS)
	lcb_add_option("Relative prefix" "liblinphone and mediastreamer will look for their respective ressources relatively to their location." OFF)
endif()
