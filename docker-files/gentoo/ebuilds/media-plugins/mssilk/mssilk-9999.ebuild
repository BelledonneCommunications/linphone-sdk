EAPI=8

inherit cmake git-r3

DESCRIPTION="Mediastreamer plugin for SILK audio codec support"
HOMEPAGE="https://gitlab.linphone.org/BC/public/mssilk"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/mssilk.git"
EGIT_BRANCH="master"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE=""

RDEPEND="media-libs/mediastreamer2"
DEPEND="${RDEPEND}"
