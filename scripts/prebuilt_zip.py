#/usr/bin/python

import os
import os.path
import re
import shutil
import sys
import tempfile
import zipfile

def main(argv=None):
  if argv is None:
    argv = sys.argv
  if len(argv) == 4:
    log_filename = argv[1]
    install_prefix = argv[2]
    zip_filename = argv[3]
    tempdir = tempfile.mkdtemp()
    rootdir = os.path.join(tempdir, os.path.basename(zip_filename).replace(".zip", ""))
    if not os.path.exists(rootdir):
      os.makedirs(rootdir)
    fin = open(log_filename, 'r')
    regexp1 = re.compile(r'^((.*?)/bin/)?install ((-c|-m [0-9]+) )+(.*)$')
    regexp2 = re.compile(r'^cp -p (.*)$')
    for line in fin:
      l = None
      result = regexp1.match(line)
      if result is not None:
        l = result.group(5).split()
      else:
        result = regexp2.match(line)
        if result is not None:
          l = result.group(1).split()
      if l is not None:
        if len(l) < 2:
          continue
        l = [item.replace('"', '').replace("'", "") for item in l]
        srcs = l[:-1]
        dst = l[-1]
        curpath = dst.replace(install_prefix, '')
        if curpath.startswith('/'):
          curpath = curpath[1:]
        curpath = os.path.join(rootdir, curpath)
        if os.path.isdir(dst):
          if not os.path.exists(curpath):
            os.makedirs(curpath)
          for src in srcs:
            if '*' in src:
              # TODO: handle wildcard and multiline with '\'
              continue
            shutil.copy(os.path.join(dst, os.path.basename(src)), curpath)
        else:
          if not os.path.exists(os.path.dirname(curpath)):
            os.makedirs(os.path.dirname(curpath))
          shutil.copyfile(dst, curpath)
    zipf = zipfile.ZipFile(zip_filename, 'w')
    for root, dirs, files in os.walk(tempdir):
      for f in files:
        fullpath = os.path.join(root, f)
        relativepath = fullpath.replace(tempdir, '')
        if relativepath.startswith('/'):
          relativepath = relativepath[1:]
        zipf.write(fullpath, relativepath, zipfile.ZIP_DEFLATED)
    zipf.close()
    shutil.rmtree(tempdir)

if __name__ == "__main__":
  sys.exit(main())
