EAPI=8

inherit cmake git-r3

DESCRIPTION="Matroska media container support"
HOMEPAGE="https://gitlab.linphone.org/BC/public/bcmatroska2"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/bcmatroska2.git"
EGIT_BRANCH="bc"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="debug"

RDEPEND="dev-libs/bctoolbox"

src_configure() {
	local mycmakeargs=(
		-DCMAKE_SHARED_LINKER_FLAGS="${LDFLAGS} -z muldefs"
		-DCONFIG_DEBUG_LEAKS="$(usex debug)"
	)

	cmake_src_configure
}
