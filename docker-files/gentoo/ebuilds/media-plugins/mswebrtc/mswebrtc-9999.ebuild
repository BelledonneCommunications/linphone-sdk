EAPI=8

inherit cmake git-r3

DESCRIPTION="Mediastreamer plugin to include features from WebRTC (iSAC codec, AEC...)"
HOMEPAGE="https://gitlab.linphone.org/BC/public/mswebrtc"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/mswebrtc.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="+aec +isac +ilbc +vad"

RDEPEND="media-libs/mediastreamer2"
DEPEND="${RDEPEND}"

src_configure() {
	local mycmakeargs=(
		-DENABLE_AEC="$(usex aec)"
		-DENABLE_ISAC="$(usex isac)"
        -DENABLE_ILBC="$(usex ilbc)"
        -DENABLE_VAD="$(usex vad)"
	)

	cmake_src_configure
}
