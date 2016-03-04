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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

# Linphone build options

option(ENABLE_NLS "Enable internationalization of Linphone and Liblinphone." ON)
add_feature_info("NLS" ENABLE_NLS "Enable internationalization of Linphone and Liblinphone. (Only for the desktop target)")
option(ENABLE_VCARD "Enable vCard 4 support in Linphone and Liblinphone." ${DEFAULT_VALUE_ENABLE_VCARD})
add_feature_info("VCARD" ENABLE_VCARD "Enable vCard 4 support Linphone and Liblinphone.")

if(UNIX AND NOT IOS)
	option(ENABLE_RELATIVE_PREFIX "liblinphone and mediastreamer will look for their respective ressources relatively to their location." OFF)
	add_feature_info("Relative prefix" ENABLE_RELATIVE_PREFIX "liblinphone and mediastreamer will look for their respective ressources relatively to their location.")
endif()
