#!/bin/sh

export PATH="$PATH:@VPX_ENV_PATH@"
export INCLUDE="@VPX_ENV_INCLUDE@"
export LIB="@VPX_ENV_LIB@"
export LIBPATH="@VPX_ENV_LIBPATH@"

cd @ep_build@
@ep_source@/configure @EP_vpx_CONFIGURE_OPTIONS_STR@ "--prefix=@CMAKE_INSTALL_PREFIX@" "--target=@VPX_TARGET@" @ep_redirect_to_file@
