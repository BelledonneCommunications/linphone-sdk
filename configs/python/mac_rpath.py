#!/usr/bin/python

import sys
import getopt
import os
import subprocess
import re


# List directory files
def list_files(source_dir):
  source_dir = os.path.abspath(source_dir)
 
  # Build file list
  filelist = []
  for file in os.listdir(source_dir):
    if os.path.isdir(os.path.join(source_dir, file)) ==  False:
      root, ext = os.path.splitext(file)
      if ext == '.dylib' or ext == '.so':
        filelist.append(file)
  return filelist

# List libraries used in a file
def list_libraries(file):
  print "Exec: /usr/bin/otool -L %s" % file 
  ret = subprocess.check_output(["/usr/bin/otool", "-L", file])

  librarylist = []
  for a in ret.split("\n"):
    pop = re.search("^\s+(\S*).*$", a)
    if pop:
      librarylist.append(pop.group(1))
  return librarylist

# Change library id
def change_library_id(file, id):
  id = "@loader_path/" + id
  os.chmod(file, 0o755)
  print "%s: Change id %s" % (file, id)
  ret = subprocess.check_output(["/usr/bin/install_name_tool", "-id", id, file], stderr=subprocess.STDOUT)

# Change path to a library in a file
def change_library_path(file, old, new, path = ""):
  if len(path)> 0 and path[-1] <> '/':
    path = path + "/"
  new = "@loader_path/" + path + new
  os.chmod(file, 0o755)
  print "%s: Replace %s -> %s" % (file, old, new)
  ret = subprocess.check_output(["/usr/bin/install_name_tool", "-change", old, new, file])

# Replace libraries used by a file
def replace_libraries(file, name, libraries, path = ""):
  print "---------------------------------------------"
  print "Replace libraries in %s" % file
  change_library_id(file, name)
  librarylist = list_libraries(file)
  for lib in libraries:
    if lib <> name:
      completelib = [s for s in librarylist if lib in s]
      if len(completelib) == 1:
         change_library_path(file, completelib[0], lib, path)
  print "---------------------------------------------"

def main(argv=None):
  if argv is None:
      argv = sys.argv
  if len(argv) > 1 and len(argv) <= 3:
    path = argv[1]
    file_list = list_files(path)
    if len(argv) <= 2:
      for file in file_list:
        replace_libraries(os.path.join(path, file), file, file_list)
    else:
      path2 = argv[2]
      if os.path.isdir(path2):
        file_list2 = list_files(path2)
        for file in file_list2:
          replace_libraries(os.path.join(path2, file), file, file_list, os.path.relpath(path, path2))
      else:
        file = argv[2]
        replace_libraries(file, os.path.basename(file), file_list, os.path.relpath(path, os.path.dirname(file)))


if __name__ == "__main__":
    sys.exit(main())
