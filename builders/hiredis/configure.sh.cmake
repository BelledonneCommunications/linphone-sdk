#!/bin/sh

export RPM_TOPDIR="@LINPHONE_BUILDER_WORK_DIR@/rpmbuild/SOURCES/"
export HIREDIS_TARBALL_URL="https://github.com/redis/hiredis/archive/v0.13.3.tar.gz"

mkdir -p "$RPM_TOPDIR"
echo "be6f1c50fc4d649dd2924f0afecc0a1705dbe0d3  v0.13.3.tar.gz" > "$RPM_TOPDIR/v0.13.3.tar.gz.sha1"

wget "$HIREDIS_TARBALL_URL" -P "$RPM_TOPDIR"

# check Sha1
cd "$RPM_TOPDIR" && sha1sum -c "$RPM_TOPDIR/v0.13.3.tar.gz.sha1"
