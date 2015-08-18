#!/bin/sh

export PATH="$PATH:@EP_vpx_ENV_PATH@"
export INCLUDE="@EP_vpx_ENV_INCLUDE@"
export LIB="@EP_vpx_ENV_LIB@"
export LIBPATH="@EP_vpx_ENV_LIBPATH@"

cd @ep_build@
@ep_source@/configure @EP_vpx_CONFIGURE_OPTIONS_STR@ "--prefix=@CMAKE_INSTALL_PREFIX@" "--target=@EP_vpx_TARGET@" @ep_redirect_to_file@
