EAPI=8

inherit cmake

DESCRIPTION="BroadVoice 16 kbs codec"
HOMEPAGE="https://gitlab.linphone.org/BC/public/external/bv16-floatingpoint"
SRC_URI="https://gitlab.linphone.org/BC/public/external/${PN}/-/archive/${PV}/${P}.tar.bz2 -> ${P}.tar.bz2"

LICENSE="LGPL-2.1"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="static-libs"

src_configure() {
	local mycmakeargs=(
		-DENABLE_STATIC="$(usex static-libs)"
	)

	cmake_src_configure
}
