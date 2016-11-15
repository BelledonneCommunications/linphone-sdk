#!/bin/sh

export PATH="$PATH:@VPX_ENV_PATH@"
export INCLUDE="@VPX_ENV_INCLUDE@"
export LIB="@VPX_ENV_LIB@"
export LIBPATH="@VPX_ENV_LIBPATH@"

cd @ep_build@
make V=1 @ep_redirect_to_file@
