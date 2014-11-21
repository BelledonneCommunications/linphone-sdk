#!/usr/bin/python

import distutils.util
import sys

def main(argv=None):
    print(';'.join(distutils.util.get_platform().split('-')[1:]))

if __name__ == "__main__":
    sys.exit(main())
