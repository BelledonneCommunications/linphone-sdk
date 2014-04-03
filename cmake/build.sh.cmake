#!/bin/sh

export CC=@CMAKE_C_COMPILER@
export CXX=@CMAKE_CXX_COMPILER@
export OBJC=@CMAKE_C_COMPILER@
export LD=@CMAKE_LINKER@
export AR=@CMAKE_AR@
export RANLIB=@CMAKE_RANLIB@
export STRIP=@CMAKE_STRIP@
export NM=@CMAKE_NM@

export ASFLAGS="@ep_asflags@ @LINPHONE_BUILDER_TOOLCHAIN_ASFLAGS@ @ep_extra_asflags@"
export CPPFLAGS="@ep_cppflags@ @LINPHONE_BUILDER_CPPFLAGS@ @ep_extra_cppflags@"
export CFLAGS="@ep_cflags@ @LINPHONE_BUILDER_CFLAGS@ @ep_extra_cflags@"
export CXXFLAGS="@ep_cxxflags@ @LINPHONE_BUILD_TOOLCHAIN_CXXFLAGS@ @ep_extra_cxxflags@"
export OBJCFLAGS="@ep_objcflags@ @LINPHONE_BUILD_TOOLCHAIN_OBJCFLAGS@ @ep_extra_objcflags@"
export LDFLAGS="@ep_ldflags@ @LINPHONE_BUILDER_LDFLAGS@ @ep_extra_ldflags@"

export PKG_CONFIG="@LINPHONE_BUILDER_PKG_CONFIG@"
export PKG_CONFIG_PATH="@LINPHONE_BUILDER_PKG_CONFIG_PATH@"
export PKG_CONFIG_LIBDIR="@LINPHONE_BUILDER_PKG_CONFIG_LIBDIR@"

cd @ep_build@
make V=@AUTOTOOLS_VERBOSE_MAKEFILE@ @ep_redirect_to_file@
