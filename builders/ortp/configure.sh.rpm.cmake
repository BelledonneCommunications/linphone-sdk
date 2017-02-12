#!/bin/sh

cd @ep_build@

cmake @ep_source@  -DCPACK_PACKAGE_NAME=bc-ortp -DCMAKE_INSTALL_PREFIX=@RPM_INSTALL_PREFIX@ -DCMAKE_PREFIX_PATH=@RPM_INSTALL_PREFIX@
make package_source
