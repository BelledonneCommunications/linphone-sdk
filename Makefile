############################################################################
# Makefile
# Copyright (C) 2014  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

NUMCPUS?=$(shell grep -c '^processor' /proc/cpuinfo || echo "1" )

.PHONY: build-desktop build-bb10-i486 build-bb10-arm build-bb10 build-ios-i386 build-ios-armv7 build-ios-armv7s

all: build-desktop

build-desktop:
	mkdir -p WORK/cmake-desktop && \
	cd WORK/cmake-desktop && \
	cmake ../.. $(filter -D%,$(MAKEFLAGS)) && \
	make -j $(NUMCPUS)

clean-desktop:
	rm -rf WORK/Build-desktop && \
	rm -rf WORK/tmp-desktop

build-bb10-i486:
	mkdir -p OUTPUT/liblinphone-bb10-sdk && \
	mkdir -p WORK/cmake-bb10-i486 && \
	cd WORK/cmake-bb10-i486 && \
	cmake ../.. -DLINPHONE_BUILDER_TOOLCHAIN=bb10-i486 -DCMAKE_PREFIX_PATH=../../OUTPUT/liblinphone-bb10-sdk/i486 -DCMAKE_INSTALL_PREFIX=../../OUTPUT/liblinphone-bb10-sdk/i486 $(filter -D%,$(MAKEFLAGS)) && \
	make -j $(NUMCPUS)

build-bb10-arm:
	mkdir -p OUTPUT/liblinphone-bb10-sdk && \
	mkdir -p WORK/cmake-bb10-arm && \
	cd WORK/cmake-bb10-arm && \
	cmake ../.. -DLINPHONE_BUILDER_TOOLCHAIN=bb10-arm -DCMAKE_PREFIX_PATH=../../OUTPUT/liblinphone-bb10-sdk/arm -DCMAKE_INSTALL_PREFIX=../../OUTPUT/liblinphone-bb10-sdk/arm $(filter -D%,$(MAKEFLAGS)) && \
	make -j $(NUMCPUS)

build-bb10: build-bb10-i486 build-bb10-arm

clean-bb10-i486:
	rm -rf WORK/Build-bb10-i486 && \
	rm -rf WORK/tmp-bb10-i486 && \
	rm -rf OUTPUT/liblinphone-bb10-sdk/i486

clean-bb10-arm:
	rm -rf WORK/Build-bb10-arm && \
	rm -rf WORK/tmp-bb10-arm && \
	rm -rf OUTPUT/liblinphone-bb10-sdk/arm

clean-bb10: clean-bb10-i486 clean-bb10-arm

help-bb10:
	mkdir -p OUTPUT/liblinphone-bb10-sdk && \
	mkdir -p WORK/cmake-bb10-i486 && \
	cd WORK/cmake-bb10-i486 && \
	cmake ../.. -DLINPHONE_BUILDER_TOOLCHAIN=bb10-i486 -DCMAKE_PREFIX_PATH=../../OUTPUT/liblinphone-bb10-sdk/i486 -DCMAKE_INSTALL_PREFIX=../../OUTPUT/liblinphone-bb10-sdk/i486 $(filter -D%,$(MAKEFLAGS)) -LH

generate-bb10-sdk: build-bb10
	cd OUTPUT && \
	zip -r liblinphone-bb10-sdk.zip liblinphone-bb10-sdk

build-ios-i386:
	mkdir -p OUTPUT/liblinphone-ios-sdk && \
	mkdir -p WORK/cmake-ios-i386 && \
	cd WORK/cmake-ios-i386 && \
	cmake ../.. -DLINPHONE_BUILDER_TOOLCHAIN=ios-i386 -DCMAKE_PREFIX_PATH=../../OUTPUT/liblinphone-ios-sdk/i386 -DCMAKE_INSTALL_PREFIX=../../OUTPUT/liblinphone-ios-sdk/i386 $(filter -D%,$(MAKEFLAGS)) && \
	make -j $(NUMCPUS)

