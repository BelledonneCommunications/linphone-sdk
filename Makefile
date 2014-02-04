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

build-bb10-sdk:
	export TOOLCHAIN=bb10-i486 && ./scripts/build.sh
	export TOOLCHAIN=bb10-arm && ./scripts/build.sh

generate-bb10-sdk: build-bb10-sdk
	cd .. && \
	zip -r liblinphone-bb10-sdk.zip \
	liblinphonesdk/bb10-arm \
	liblinphonesdk/bb10-i486

clean-bb10-sdk:
	export TOOLCHAIN=bb10-i486 && ./scripts/clean.sh
	export TOOLCHAIN=bb10-arm && ./scripts/clean.sh

veryclean-bb10-sdk:
	rm -rf build-bb10-i486
	rm -rf build-bb10-arm
	rm -rf ../liblinphonesdk
