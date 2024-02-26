EAPI=8

inherit cmake git-r3

DESCRIPTION="Mediastreamer plugin for codec2 audio codec support"
HOMEPAGE="https://gitlab.linphone.org/BC/public/mscodec2"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/mscodec2.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE=""

RDEPEND="media-libs/mediastreamer2
    media-libs/codec2"
DEPEND="${RDEPEND}"
