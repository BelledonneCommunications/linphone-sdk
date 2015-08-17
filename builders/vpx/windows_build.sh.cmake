#!/bin/sh

export PATH="@EP_vpx_ENV_PATH@:$PATH"
export INCLUDE="@EP_vpx_ENV_INCLUDE@"
export LIB="@EP_vpx_ENV_LIB@"
export LIBPATH="@EP_vpx_ENV_LIBPATH@"

cd @ep_build@
make V=1
