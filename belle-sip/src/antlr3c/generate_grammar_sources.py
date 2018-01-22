#!/usr/bin/env python

############################################################################
# generate_grammar_sources.py
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

import sys
from subprocess import call

def main():
    print("Deleting old sources...")
    ret = call("rm ../grammars/belle_sdp*.c ../grammars/belle_sdp*.h ../grammars/belle_sip_message*.c ../grammars/belle_sip_message*.h", shell=True)
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
