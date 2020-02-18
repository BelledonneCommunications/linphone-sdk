#!/usr/bin/env python

#
# Copyright (c) 2012-2019 Belledonne Communications SARL.
#
# This file is part of belle-sip.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

import sys
from glob import glob
from subprocess import call

def main():
    print("Deleting old sources...")
    ret = call("rm -f ../grammars/belle_sdp*.c ../grammars/belle_sdp*.h ../grammars/belle_sip_message*.c ../grammars/belle_sip_message*.h", shell=True)
    if ret != 0:
        print("An error occured while deleting old sources")
        return -1
    else:
        print("Done")

    print("Generating sources from belle_sdp.g...")
    ret = call(['java', '-jar', 'antlr-3.4-complete.jar', '-make', '-Xmultithreaded', '-Xconversiontimeout', '10000', '-fo', '../grammars', '../grammars/belle_sdp.g'])
    if ret != 0:
        print("An error occured while generating sources from belle_sdp.g")
        return -1
    else:
        print("Done")

    print("Generating sources from belle_sip_message.g...")
    ret = call(['java', '-jar', 'antlr-3.4-complete.jar', '-make', '-Xmultithreaded', '-Xconversiontimeout', '10000', '-fo', '../grammars', '../grammars/belle_sip_message.g'])
    if ret != 0:
        print("An error occured while generating sources from belle_sip_message.g")
        return -1
    else:
        print("Done")

if __name__ == "__main__":
    sys.exit(main())
