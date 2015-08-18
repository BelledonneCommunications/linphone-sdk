#!/bin/sh

export PATH="$PATH:@EP_vpx_ENV_PATH@"
export INCLUDE="@EP_vpx_ENV_INCLUDE@"
export LIB="@EP_vpx_ENV_LIB@"
export LIBPATH="@EP_vpx_ENV_LIBPATH@"

cd @ep_build@
make V=1 @ep_redirect_to_file@
