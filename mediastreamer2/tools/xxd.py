#!/usr/bin/python

# Copyright (C) 2016 Belledonne Communications SARL
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

import argparse
import string
import sys


def print_include_hexdump_line(buf):
    hexbuf = [('0x%02x' % ord(i)) for i in buf]
    finalcomma = ''
    if len(hexbuf) == 12:
        finalcomma = ','
    print("  {0}{1}".format(", ".join(hexbuf[i] for i in range(0, len(hexbuf))), finalcomma))

def include_hexdump(infilename):
    varname = infilename.replace('.', '_').replace('-', '_')
    infile = open(infilename, 'r')
    size = 0
    print("unsigned char {0}[] = {{".format(varname))
    while True:
        buf = infile.read(12)
        if not buf:
            break
        print_include_hexdump_line(buf)
        size = size + len(buf)
    print("};")
    print("unsigned int {0}_len = {1};".format(varname, size))

def print_hexdump_line(counter, buf):
    hexbuf = [('%02x' % ord(i)) for i in buf]
    print("{0}: {1:<39}  {2}".format(("%08x" % (counter * 16)),
        ' '.join([''.join(hexbuf[i : i + 2]) for i in range(0, len(hexbuf), 2)]),
        ''.join([c if c in string.printable[:-5] else '.' for c in buf])))

def hexdump(infilename):
    infile = open(infilename, 'r')
    counter = 0
    while True:
        buf = infile.read(16)
        if not buf:
            break
        print_hexdump_line(counter, buf)
        counter += 1


def main(argv = None):
    if argv is None:
        argv = sys.argv
    argparser = argparse.ArgumentParser(description="Make a hexdump.")
    argparser.add_argument('-i', '--include', help="Output in C include file style. A complete static array definition is written (named after the input file).", action='store_true')
    argparser.add_argument('infile', help="File to dump.")
    args = argparser.parse_args()
    if args.include:
        include_hexdump(args.infile)
    else:
        hexdump(args.infile)



if __name__ == "__main__":
    sys.exit(main())
