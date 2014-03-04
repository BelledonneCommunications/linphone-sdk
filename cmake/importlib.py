#!/usr/bin/python

import os
import re
import subprocess
import sys
import tempfile

def main(argv=None):
  if argv is None:
      argv = sys.argv
  if len(argv) >= 3:
    dllfile = argv[1]
    dllpath, dllextension = os.path.splitext(dllfile)
    dllname = os.path.basename(dllpath)
    libfile = argv[2]
    deffile = os.path.join(tempfile.gettempdir(), dllname + ".def")
    ret = subprocess.check_output(["dumpbin", "/exports", dllfile, "/out:" + deffile], stderr=subprocess.STDOUT)
    fin = open(deffile, "r")
    lines = fin.readlines()
    exports = []
    for line in lines:
      exportre = re.compile('^\s+\d+\s+[\dA-F]+\s+[\dA-F]+\s+(\w+)(\s=\s\w+)?$')
      m = exportre.match(line)
      if m:
        exports.append(m.group(1))
    fin.close()
    fin = open(deffile, "w")
    fin.write("EXPORTS " + dllname + "\n")
    for export in exports:
      fin.write("\t" + export + "\n")
    fin.close()
    ret = subprocess.check_output(["lib", "/def:" + deffile, "/out:" + libfile, "/machine:X86"], stderr=subprocess.STDOUT)
    print(ret)

if __name__ == "__main__":
    sys.exit(main())
