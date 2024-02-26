EAPI=8

inherit cmake git-r3

DESCRIPTION="BC Unit Test Framework"
HOMEPAGE="https://gitlab.linphone.org/BC/public/bcunit"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/bcunit.git"
EGIT_BRANCH="master"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="debug doc examples ncurses test"
RESTRICT="test"

RDEPEND="ncurses? ( sys-libs/ncurses:0= )"
DEPEND="${RDEPEND}"
BDEPEND="virtual/pkgconfig"

src_configure() {
	local mycmakeargs=(
		-DENABLE_BCUNIT_CURSES="$(usex ncurses)"
		-DENABLE_BCUNIT_DOC="$(usex doc)"
		-DENABLE_BCUNIT_EXAMPLES="$(usex examples)"
		-DENABLE_BCUNIT_MEMTRACE="$(usex debug)"
		-DENABLE_BCUNIT_TEST="$(usex test)"
	)

	cmake_src_configure
}

