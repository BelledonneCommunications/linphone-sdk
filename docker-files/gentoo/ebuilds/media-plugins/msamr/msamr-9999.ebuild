EAPI=8

inherit cmake git-r3

DESCRIPTION="Mediastreamer plugin for AMR codec support"
HOMEPAGE="https://gitlab.linphone.org/BC/public/msamr"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/msamr.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="+amrnb amrwb"

RDEPEND="media-libs/mediastreamer2
    amrnb? ( media-libs/opencore-amr )
    amrwb? ( media-libs/vo-amrwbenc )"
DEPEND="${RDEPEND}"

src_configure() {
	local mycmakeargs=(
		-DENABLE_NARROWBAND="$(usex amrnb)"
		-DENABLE_WIDEBAND="$(usex amrwb)"
	)

	cmake_src_configure
}
