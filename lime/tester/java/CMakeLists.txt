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

set (LIMETESTER_SOURCE_FILES_JAVA
	org/linphone/limeTester/LimeTesterUtils.java
	org/linphone/limeTester/HelloWorld.java
	org/linphone/limeTester/LimeLimeTester.java
	org/linphone/limeTester/LimeTester.java
)

set (LIME_JAR_FILE ${PROJECT_BINARY_DIR}/src/java/Lime.jar)
message(STATUS "Included Lime Jar file ${LIME_JAR_FILE}")

add_jar(LimeTester ${LIMETESTER_SOURCE_FILES_JAVA} INCLUDE_JARS ${LIME_JAR_FILE})

get_target_property(Tester_jarFile LimeTester JAR_FILE)

configure_file(../data/pattern_getSelfIk.C25519.sqlite3 ${CMAKE_CURRENT_BINARY_DIR}/pattern_getSelfIk.C25519.sqlite3 COPYONLY)
configure_file(../data/pattern_getSelfIk.C448.sqlite3 ${CMAKE_CURRENT_BINARY_DIR}/pattern_getSelfIk.C448.sqlite3 COPYONLY)

if (ENABLE_CURVE25519)
	set (JAVA_CURVE25519_OPTION -DenableC25519=true)
else ()
	set (JAVA_CURVE25519_OPTION -DenableC25519=false)
endif()

if (ENABLE_CURVE448)
	set (JAVA_CURVE448_OPTION -DenableC448=true)
else ()
	set (JAVA_CURVE448_OPTION -DenableC448=false)
endif()

add_test(NAME JNI COMMAND ${Java_JAVA_EXECUTABLE} ${JAVA_CURVE25519_OPTION} ${JAVA_CURVE448_OPTION} -Djava.library.path=${CMAKE_BINARY_DIR}/src -cp ${Tester_jarFile}:${LIME_JAR_FILE} -Xcheck:jni -ea org/linphone/limeTester/LimeTester)
