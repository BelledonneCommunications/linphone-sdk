EAPI=8

inherit cmake git-r3

DESCRIPTION="Extension to the bctoolbox lib providing Post Quantum Cryptography"
HOMEPAGE="https://gitlab.linphone.org/BC/public/postquantumcryptoengine"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/postquantumcryptoengine.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="test"

RDEPEND="dev-libs/bctoolbox[test?]
		dev-libs/liboqs"
DEPEND="${RDEPEND}"

src_configure() {
	local mycmakeargs=(
		-DENABLE_STRICT=YES
		-DENABLE_UNIT_TESTS="$(usex test)"
	)

	cmake_src_configure
}

