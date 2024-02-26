EAPI=8

inherit cmake

DESCRIPTION="CMake files for C++ JSON reader and writer"
HOMEPAGE="https://github.com/open-source-parsers/jsoncpp"
SRC_URI="https://github.com/open-source-parsers/jsoncpp/archive/${PV}.tar.gz -> ${P}.tar.gz"

LICENSE="GPL-3"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="static-libs test"

RDEPEND="dev-libs/jsoncpp:0="

src_configure() {
	local mycmakeargs=(
		-DJSONCPP_WITH_TESTS="$(usex test)"
		-DJSONCPP_WITH_POST_BUILD_UNITTEST="$(usex test)"
		-DBUILD_STATIC_LIBS="$(usex static-libs)"
		-DBUILD_OBJECT_LIBS=OFF
	)

	cmake_src_configure
}

src_install() {
	cmake_src_install

	# remove everything except cmake files
	rm -rf "${ED}"/usr/{include,share,lib64/pkgconfig,lib64/lib*} \
		|| die "rm failed"
	# add link to header files from dev-libs/jsoncpp
	dosym /usr/include/jsoncpp/json /usr/include/json
	dosym ./libjsoncpp.so usr/"$(get_libdir)"/libjsoncpp.so."${PV}"
}
