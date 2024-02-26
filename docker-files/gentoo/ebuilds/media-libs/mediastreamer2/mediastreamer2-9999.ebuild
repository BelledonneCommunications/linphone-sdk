EAPI=8

inherit cmake git-r3

DESCRIPTION="Mediastreaming library for telephony application"
HOMEPAGE="https://gitlab.linphone.org/BC/public/mediastreamer2"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/mediastreamer2.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="alsa av1 bv16 debug doc g726 g729 gsm jpeg matroska opengl opus pcap portaudio +pulseaudio qrcode speex srtp resample test theora tools +v4l vpx yuv zrtp"
REQUIRED_USE="zrtp? ( srtp )
	resample? ( speex )
	|| ( alsa portaudio pulseaudio )
	|| ( opengl v4l )"
PROPERTIES="test_network"

RDEPEND="dev-libs/bctoolbox[test?]
	net-libs/ortp
	alsa? ( media-libs/alsa-lib )
	av1? ( || ( media-libs/dav1d
		media-libs/libaom ) )
	bv16? ( media-libs/bv16-floatingpoint )
	g726? ( media-libs/spandsp )
	g729? ( media-libs/bcg729 )
	gsm? ( media-sound/gsm )
	jpeg? ( media-libs/libjpeg-turbo )
	matroska? ( media-libs/bcmatroska2 )
	opengl? ( media-libs/glew:0
		x11-libs/libX11
		virtual/opengl )
	opus? ( media-libs/opus )
	pcap? ( net-libs/libpcap )
	portaudio? ( media-libs/portaudio )
	pulseaudio? ( media-sound/pulseaudio-daemon )
	qrcode? ( media-libs/zxing-cpp )
	speex? ( media-libs/speex
		media-libs/speexdsp )
	srtp? ( net-libs/libsrtp:2 )
	theora? ( media-libs/libtheora )
	v4l? ( media-libs/libv4l )
	vpx? ( media-libs/libvpx:= )
	yuv? ( media-libs/libyuv )
	zrtp? ( net-libs/bzrtp[sqlite] )"
DEPEND="${RDEPEND}"
BDEPEND="doc? ( app-text/doxygen )"

src_prepare() {
	# fix path for nowebcamCIF.jpg
	# sed -i '/DESTINATION ${CMAKE_INSTALL_DATADIR}/s|}|}/Mediastreamer2|' \
	# 	src/CMakeLists.txt || die "sed for CMakeLists.txt failed"

	cmake_src_prepare
}

src_configure() {
	local mycmakeargs=(
		-DENABLE_ALSA="$(usex alsa)"
		-DENABLE_AV1="$(usex av1)"
		-DENABLE_BV16="$(usex bv16)"
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
		-DENABLE_LIBYUV="$(usex yuv)"
		-DENABLE_MKV="$(usex matroska)"
		-DENABLE_OPUS="$(usex opus)"
		-DENABLE_PCAP="$(usex pcap)"
		-DENABLE_PORTAUDIO="$(usex portaudio)"
		-DENABLE_PULSEAUDIO="$(usex pulseaudio)"
		-DENABLE_QRCODE="$(usex qrcode)"
		-DENABLE_RELATIVE_PREFIX=OFF
		-DENABLE_RESAMPLE="$(usex resample)"
		-DENABLE_SPEEX_CODEC="$(usex speex)"
		-DENABLE_SPEEX_DSP="$(usex speex)"
		-DENABLE_SRTP="$(usex srtp)"
		-DENABLE_STRICT=NO
		-DENABLE_THEORA="$(usex theora)"
		-DENABLE_TOOLS="$(usex tools)"
		-DENABLE_UNIT_TESTS="$(usex test)"
		-DENABLE_V4L="$(usex v4l)"
		-DENABLE_VPX="$(usex vpx)"
		-DENABLE_ZRTP="$(usex zrtp)"
	)

	cmake_src_configure
}

src_install() {
	cmake_src_install

	# path is needed for Mediastreamer2Config.cmake
	# portage doesn't install empty dirs
	keepdir /usr/$(get_libdir)/mediastreamer/plugins
}
