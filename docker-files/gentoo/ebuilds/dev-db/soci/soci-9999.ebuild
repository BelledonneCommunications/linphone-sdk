EAPI=8

inherit cmake git-r3

DESCRIPTION="Database access library for C++"
HOMEPAGE="https://gitlab.linphone.org/BC/public/external/soci"
EGIT_REPO_URI="https://gitlab.linphone.org/BC/public/external/soci.git"
EGIT_BRANCH="bc"

LICENSE="BSD-1"
KEYWORDS="~amd64 ~x86"
SLOT="0"
IUSE="firebird mysql odbc oracle postgres sqlite test"

RDEPEND="dev-libs/boost:=
	firebird? ( dev-db/firebird )
	mysql? ( dev-db/mysql-connector-c:0= )
	odbc? ( dev-db/unixODBC )
	oracle? ( dev-db/oracle-instantclient:= )
	postgres? ( dev-db/postgresql:= )
	sqlite? ( dev-db/sqlite:3 )"
DEPEND="${RDEPEND}"

src_configure() {
	local mycmakeargs=(
		-DSOCI_INSTALL_BACKEND_TARGETS=OFF
		-DSOCI_SHARED=ON
		-DSOCI_STATIC=OFF
		-DSOCI_TESTS="$(usex test)"
		-DWITH_MYSQL="$(usex mysql)"
		-DWITH_POSTGRESQL="$(usex postgres)"
		-DWITH_SQLITE3="$(usex sqlite)"
	)

	cmake_src_configure
}
