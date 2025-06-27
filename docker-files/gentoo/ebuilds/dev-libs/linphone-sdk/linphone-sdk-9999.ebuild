EAPI=8

PYTHON_COMPAT=( python3_{9..13} )

inherit cmake git-r3 python-r1

DESCRIPTION="SIP library supporting voice/video calls and text messaging"
HOMEPAGE="https://gitlab.linphone.org/BC/public/linphone-sdk"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/linphone-sdk.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="+aec alsa amrnb amrwb +av1 +bv16 codec2 debug doc +g726 -g729 +gsm +jpeg -ldap +lime +matroska ntp-timestamp
	+opengl openh264 +opus ortp-minimal pcap portaudio pqcrypto +pulseaudio +qrcode +resample +speex +srtp test +theora
	tools +v4l +vad +vpx +yuv +zeroconf +zrtp"
REQUIRED_USE="${PYTHON_REQUIRED_USE}
	zrtp? ( srtp )
	resample? ( speex )
	qrcode? ( jpeg )
	|| ( alsa portaudio pulseaudio )
	|| ( opengl v4l )"
PROPERTIES="test_network"

RDEPEND="dev-db/sqlite:3
	dev-db/soci[sqlite]
	dev-libs/decaf
	dev-libs/jsoncpp:0=
	dev-libs/libxml2:2
	dev-libs/openssl:0
	dev-libs/xerces-c
	net-libs/mbedtls:1
	sys-libs/zlib:0
	virtual/libiconv
	virtual/libintl
	virtual/libudev
	alsa? ( media-libs/alsa-lib )
	amrnb? ( media-libs/opencore-amr )
	amrwb? ( media-libs/vo-amrwbenc )
	av1? ( || ( media-libs/dav1d media-libs/libaom ) )
	bv16? ( media-libs/bv16-floatingpoint )
	codec2? ( media-libs/codec2 )
	g726? ( media-libs/spandsp )
	g729? ( media-libs/bcg729 )
	gsm? ( media-sound/gsm )
	jpeg? ( media-libs/libjpeg-turbo )
	ldap? ( net-nds/openldap:0= )
	opengl? ( media-libs/glew:0	x11-libs/libX11	virtual/opengl )
	openh264? ( media-libs/openh264 )
	opus? ( media-libs/opus )
	pcap? ( net-libs/libpcap )
	portaudio? ( media-libs/portaudio )
	pqcrypto? ( dev-libs/liboqs )
	pulseaudio? ( media-sound/pulseaudio-daemon )
	qrcode? ( media-libs/zxing-cpp:0= )
	speex? ( media-libs/speex media-libs/speexdsp )
	srtp? ( net-libs/libsrtp:2 )
	theora? ( media-libs/libtheora )
	tools? ( ${PYTHON_DEPS}
		dev-python/pystache[${PYTHON_USEDEP}]
		dev-python/six[${PYTHON_USEDEP}] )
	v4l? ( media-libs/libv4l )
	vpx? ( media-libs/libvpx:= )
	yuv? ( media-libs/libyuv )
	zeroconf? ( net-dns/avahi[mdnsresponder-compat] )"
DEPEND="${RDEPEND}"
BDEPEND="${PYTHON_DEPS}
	app-text/doxygen[dot]
	dev-cpp/jsoncpp
	dev-python/pystache[${PYTHON_USEDEP}]
	dev-python/six[${PYTHON_USEDEP}]
	dev-vcs/git
	virtual/libudev
	virtual/pkgconfig
	doc? ( dev-python/sphinx[${PYTHON_USEDEP}] )"

