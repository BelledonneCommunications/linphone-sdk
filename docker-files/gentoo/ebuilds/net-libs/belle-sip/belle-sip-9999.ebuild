EAPI=8

inherit cmake git-r3

DESCRIPTION="SIP (RFC3261) implementation"
HOMEPAGE="https://gitlab.linphone.org/BC/public/belle-sip"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/belle-sip.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="test zeroconf"
PROPERTIES="test_network"

RDEPEND="dev-libs/belr
	dev-libs/bctoolbox[test?]
	sys-libs/zlib:=
	zeroconf? ( net-dns/avahi[mdnsresponder-compat] )"
DEPEND="${RDEPEND}"

src_configure() {
	local mycmakeargs=(
		-DENABLE_MDNS="$(usex zeroconf)"
		-DENABLE_STRICT=YES
		-DENABLE_UNIT_TESTS="$(usex test)"
	)

	cmake_src_configure
}

src_test() {
	# no cmake_src_test since it supports in source build only
	"${S}"_build/tester/belle-sip-tester \
		--resource-dir "${S}"/tester/ \
		|| die "tests failed"
}
