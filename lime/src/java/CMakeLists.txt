############################################################################
# CMakeLists.txt
# Copyright (C) 2019  Belledonne Communications, Grenoble France
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
find_package(Java REQUIRED)
include(UseJava)

set (LIME_SOURCE_FILES_JAVA
	org/linphone/lime/LimeException.java
	org/linphone/lime/LimeCurveId.java
	org/linphone/lime/LimeEncryptionPolicy.java
	org/linphone/lime/LimeOutputBuffer.java
	org/linphone/lime/LimePeerDeviceStatus.java
	org/linphone/lime/LimeCallbackReturn.java
	org/linphone/lime/LimeStatusCallback.java
	org/linphone/lime/RecipientData.java
	org/linphone/lime/LimePostToX3DH.java
	org/linphone/lime/LimeManager.java
)

add_jar(Lime ${LIME_SOURCE_FILES_JAVA})

get_target_property(Lime_jarFile Lime JAR_FILE)

message(STATUS "Lime Jar file ${Lime_jarFile}")

