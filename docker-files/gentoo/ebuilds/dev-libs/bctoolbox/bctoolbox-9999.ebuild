EAPI=8

inherit cmake git-r3

DESCRIPTION="Utilities library used by Belledonne Communications softwares"
HOMEPAGE="https://gitlab.linphone.org/BC/public/bctoolbox"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/bctoolbox.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="mbedtls openssl test"
PROPERTIES="test_network"

RDEPEND="dev-libs/decaf
	mbedtls? ( net-libs/mbedtls:1 )
	openssl? ( dev-libs/openssl:0 )"
DEPEND="${RDEPEND}"
BDEPEND="virtual/pkgconfig
	test? ( dev-libs/bcunit )"

src_configure() {
	local mycmakeargs=(
		-DENABLE_DEBUG_LOGS=NO
		-DENABLE_MBEDTLS="$(usex mbedtls )"
		-DENABLE_OPENSSL="$(usex openssl)"
		-DENABLE_STRICT=YES
		-DENABLE_TESTS_COMPONENT="$(usex test)"
		-DENABLE_UNIT_TESTS="$(usex test)"
		-DCMAKE_CXX_FLAGS="-DDECAF_EDDSA_NON_KEYPAIR_API_IS_DEPRECATED=0"
	)

	cmake_src_configure
}

src_test() {
	"${S}"_build/tester/bctoolbox-tester || die "tests failed"

	cmake_src_test
}

