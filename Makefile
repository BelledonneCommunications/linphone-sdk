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

.PHONY: build-desktop build-bb10-i486 build-bb10-arm build-bb10

all: build-desktop

build-desktop:
	mkdir -p build-desktop && \
	cd build-desktop && \
	cmake .. $(filter -D%,$(MAKEFLAGS)) && \
	make

clean-desktop:
	cd build-desktop && \
	make clean

veryclean-desktop:
	rm -rf build-desktop

build-bb10-i486:
	mkdir -p liblinphonesdk && \
	mkdir -p build-bb10-i486 && \
	cd build-bb10-i486 && \
	cmake .. -DLINPHONE_BUILDER_TOOLCHAIN=bb10-i486 -DCMAKE_INSTALL_PREFIX=../liblinphonesdk/bb10-i486 $(filter -D%,$(MAKEFLAGS)) && \
	make

build-bb10-arm:
	mkdir -p liblinphonesdk && \
	mkdir -p build-bb10-arm && \
	cd build-bb10-arm && \
	cmake .. -DLINPHONE_BUILDER_TOOLCHAIN=bb10-arm -DCMAKE_INSTALL_PREFIX=../liblinphonesdk/bb10-arm $(filter -D%,$(MAKEFLAGS)) && \
	make

build-bb10: build-bb10-i486 build-bb10-arm

clean-bb10-i486:
	cd build-bb10-i486 && \
	make clean

clean-bb10-arm:
	cd build-bb10-arm && \
	make clean

clean-bb10: clean-bb10-i486 clean-bb10-arm

veryclean-bb10-i486:
	rm -rf build-bb10-i486

veryclean-bb10-arm:
	rm -rf build-bb10-arm

veryclean-bb10: veryclean-bb10-i486 veryclean-bb10-arm
	rm -rf liblinphonesdk

generate-bb10-sdk: build-bb10
	zip -r liblinphone-bb10-sdk.zip \
	liblinphonesdk/bb10-arm \
	liblinphonesdk/bb10-i486
