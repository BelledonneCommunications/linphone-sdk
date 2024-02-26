EAPI=8

inherit cmake git-r3

DESCRIPTION="Language recognition library by Belledonne Communications"
HOMEPAGE="https://gitlab.linphone.org/BC/public/belr"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/belr.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="test tools"

RDEPEND="dev-libs/bctoolbox[test?]"
DEPEND="${RDEPEND}"
BDEPEND="virtual/libudev
	virtual/pkgconfig"

src_configure() {
	local mycmakeargs=(
		-DENABLE_STRICT=YES
		-DENABLE_TOOLS="$(usex tools)"
		-DENABLE_UNIT_TESTS="$(usex test)"
	)

	cmake_src_configure
}

src_test() {
	"${S}"_build/tester/belr-tester --resource-dir "${S}"/tester/res \
		|| die "tests failed"

	cmake_src_test
}
