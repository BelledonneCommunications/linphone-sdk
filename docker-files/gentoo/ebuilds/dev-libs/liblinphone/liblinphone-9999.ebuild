EAPI=8

PYTHON_COMPAT=( python3_{9..12} )

inherit cmake git-r3 python-r1

DESCRIPTION="SIP library supporting voice/video calls and text messaging"
HOMEPAGE="https://gitlab.linphone.org/BC/public/liblinphone"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/liblinphone.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="debug doc ldap lime qrcode test tools"
REQUIRED_USE="${PYTHON_REQUIRED_USE}"
PROPERTIES="test_network"

RDEPEND="dev-libs/belr
	dev-libs/jsoncpp:0=
	dev-db/sqlite:3
	dev-db/soci
	dev-libs/belcard
	net-libs/belle-sip
	dev-libs/libxml2:2
	dev-libs/lime
	dev-libs/xerces-c
	dev-libs/bctoolbox[test?]
	net-libs/ortp
	media-libs/mediastreamer2[zrtp,srtp,jpeg]
	sys-libs/zlib:0
	virtual/libiconv
	virtual/libintl
	virtual/libudev
	ldap? ( net-nds/openldap:0= )
	qrcode? ( media-libs/zxing-cpp:0= )
	tools? ( ${PYTHON_DEPS}
		dev-python/pystache[${PYTHON_USEDEP}]
		dev-python/six[${PYTHON_USEDEP}] )"
DEPEND="${RDEPEND}"
BDEPEND="${PYTHON_DEPS}
	app-text/doxygen[dot]
    dev-cpp/jsoncpp
	dev-python/pystache[${PYTHON_USEDEP}]
	dev-python/six[${PYTHON_USEDEP}]
	dev-vcs/git
	doc? ( dev-python/sphinx[${PYTHON_USEDEP}] )"

src_configure() {
	local mycmakeargs=(
		-DENABLE_CONSOLE_UI=NO
		-DENABLE_DEBUG_LOGS="$(usex debug)"
		-DENABLE_DOC="$(usex doc)"
		-DENABLE_LDAP="$(usex ldap)"
		-DENABLE_LIME_X3DH="$(usex lime)"
		-DENABLE_QRCODE="$(usex qrcode)"
		-DENABLE_RELATIVE_PREFIX=OFF
		-DENABLE_STRICT=YES
		-DENABLE_TOOLS="$(usex tools)"
		-DENABLE_UNIT_TESTS="$(usex test)"
	)

	cmake_src_configure
}

src_test() {
	"${S}"_build/tester/liblinphone-tester \
		--resource-dir "${S}"/tester/ \
		|| die "tests failed"

	cmake_src_test
}

src_install() {
	cmake_src_install

	# path is needed for LibLinphoneConfig.cmake
	# portage doesn't install empty dirs
	keepdir /usr/$(get_libdir)/liblinphone/plugins
}
