############################################################################
# CMakeLists.txt
# Copyright (C) 2022  Belledonne Communications, Grenoble France
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

set(POSTQUANTUMCRYPTOENGINE_TEST_C_SOURCES
	postquantumcryptoengine-tester.c
)
set(POSTQUANTUMCRYPTOENGINE_TEST_CXX_SOURCES
	crypto.cc
)

add_definitions(
	-DBCTBX_LOG_DOMAIN="postquantumcryptoengine"
)

bc_apply_compile_flags(POSTQUANTUMCRYPTOENGINE_TEST_C_SOURCES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
bc_apply_compile_flags(POSTQUANTUMCRYPTOENGINE_TEST_CXX_SOURCES STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)

add_executable(postquantumcryptoengine-tester ${POSTQUANTUMCRYPTOENGINE_TEST_C_SOURCES} ${POSTQUANTUMCRYPTOENGINE_TEST_CXX_SOURCES})
set_target_properties(postquantumcryptoengine-tester PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
set_target_properties(postquantumcryptoengine-tester PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(postquantumcryptoengine-tester PRIVATE bctoolbox bctoolbox-tester)

if(ENABLE_STATIC)
	target_link_libraries(postquantumcryptoengine-tester PRIVATE postquantumcryptoengine-static)
else()
	target_link_libraries(postquantumcryptoengine-tester PRIVATE postquantumcryptoengine)
endif()

add_test(NAME postquantumcryptoengine-tester COMMAND postquantumcryptoengine-tester)