build-ios-armv7:
	mkdir -p OUTPUT/liblinphone-ios-sdk && \
	mkdir -p WORK/cmake-ios-armv7 && \
	cd WORK/cmake-ios-armv7 && \
	cmake ../.. -DLINPHONE_BUILDER_TOOLCHAIN=ios-armv7 -DCMAKE_PREFIX_PATH=../../OUTPUT/liblinphone-ios-sdk/armv7 -DCMAKE_INSTALL_PREFIX=../../OUTPUT/liblinphone-ios-sdk/armv7 $(filter -D%,$(MAKEFLAGS)) && \
	make -j $(NUMCPUS)

build-ios-armv7s:
	mkdir -p OUTPUT/liblinphone-ios-sdk && \
	mkdir -p WORK/cmake-ios-armv7s && \
	cd WORK/cmake-ios-armv7s && \
	cmake ../.. -DLINPHONE_BUILDER_TOOLCHAIN=ios-armv7s -DCMAKE_PREFIX_PATH=../../OUTPUT/liblinphone-ios-sdk/armv7s -DCMAKE_INSTALL_PREFIX=../../OUTPUT/liblinphone-ios-sdk/armv7s $(filter -D%,$(MAKEFLAGS)) && \
	make -j $(NUMCPUS)

build-ios: build-ios-i386 build-ios-armv7 build-ios-armv7s

clean-ios-i386:
	rm -rf WORK/Build-ios-i386 && \
	rm -rf WORK/tmp-ios-i386 && \
	rm -rf OUTPUT/liblinphone-ios-sdk/i386

clean-ios-armv7:
	rm -rf WORK/Build-ios-armv7 && \
	rm -rf WORK/tmp-ios-armv7 && \
	rm -rf OUTPUT/liblinphone-ios-sdk/armv7

clean-ios-armv7s:
	rm -rf WORK/Build-ios-armv7s && \
	rm -rf WORK/tmp-ios-armv7s && \
	rm -rf OUTPUT/liblinphone-ios-sdk/armv7s

clean-ios: clean-ios-i386 clean-ios-armv7 clean-ios-armv7s

help-ios:
	mkdir -p OUTPUT/liblinphone-ios-sdk && \
	mkdir -p WORK/cmake-ios-i386 && \
	cd WORK/cmake-ios-i386 && \
	cmake ../.. -DLINPHONE_BUILDER_TOOLCHAIN=ios-i386 -DCMAKE_PREFIX_PATH=../../OUTPUT/liblinphone-ios-sdk/i386 -DCMAKE_INSTALL_PREFIX=../../OUTPUT/liblinphone-ios-sdk/i386 $(filter -D%,$(MAKEFLAGS)) -LH

generate-ios-sdk: build-ios
	arm_archives=`find OUTPUT/liblinphone-ios-sdk/armv7 -name *.a` && \
	mkdir -p OUTPUT/liblinphone-ios-sdk/apple-darwin && \
	cp -rf OUTPUT/liblinphone-ios-sdk/armv7/include OUTPUT/liblinphone-ios-sdk/apple-darwin/. && \
	cp -rf OUTPUT/liblinphone-ios-sdk/armv7/share OUTPUT/liblinphone-ios-sdk/apple-darwin/. && \
	for archive in $$arm_archives ; do \
		i386_path=`echo $$archive | sed -e "s/armv7/i386/"` ;\
		armv6_path=`echo $$archive | sed -e "s/armv7/armv6/"` ;\
		if  test ! -f "$$armv6_path"; then \
			armv6_path= ; \
		fi; \
		armv7s_path=`echo $$archive | sed -e "s/armv7/armv7s/"` ;\
		if  test ! -f "$$armv7s_path"; then \
			armv7s_path= ; \
		fi; \
		destpath=`echo $$archive | sed -e "s/-debug//"` ;\
		destpath=`echo $$destpath | sed -e "s/armv7/apple-darwin/"` ;\
		if test -f "$$i386_path"; then \
			echo "Mixing $$archive into $$destpath"; \
			mkdir -p `dirname $$destpath` ; \
			lipo -create $$archive $$armv7s_path $$armv6_path $$i386_path -output $$destpath; \
		else \
			echo "WARNING: archive `basename $$archive` exists in arm tree but does not exists in i386 tree."; \
		fi \
	done && \
	cd OUTPUT && \
	zip -r liblinphone-ios-sdk.zip liblinphone-ios-sdk/apple-darwin

veryclean:
	rm -rf WORK && \
	rm -rf OUTPUT

