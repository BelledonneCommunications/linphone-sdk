
EXTRA_DIST=mediastreamer-config.h.in pkg.list autogen.sh mediastreamer.pc.in mediastreamer2.spec.in mediastreamer2.spec 

ACLOCAL_AMFLAGS = -I m4 $(ACLOCAL_MACOS_FLAGS)

SUBDIRS=po src build m4 include tools tester help

ACLOCAL_FLAGS=-I$(top_srcdir)/m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mediastreamer.pc

unit-test:
	cd tester && $(MAKE) test

## oRTP packaging methods:

# `make rpm'

.phony: rpm

rpm:
	$(MAKE) dist
# <https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=206841>
	TAR_OPTIONS=--wildcards rpmbuild -ta --clean --rmsource --rmspec $(PACKAGE)-$(VERSION).tar.gz

rpm-novideo:
	$(MAKE) dist
# <https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=206841>
	TAR_OPTIONS=--wildcards rpmbuild -ta --clean --rmsource --rmspec --without video $(PACKAGE)-$(VERSION).tar.gz 


# `make package'

if WITH_EPM

.PHONY: package

PKG_NAME = $(PACKAGE)-$(VERSION)-$(RELEASE)
BUILDROOT=`pwd`/epm-install

package: $(srcdir)/pkg.list $(srcdir)/configure
	[ -n "$(BUILDROOT)" ] && rm -rf $(BUILDROOT)/*
	[ -n "$(PKG_NAME).*" ] && rm -f $(PKG_NAME).*
	$(MAKE) install DESTDIR=$(BUILDROOT)
	$(MKEPMLIST) -u $(SYS_USER) -g $(SYS_GROUP) --prefix $(prefix) \
		$(BUILDROOT)/$(prefix) > files.list
	$(EPM) -v -f native -g -n -a $(ARCH) --keep-files --output-dir pkg \
		srcdir=$(srcdir) \
		top_srcdir=$(top_srcdir) \
		top_builddir=$(top_builddir) \
		PACKAGE=$(PACKAGE) \
		SUMMARY="$(SUMMARY)" \
		VERSION=$(ORTP_PKGCONFIG_VERSION) \
		RELEASE=$(RELEASE) \
		LICENSE="$(LICENSE)" \
		VENDOR="$(VENDOR)" \
		PACKAGER="$(PACKAGER)" \
		$(PACKAGE) $(srcdir)/pkg.list
	rm -rf $(BUILDROOT)
	mv -f pkg/$(PACKAGE)-$(ORTP_PKGCONFIG_VERSION)-$(RELEASE).$(EPM_PKG_EXT) $(PKG_NAME).$(ARCH).$(EPM_PKG_EXT)

clean-local:
	rm -rf pkg $(BUILDROOT)
	rm -f files.list

endif WITH_EPM

deb:
	$(MAKE) dist
	mv $(distdir).tar.gz ../mediastreamer2_$(VERSION).orig.tar.gz
	dpkg-buildpackage -us -uc -sa


EXTRA_DIST += README-ANDROID \
build/android/Android.mk \
build/android/basedescs.h \
build/android/libmediastreamer2_AndroidConfig.h \
build/android/voipdescs.h \
build/git-pre-commit-hook \
build/iphone/basedescs.h \
build/iphone/voipdescs.h \
build/winmob/echo/echo.vcproj \
java/.classpath \
java/.project \
java/AndroidManifest.xml \
java/default.properties \
java/proguard.cfg \
java/res/layout-land/main.xml \
java/res/layout/main.xml \
java/res/menu/videocall_activity_menu.xml \
java/res/values/strings.xml \
java/src/org/linphone/mediastream/CpuUtils.java \
java/src/org/linphone/mediastream/Log.java \
java/src/org/linphone/mediastream/Version.java \
java/src/org/linphone/mediastream/video/AndroidVideoWindowImpl.java \
java/src/org/linphone/mediastream/video/capture/AndroidVideoApi5JniWrapper.java \
java/src/org/linphone/mediastream/video/capture/AndroidVideoApi8JniWrapper.java \
java/src/org/linphone/mediastream/video/capture/AndroidVideoApi9JniWrapper.java \
java/src/org/linphone/mediastream/video/capture/hwconf/AndroidCameraConfiguration.java \
java/src/org/linphone/mediastream/video/capture/hwconf/AndroidCameraConfigurationReader5.java \
java/src/org/linphone/mediastream/video/capture/hwconf/AndroidCameraConfigurationReader9.java \
java/src/org/linphone/mediastream/video/capture/hwconf/Hacks.java \
java/src/org/linphone/mediastream/video/display/GL2JNIView.java \
java/src/org/linphone/mediastream/video/display/OpenGLESDisplay.java \
jni/Application.mk \
src/android/libneon/Android.mk \
src/android/libneon/libneon_jni.cc \
src/filter-template.c \
src/ortp-deps/README \
src/ortp-deps/ortp-config-win32.h \
src/ortp-deps/ortp-config.h \
src/ortp-deps/ortp/b64.h \
src/ortp-deps/ortp/logging.h \
src/ortp-deps/ortp/payloadtype.h \
src/ortp-deps/ortp/port.h \
src/ortp-deps/ortp/str_utils.h \
src/ortp-deps/update_ortp.sh \
src/ortp-deps/utils.h \
src/voip/scaler.h \
src/voip/scaler_arm.S \
tools/Android.mk \
tools/filters_graph.py \
tools/ios/en.lproj/InfoPlist.strings \
tools/ios/en.lproj/MainWindow.xib \
tools/ios/en.lproj/mediastreamViewController.xib \
tools/ios/mediastream-Info.plist \
tools/ios/mediastream-Prefix.pch \
tools/ios/mediastreamAppDelegate.h \
tools/ios/mediastreamAppDelegate.m \
tools/ios/mediastreamViewController.h \
tools/ios/mediastreamViewController.m \
tools/ring_jni.c
