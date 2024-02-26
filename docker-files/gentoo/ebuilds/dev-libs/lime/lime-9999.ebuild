EAPI=8

inherit cmake git-r3

DESCRIPTION="C++ library implementing Open Whisper System Signal protocol"
HOMEPAGE="https://gitlab.linphone.org/BC/public/lime"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/lime.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="doc test"
# RESTRICT="test" # fail: segfault

RDEPEND="dev-db/soci[sqlite]
	dev-libs/bctoolbox[test?]"
DEPEND="${RDEPEND}"
BDEPEND="doc? ( app-text/doxygen )
	test? ( net-libs/belle-sip )"

src_configure() {
	local mycmakeargs=(
		-DENABLE_DOC="$(usex doc)"
		-DENABLE_STRICT=YES
		-DENABLE_UNIT_TESTS="$(usex test)"
	)

	cmake_src_configure
}
