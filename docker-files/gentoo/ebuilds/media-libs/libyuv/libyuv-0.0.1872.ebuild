EAPI=8

CMAKE_IN_SOURCE_BUILD="1"

inherit cmake edo git-r3

# Version bump :
# The stable libyuv version follows the chromium browser:
# https://chromereleases.googleblog.com/search/label/Desktop%20Update
# search for "The stable channel has been updated to" XX.X.XXXX.XXX
#  -> https://github.com/chromium/chromium/blob/87.0.4280.88/DEPS
#     -> 'src/third_party/libyuv': '6afd9becdf58822b1da6770598d8597c583ccfad'
# https://chromium.googlesource.com/libyuv/libyuv/+/6afd9becdf58822b1da6770598d8597c583ccfad/include/libyuv/version.h
#  -> #define LIBYUV_VERSION 1822

DESCRIPTION="Library for freeswitch yuv graphics manipulation"
HOMEPAGE="https://chromium.googlesource.com/libyuv/libyuv"
EGIT_REPO_URI="https://chromium.googlesource.com/libyuv/libyuv.git/"
EGIT_COMMIT="04821d1e7d60845525e8db55c7bcd41ef5be9406"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~x86"

RDEPEND="media-libs/libjpeg-turbo:0="

src_prepare() {
	# cmake_minimum_required() should be called prior to
	# this top-level project(), do not install static, fix libdir,
	# install yuvconstants
	sed -i  -e '/CMAKE_MINIMUM_REQUIRED( VERSION 2.8.12 )/d' \
		-e '/PROJECT (/iCMAKE_MINIMUM_REQUIRED( VERSION 2.8.12 )' \
		-e "/DESTINATION/s| lib| $(get_libdir)|" \
		-e "/TARGETS \${ly_lib_static}/d" \
		-e "/INSTALL ( PROGRAMS/aINSTALL ( PROGRAMS \${CMAKE_BINARY_DIR}/yuvconstants                  DESTINATION bin )" \
		CMakeLists.txt || die "sed failed for CMakeLists.txt"

	cmake_src_prepare
}

src_configure() {
	local mycmakeargs=(
		-DUNIT_TEST=NO
	)

	cmake_src_configure
}

src_install() {
	cmake_src_install

	insinto /usr/"$(get_libdir)"/cmake/libyuv
	doins "${FILESDIR}"/libyuv-config.cmake
}
