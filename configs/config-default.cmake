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

# Include linphone builder options definitions
include(cmake/LinphoneBuilderOptions.cmake)


# List of the projects to build. The order is important and must follow the dependencies.
set(LINPHONE_BUILDER_BUILDERS )
if(${ENABLE_UNIT_TESTS})
	list(APPEND LINPHONE_BUILDER_BUILDERS cunit)
endif(${ENABLE_UNIT_TESTS})
if(${ENABLE_SRTP})
	list(APPEND LINPHONE_BUILDER_BUILDERS srtp)
endif(${ENABLE_SRTP})
if(${ENABLE_GSM})
	list(APPEND LINPHONE_BUILDER_BUILDERS gsm)
endif(${ENABLE_GSM})
if(${ENABLE_OPUS})
	list(APPEND LINPHONE_BUILDER_BUILDERS opus)
endif(${ENABLE_OPUS})
if(${ENABLE_SPEEX})
	list(APPEND LINPHONE_BUILDER_BUILDERS speex)
endif(${ENABLE_SPEEX})

list(APPEND LINPHONE_BUILDER_BUILDERS
	xml2
	antlr3c
	polarssl
	bellesip
	ortp
	ms2
	linphone
)


# cunit
set(EP_cunit_GIT_REPOSITORY "git://git.linphone.org/cunit.git")
set(EP_cunit_GIT_TAG "86562ef04d0d66c007d7822944a75f540ae37f19") # Branch 'linphone'
set(EP_cunit_CMAKE_OPTIONS "-DENABLE_AUTOMATED=0" "-DENABLE_CONSOLE=0")

# xml2
set(EP_xml2_GIT_REPOSITORY "git://git.gnome.org/libxml2")
set(EP_xml2_GIT_TAG "v2.8.0")
set(EP_xml2_CONFIGURE_OPTIONS "--with-minimum --with-xpath --with-tree --with-schemas --with-reader --with-writer --enable-rebuild-docs=no")

# antlr3c
set(EP_antlr3c_GIT_REPOSITORY "git://git.linphone.org/antlr3.git")
set(EP_antlr3c_GIT_TAG "b882cfc0d8e6485d6d050e7f5ec36f870c7ece7b") # Branch 'linphone'
set(EP_antlr3c_CMAKE_OPTIONS "-DENABLE_DEBUGGER=0")

# polarssl
set(EP_polarssl_GIT_REPOSITORY "git://git.linphone.org/polarssl.git")
set(EP_polarssl_GIT_TAG "3681900a1e0a3a8c77fc33c545cccd93977a1cf2") # Branch 'linphone'

# belle-sip
set(EP_bellesip_GIT_REPOSITORY "git://git.linphone.org/belle-sip.git")
set(EP_bellesip_GIT_TAG "cb977e6aaa0a319c437d08e3d67492d1453adbfb") # Branch 'master'
set(EP_bellesip_CMAKE_OPTIONS )
set(EP_bellesip_DEPENDENCIES EP_antlr3c EP_polarssl)
if(${ENABLE_UNIT_TESTS})
	list(APPEND EP_bellesip_DEPENDENCIES EP_cunit)
else(${ENABLE_UNIT_TESTS})
	list(APPEND EP_bellesip_CMAKE_OPTIONS "-DENABLE_TESTS=0")
endif(${ENABLE_UNIT_TESTS})

# srtp
set(EP_srtp_GIT_REPOSITORY "git://git.linphone.org/srtp.git")
set(EP_srtp_GIT_TAG "da2ece56f18d35a12f0fee5dcb99e03ff15864de") # Branch 'master'

