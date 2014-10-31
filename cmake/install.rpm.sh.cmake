#!/bin/sh

export CC="@AUTOTOOLS_C_COMPILER@"
export CXX="@AUTOTOOLS_CXX_COMPILER@"
export OBJC="@AUTOTOOLS_OBJC_COMPILER@"
export LD="@AUTOTOOLS_LINKER@"
export AR="@AUTOTOOLS_AR@"
export RANLIB="@AUTOTOOLS_RANLIB@"
export STRIP="@AUTOTOOLS_STRIP@"
export NM="@AUTOTOOLS_NM@"

export ASFLAGS="@ep_asflags@"
export CPPFLAGS="@ep_cppflags@"
export CFLAGS="@ep_cflags@"
export CXXFLAGS="@ep_cxxflags@"
export OBJCFLAGS="@ep_objcflags@"
export LDFLAGS="@ep_ldflags@"

export PKG_CONFIG="@LINPHONE_BUILDER_PKG_CONFIG@"
export PKG_CONFIG_PATH="@LINPHONE_BUILDER_PKG_CONFIG_PATH@"
export PKG_CONFIG_LIBDIR="@LINPHONE_BUILDER_PKG_CONFIG_LIBDIR@"

export RPM_TOPDIR="@LINPHONE_BUILDER_WORK_DIR@/rpmbuild"

if [[ "@PLATFORM@" -eq "Debian" ]]; then
	cd "$RPM_TOPDIR/RPMS"
	find "$RPM_TOPDIR/RPMS" -name "*@LINPHONE_BUILDER_RPMBUILD_NAME@*.rpm" -exec fakeroot alien -d {} +
	find "$RPM_TOPDIR/RPMS" -name "*@LINPHONE_BUILDER_RPMBUILD_NAME@*.deb" -exec sudo dpkg -i {} +
else
	find "$RPM_TOPDIR/RPMS" -name "*@LINPHONE_BUILDER_RPMBUILD_NAME@*.rpm" -exec sudo rpm -ivh --force {} +
	#sudo rpm -ivh WORK/rpmbuild/RPMS/x86_64/*.rpm --force
fi
