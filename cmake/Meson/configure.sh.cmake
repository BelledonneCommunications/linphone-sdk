#!/bin/sh

export PATH="@EP_PROGRAM_PATH@"

cd @EP_BUILD_DIR@

# Setting only prefix is not enough for older versions of Meson, libdir MUST be set to.
@LINPHONESDK_MESON_PROGRAM@ setup @EP_SOURCE_DIR_RELATIVE_TO_BUILD_DIR@ --buildtype @EP_BUILD_TYPE@ --prefix "@CMAKE_INSTALL_PREFIX@" --libdir "@CMAKE_INSTALL_LIBDIR@" @EP_ADDITIONAL_OPTIONS@
