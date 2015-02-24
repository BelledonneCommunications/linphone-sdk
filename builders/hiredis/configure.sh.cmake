#!/bin/sh

export RPM_TOPDIR="@LINPHONE_BUILDER_WORK_DIR@/rpmbuild/SOURCES/"
export HIREDIS_TARBALL_URL="https://github.com/redis/hiredis/archive/v0.11.0.tar.gz"

mkdir -p "$RPM_TOPDIR"
echo "694b6d7a6e4ea7fb20902619e9a2423c014b37c1  v0.11.0.tar.gz" > "$RPM_TOPDIR/v0.11.0.tar.gz.sha1"

wget "$HIREDIS_TARBALL_URL" -P "$RPM_TOPDIR"

# check Sha1
cd "$RPM_TOPDIR" && sha1sum -c "$RPM_TOPDIR/v0.11.0.tar.gz.sha1"