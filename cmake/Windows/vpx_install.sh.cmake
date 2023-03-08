#!/bin/sh

export INCLUDE="@EP_ENV_INCLUDE@"
export LIB="@EP_ENV_LIB@"
export LIBPATH="@EP_ENV_LIBPATH@"

cd @EP_BUILD_DIR@
make install V=1
