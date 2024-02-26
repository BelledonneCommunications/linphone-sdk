EAPI=8

inherit cmake git-r3

DESCRIPTION="The libdecaf library is for elliptic curve research and practical application."
HOMEPAGE="https://gitlab.linphone.org/BC/public/lime"
EGIT_REPO_URI="https://git.code.sf.net/p/ed448goldilocks/code"
EGIT_COMMIT="v1.0.2"

LICENSE="MIT"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="doc static-libs"

RDEPEND=""
DEPEND="${RDEPEND}"
BDEPEND=">=dev-lang/python-3
    doc? ( app-text/doxygen )"

src_configure() {
	local mycmakeargs=(
        -DENABLE_SHARED="$(usex static-libs NO YES)"
		-DENABLE_STATIC="$(usex static-libs)"
		-DENABLE_STRICT=NO
        -DCMAKE_C_FLAGS="-DDECAF_EDDSA_NON_KEYPAIR_API_IS_DEPRECATED=0"
	)

	cmake_src_configure
}
