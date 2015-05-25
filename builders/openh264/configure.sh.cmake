#!/bin/sh

cd @ep_source@
if [ ! -f .out-of-tree-build-patch-applied ]; then
	patch -p1 -i @EP_openh264_OUT_OF_TREE_BUILD_PATCH@ && touch .out-of-tree-build-patch-applied
fi

