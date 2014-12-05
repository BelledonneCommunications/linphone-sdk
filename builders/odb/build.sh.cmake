#!/bin/sh

export RPM_TOPDIR="@LINPHONE_BUILDER_WORK_DIR@/rpmbuild/RPMS/x86_64"
export ODB_RPM_URL="http://www.codesynthesis.com/download/odb/2.3/odb-2.3.0-1.x86_64.rpm"

mkdir -p "$RPM_TOPDIR"
wget "$ODB_RPM_URL" -P "$RPM_TOPDIR"