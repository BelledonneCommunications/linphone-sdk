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

cd "@ep_build@"

if [ ! -f "@ep_config_h_file@" ]
then
	@ep_autogen_command@ @ep_autogen_redirect_to_file@
	@ep_configure_env@ @ep_configure_command@ @ep_configure_redirect_to_file@
fi
