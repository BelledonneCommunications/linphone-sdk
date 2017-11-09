#!/bin/sh

export RPM_TOPDIR="@LINPHONE_BUILDER_WORK_DIR@/rpmbuild"


cd @ep_build@

# create rpmbuild workdir if needed
for dir in BUILDROOT RPMS SOURCES SPECS SRPMS; do
	mkdir -p "$RPM_TOPDIR/$dir"
done

if [ ! -f @ep_config_h_file@ ]
then
	cmake @ep_source@ -DCMAKE_INSTALL_PREFIX=@RPM_INSTALL_PREFIX@ -DCPACK_PACKAGE_NAME=bc-bctoolbox -DCPACK_GENERATOR=RPM \
	`echo "@EP_bctoolbox_CMAKE_OPTIONS@" | sed 's/;/ /g'`
	make package
	cp -v *.rpm "$RPM_TOPDIR/RPMS/"
fi
