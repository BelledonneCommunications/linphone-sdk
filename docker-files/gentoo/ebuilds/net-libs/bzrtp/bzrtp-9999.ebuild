EAPI=8

inherit cmake git-r3

DESCRIPTION="Media Path Key Agreement for Unicast Secure RTP"
HOMEPAGE="https://gitlab.linphone.org/BC/public/bzrtp"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/bzrtp.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="doc pqcrypto sqlite test"
RESTRICT="!test? ( test )"

RDEPEND="dev-libs/bctoolbox[test?]
	pqcrypto? ( dev-libs/postquantumcryptoengine )
	sqlite? ( dev-db/sqlite:3
	dev-libs/libxml2:2 )"
DEPEND="${RDEPEND}"
BDEPEND="virtual/pkgconfig"

src_configure() {
	local mycmakeargs=(
		-DENABLE_DOC="$(usex doc)"
		-DENABLE_STRICT=NO
		-DENABLE_PQCRYPTO="$(usex pqcrypto)"
		-DENABLE_UNIT_TESTS="$(usex test)"
		-DENABLE_ZIDCACHE="$(usex sqlite)"
	)

	cmake_src_configure
}
