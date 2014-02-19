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

.PHONY: build-desktop build-bb10-i486 build-bb10-arm build-bb10

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

veryclean:
	rm -rf WORK && \
	rm -rf OUTPUT

generate-bb10-sdk: build-bb10
	cd OUTPUT && \
	zip -r liblinphone-bb10-sdk.zip liblinphone-bb10-sdk
