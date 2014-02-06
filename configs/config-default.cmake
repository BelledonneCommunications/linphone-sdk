############################################################################
# config-default.cmake
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

# List of the projects to build. The order is important and must follow the dependencies.
set(LINPHONE_BUILDER_BUILDERS
	cunit
	xml2
	antlr3c
	polarssl
	bellesip
	srtp
	gsm
	speex
	opus
	linphone
)

# cunit
set(EP_cunit_GIT_REPOSITORY "git://git.linphone.org/cunit.git")
set(EP_cunit_GIT_TAG "linphone")
set(EP_cunit_CMAKE_OPTIONS "-DENABLE_AUTOMATED=0 -DENABLE_CONSOLE=0")

# xml2
set(EP_xml2_GIT_REPOSITORY "git://git.gnome.org/libxml2")
set(EP_xml2_GIT_TAG "v2.8.0")
set(EP_xml2_CONFIGURE_OPTIONS "--disable-rebuild-docs --with-iconv=no --with-python=no --with-zlib=no --with-modules=no")

# antlr3c
set(EP_antlr3c_GIT_REPOSITORY "git://git.linphone.org/antlr3.git")
set(EP_antlr3c_GIT_TAG "linphone")
set(EP_antlr3c_CMAKE_OPTIONS "-DENABLE_DEBUGGER=0")

# polarssl
set(EP_polarssl_GIT_REPOSITORY "git://git.linphone.org/polarssl.git")
set(EP_polarssl_GIT_TAG "linphone")

# belle-sip
set(EP_bellesip_GIT_REPOSITORY "git://git.linphone.org/belle-sip.git")
set(EP_bellesip_GIT_TAG "master")
set(EP_bellesip_CONFIGURE_OPTIONS "--enable-tls")
set(EP_bellesip_DEPENDENCIES EP_antlr3c EP_cunit EP_polarssl)

# srtp
set(EP_srtp_GIT_REPOSITORY "git://git.linphone.org/srtp.git")
set(EP_srtp_GIT_TAG "master")

# gsm
set(EP_gsm_GIT_REPOSITORY "git://git.linphone.org/gsm.git")
set(EP_gsm_GIT_TAG "linphone")
set(EP_gsm_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/gsm/CMakeLists.txt" "<SOURCE_DIR>")

# speex
set(EP_speex_GIT_REPOSITORY "git://git.linphone.org/speex.git")
set(EP_speex_GIT_TAG "linphone")

# opus
set(EP_opus_GIT_REPOSITORY "git://git.opus-codec.org/opus.git")
set(EP_opus_GIT_TAG "v1.0.3")
set(EP_opus_CONFIGURE_OPTIONS "--disable-extra-programs --disable-doc")

# linphone
set(EP_linphone_GIT_REPOSITORY "git://git.linphone.org/linphone.git")
set(EP_linphone_GIT_TAG "master")
set(EP_linphone_CONFIGURE_OPTIONS "--disable-strict --enable-bellesip")
set(EP_linphone_DEPENDENCIES EP_bellesip EP_xml2 EP_gsm EP_speex EP_opus EP_srtp)
