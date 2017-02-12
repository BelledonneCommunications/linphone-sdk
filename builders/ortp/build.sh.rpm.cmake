#!/bin/bash

export ASFLAGS="@ep_asflags@"
export CPPFLAGS="@ep_cppflags@"
export CFLAGS="@ep_cflags@"
export CXXFLAGS="@ep_cxxflags@"
export OBJCFLAGS="@ep_objcflags@"
export LDFLAGS="@ep_ldflags@"


export RPM_TOPDIR="@LINPHONE_BUILDER_WORK_DIR@/rpmbuild"


rpmbuild -ta --clean --rmsource --define "_topdir $RPM_TOPDIR" \
        @LINPHONE_BUILDER_RPMBUILD_GLOBAL_OPTION@ \
        @LINPHONE_BUILDER_RPMBUILD_OPTIONS@ \
	--rmspec @ep_build@/bc-ortp-*.tar.gz \
        $VERBOSE @ep_redirect_to_file@ 

