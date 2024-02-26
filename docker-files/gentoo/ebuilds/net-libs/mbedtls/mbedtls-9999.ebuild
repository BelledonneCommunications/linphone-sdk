EAPI=8

inherit cmake git-r3

DESCRIPTION="Database access library for C++"
HOMEPAGE="https://gitlab.linphone.org/BC/public/external/mbedtls"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/external/mbedtls.git"
EGIT_BRANCH="bc"

LICENSE="|| ( Apache-2.0 GPL-2+ )"
KEYWORDS="~amd64 ~x86"
SLOT="1"
IUSE=""

RDEPEND=""
DEPEND="${RDEPEND}"

src_configure() {
	local mycmakeargs=(
        -DUSE_SHARED_MBEDTLS_LIBRARY=ON
        -DUSE_STATIC_MBEDTLS_LIBRARY=OFF
        -DENABLE_PROGRAMS=OFF
        -DENABLE_TESTING=OFF
        -DMBEDTLS_FATAL_WARNINGS=OFF
	)

	cmake_src_configure
}
