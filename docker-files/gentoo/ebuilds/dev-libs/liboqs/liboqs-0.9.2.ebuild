EAPI=8

inherit cmake

DESCRIPTION="Library for quantum-resistant cryptographic algorithms"
HOMEPAGE="https://openquantumsafe.org/"
SRC_URI="https://github.com/open-quantum-safe/liboqs/archive/refs/tags/${PV}.tar.gz -> ${P}.tar.gz"

LICENSE="MIT"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="openssl"

RDEPEND="openssl? ( dev-libs/openssl )"
DEPEND="${RDEPEND}"

src_configure() {
	local mycmakeargs=(
        -DOQS_BUILD_ONLY_LIB=ON
        -DOQS_DIST_LIB=ON
		-DOQS_USE_OPENSSL=$(usex openssl)
		-DOQS_MINIMAL_BUILD="KEM_kyber;KEM_hqc;OQS_ENABLE_KEM_kyber_512;OQS_ENABLE_KEM_kyber_768;OQS_ENABLE_KEM_kyber_1024;OQS_ENABLE_KEM_hqc_128;OQS_ENABLE_KEM_hqc_192;OQS_ENABLE_KEM_hqc_256"
	)

	cmake_src_configure
}

src_prepare() {
    rm -rf scripts # Crappy workaround to prevent failure in cmake_src_prepare because of a directory name CMakeLists.txt

    cmake_src_prepare
}