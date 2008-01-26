# -*- coding:windows-1251 -*-
"""
   Recursively scan specified directory to locate files with untranslated
   russian strings. Lines with <!-- NLC --> tag are ignored. Encyclopedia
   image files (.gif) are also ignored.

   If destdir argument is supplied, untranslated files are copied into
   this directory preserving structure.

   Example:
      untranslated.py ../enc_eng
"""

import codecs
import os
import re
import sys

#print "debug на русском".decode("cp1251")

# todo: option to copy files into omegat project with translation memory

def copy_untranslated(files, dst):
  """copy a list of files into dst directory preserving structure"""
  from os import makedirs
  from os.path import join, dirname, exists
  from shutil import copy

  if not exists(dst):
    sys.exit("Error: Destination directory does not exist %s" % dst)

  count = 0
  for e in files:
    #: guard against overwriting files in parent directory
    dstfname = join(dst, e.replace("..", "."))
    if not exists(dirname(dstfname)):
      makedirs(dirname(dstfname))
    copy(e, dstfname)
    count += 1
  print "%s files copied" % count


def grep_files(match, skip_mark, echo=False):
  """
  @param match: string for regexp to match lines
  @param skip: string that do not yield a positive result if matched
  """
  ru = re.compile(match)
  for root,dirs,files in os.walk(sys.argv[1]):
    if ".svn" in dirs:
      dirs.remove(".svn")
    for f in files:
      if f.endswith(".gif"):
        continue
      rucount = 0
      for l in open(os.path.join(root, f), "r"):
        if l.find(skip_mark) != -1:
          continue
        rutext = "".join(ru.findall(l))
        rucount += len(rutext)
        #if rutext: print rutext #.decode("cp1251")
      if rucount:
        if echo:
          print "%s - %d russian symbols" % (os.path.join(root, f), rucount)
        yield os.path.join(root, f)
        
    #print root,dirs,files



if len(sys.argv) < 2:
  print __doc__
  print
  print "Usage: program <sourcedir> [destdir]"
  sys.exit()

if not os.path.exists(sys.argv[1]):
  sys.exit("Specified path not found: %s" % sys.argv[1])

files = list(grep_files("[а-яА-Я] *", "<!-- NLC -->", True))

if len(sys.argv) > 2:
  print "copying files to %s" % sys.argv[2]
  copy_untranslated(files, sys.argv[2])