src_configure() {
	local mycmakeargs=(
		-DCMAKE_CXX_FLAGS="-DDECAF_EDDSA_NON_KEYPAIR_API_IS_DEPRECATED=0"
		-DCMAKE_SHARED_LINKER_FLAGS="${LDFLAGS} -z muldefs"
		-DCONFIG_DEBUG_LEAKS="$(usex debug)"
		-DBUILD_AOM=NO
		-DBUILD_BCG729=NO
		-DBUILD_BV16=NO
		-DBUILD_CODEC2=NO
		-DBUILD_DAV1D=NO
		-DBUILD_DECAF=NO
		-DBUILD_GSM=NO
		-DBUILD_JSONCPP=NO
		-DBUILD_LIBJPEGTURBO=NO
		-DBUILD_LIBSRTP2=NO
		-DBUILD_LIBVPX=NO
		-DBUILD_LIBXML2=NO
		-DBUILD_LIBYUV=NO
		-DBUILD_MBEDTLS=NO
		-DBUILD_OPENCORE_AMR=NO
		-DBUILD_OPENLDAP=NO
		-DBUILD_OPUS=NO
		-DBUILD_SOCI=NO
		-DBUILD_SPEEX=NO
		-DBUILD_SQLITE3=NO
		-DBUILD_VO_AMRWBENC=NO
		-DBUILD_XERCESC=NO
		-DBUILD_ZLIB=NO
		-DBUILD_ZXINGCPP=NO
		-DENABLE_ALSA="$(usex alsa)"
		-DENABLE_AV1="$(usex av1)"
		-DENABLE_BV16="$(usex bv16)"
		-DENABLE_CODEC2="$(usex codec2)"
		-DENABLE_CONSOLE_UI=NO
		-DENABLE_DEBUG_LOGS="$(usex debug)"
		-DENABLE_DOC="$(usex doc)"
		-DENABLE_FFMPEG=NO
		-DENABLE_G726="$(usex g726)"
		-DENABLE_G729="$(usex g729)"
		-DENABLE_G729B_CNG="$(usex g729)"
		-DENABLE_GL="$(usex opengl)"
		-DENABLE_GLX="$(usex opengl)"
		-DENABLE_GSM="$(usex gsm)"
		-DENABLE_JPEG="$(usex jpeg)"
		-DENABLE_LDAP="$(usex ldap)"
		-DENABLE_LIBYUV="$(usex yuv)"
		-DENABLE_LIME_X3DH="$(usex lime)"
		-DENABLE_MDNS="$(usex zeroconf)"
		-DENABLE_MKV="$(usex matroska)"
		-DENABLE_AMRNB="$(usex amrnb)"
		-DENABLE_AMRWB="$(usex amrwb)"
		-DENABLE_NTP_TIMESTAMP="$(usex ntp-timestamp)"
		-DENABLE_OPENH264="$(usex openh264)"
		-DENABLE_OPUS="$(usex opus)"
		-DENABLE_PCAP="$(usex pcap)"
		-DENABLE_PERF="$(usex ortp-minimal)"
		-DENABLE_PORTAUDIO="$(usex portaudio)"
		-DENABLE_PQCRYPTO="$(usex pqcrypto)"
		-DENABLE_PULSEAUDIO="$(usex pulseaudio)"
		-DENABLE_QRCODE="$(usex qrcode)"
		-DENABLE_RELATIVE_PREFIX=OFF
		-DENABLE_RESAMPLE="$(usex resample)"
		-DENABLE_SPEEX_CODEC="$(usex speex)"
		-DENABLE_SPEEX_DSP="$(usex speex)"
		-DENABLE_SRTP="$(usex srtp)"
		-DENABLE_STRICT=NO
		-DENABLE_TESTS=NO
		-DENABLE_TESTS_COMPONENT="$(usex test)"
		-DENABLE_THEORA="$(usex theora)"
		-DENABLE_TOOLS="$(usex tools)"
		-DENABLE_UNIT_TESTS="$(usex test)"
		-DENABLE_V4L="$(usex v4l)"
		-DENABLE_VPX="$(usex vpx)"
		-DENABLE_WEBRTC_AEC="$(usex aec)"
		-DENABLE_WEBRTC_VAD="$(usex vad)"
		-DENABLE_ZIDCACHE=YES
		-DENABLE_ZRTP="$(usex zrtp)"
		-DWITH_OpenH264=/usr/include/openh264
	)

	cmake_src_configure
}

src_test() {
	"${S}"_build/tester/bctoolbox-tester || die "tests failed"
	"${S}"_build/tester/belr-tester --resource-dir "${S}"/tester/res || die "tests failed"
	"${S}"_build/tester/belcard-tester --resource-dir "${S}"/tester/ || die "tests failed"
	"${S}"_build/tester/ortp-tester || die "tests failed"
	"${S}"_build/tester/belle-sip-tester --resource-dir "${S}"/tester/ || die "tests failed"
	"${S}"_build/tester/liblinphone-tester --resource-dir "${S}"/tester/ || die "tests failed"

	cmake_src_test
}

src_install() {
	cmake_src_install

	# path is needed for Mediastreamer2Config.cmake
	# portage doesn't install empty dirs
	keepdir /usr/$(get_libdir)/mediastreamer/plugins

	# path is needed for LibLinphoneConfig.cmake
	# portage doesn't install empty dirs
	keepdir /usr/$(get_libdir)/liblinphone/plugins
}