# gsm
set(EP_gsm_GIT_REPOSITORY "git://git.linphone.org/gsm.git")
set(EP_gsm_GIT_TAG "8729c98e098341582e9c9f00e56b74f7e53e1034") # Branch 'linphone'
set(EP_gsm_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/gsm/CMakeLists.txt" "<SOURCE_DIR>")

# speex
set(EP_speex_GIT_REPOSITORY "git://git.linphone.org/speex.git")
set(EP_speex_GIT_TAG "302ce26e309efb1b4a4b7b6ea4807375d157258f") # Branch 'linphone'

# opus
set(EP_opus_GIT_REPOSITORY "git://git.opus-codec.org/opus.git")
set(EP_opus_GIT_TAG "v1.0.3")
set(EP_opus_CONFIGURE_OPTIONS "--disable-extra-programs --disable-doc")

# oRTP
set(EP_ortp_GIT_REPOSITORY "git://git.linphone.org/ortp.git")
set(EP_ortp_GIT_TAG "e1ea9d5121cdabbcc16ffdb884bf705caacd81a1") # Branch 'master'
set(EP_ortp_CONFIGURE_OPTIONS "--disable-strict")
set(EP_ortp_DEPENDENCIES )
if(${ENABLE_SRTP})
	set(EP_ortp_CONFIGURE_OPTIONS "${EP_ortp_CONFIGURE_OPTIONS} --with-srtp=${CMAKE_INSTALL_PREFIX}")
	list(APPEND EP_ortp_DEPENDENCIES EP_srtp)
endif(${ENABLE_SRTP})
if(${ENABLE_ZRTP})
	# TODO
else(${ENABLE_ZRTP})
	set(EP_ortp_CONFIGURE_OPTIONS "${EP_ortp_CONFIGURE_OPTIONS} --disable-zrtp")
endif(${ENABLE_ZRTP})

# mediastreamer2
set(EP_ms2_GIT_REPOSITORY "git://git.linphone.org/mediastreamer2.git")
set(EP_ms2_GIT_TAG "a16157a62d0512b54ee7b36dadea5a95e83847c0") # Branch 'master'
set(EP_ms2_CONFIGURE_OPTIONS "--disable-strict --enable-external-ortp")
set(EP_ms2_DEPENDENCIES EP_ortp)
if(${ENABLE_GSM})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --with-gsm=${CMAKE_INSTALL_PREFIX}")
	list(APPEND EP_ms2_DEPENDENCIES EP_gsm)
else(${ENABLE_GSM})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --disable-gsm")
endif(${ENABLE_GSM})
if(${ENABLE_OPUS})
	list(APPEND EP_ms2_DEPENDENCIES EP_opus)
else(${ENABLE_OPUS})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --disable-opus")
endif(${ENABLE_OPUS})
if(${ENABLE_SPEEX})
	list(APPEND EP_ms2_DEPENDENCIES EP_speex)
else(${ENABLE_SPEEX})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --disable-speex")
endif(${ENABLE_SPEEX})
if(${ENABLE_UNIT_TESTS})
	list(APPEND EP_ms2_DEPENDENCIES EP_cunit)
else(${ENABLE_UNIT_TESTS})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --disable-tests")
endif(${ENABLE_UNIT_TESTS})

# linphone
set(EP_linphone_GIT_REPOSITORY "git://git.linphone.org/linphone.git")
set(EP_linphone_GIT_TAG "3a8d2ee20d219432b40cc583dd0d0a3e28e4e7f7") # Branch 'master'
set(EP_linphone_CONFIGURE_OPTIONS "--disable-strict --enable-bellesip --enable-external-ortp --enable-external-mediastreamer")
set(EP_linphone_DEPENDENCIES EP_bellesip EP_ortp EP_ms2 EP_xml2)
if(${ENABLE_ZRTP})
	# TODO
else(${ENABLE_ZRTP})
	set(EP_linphone_CONFIGURE_OPTIONS "${EP_linphone_CONFIGURE_OPTIONS} --disable-zrtp")
endif(${ENABLE_ZRTP})
if(${ENABLE_UNIT_TESTS})
	list(APPEND EP_linphone_DEPENDENCIES EP_cunit)
else(${ENABLE_UNIT_TESTS})
	set(EP_linphone_CONFIGURE_OPTIONS "${EP_linphone_CONFIGURE_OPTIONS} --disable-tests")
endif(${ENABLE_UNIT_TESTS})
