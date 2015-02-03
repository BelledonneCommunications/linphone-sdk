#!/bin/sh

if [ -n "@AUTOTOOLS_AS_COMPILER@" ]
then
	export AS="@AUTOTOOLS_AS_COMPILER@"
fi
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

VERBOSE=""
if [ @AUTOTOOLS_VERBOSE_MAKEFILE@ -eq 1 ]; then
	VERBOSE="--verbose"
fi

# SPEC_PREFIX=""
# if [ @LINPHONE_BUILDER_SPEC_PREFIX@ != "" ]; then
# 	SPEC_PREFIX="--define \"_prefix @LINPHONE_BUILDER_SPEC_PREFIX@\""
# fi

cd @ep_source@
# TODO rpmbuild -ba @ep_build@/@LINPHONE_BUILDER_SPEC_FILE@ --define "_topdir $RPM_TOPDIR" $SPEC_PREFIX --with bc $VERBOSE @ep_redirect_to_file@
rpmbuild -ba @ep_build@/@LINPHONE_BUILDER_SPEC_FILE@ \
	--define "_topdir $RPM_TOPDIR" --define '_PKG_CONFIG_PATH $PKG_CONFIG_PATH'\
	--define "_builddir @ep_source@" \
	@LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTION@ \
	@LINPHONE_BUILDER_RPMBUILD_OPTIONS@ \
	$VERBOSE @ep_redirect_to_file@
