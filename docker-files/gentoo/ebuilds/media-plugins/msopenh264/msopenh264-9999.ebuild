EAPI=8

inherit cmake git-r3

DESCRIPTION="Mediastreamer plugin for H.264 video codec support using the openh264 library"
HOMEPAGE="https://gitlab.linphone.org/BC/public/msopenh264"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/msopenh264.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE=""

RDEPEND="media-libs/mediastreamer2
    media-libs/openh264"
DEPEND="${RDEPEND}"

src_configure() {
	local mycmakeargs=(
		-DWITH_OpenH264=/usr/include/openh264
	)

	cmake_src_configure
}
