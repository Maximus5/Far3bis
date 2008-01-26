# -*- coding:windows-1251 -*-
"""
   Recursively scan specified directory to locate files with untranslated
   russian strings. Lines with <!-- NLC --> tag are ignored. Encyclopedia
   image files (.gif) are also ignored.

   Example:
      untranslated.py ../enc_eng
"""

import codecs
import os
import re
import sys

#print "debug на русском".decode("cp1251")

if len(sys.argv) < 2:
  print __doc__
  print
  print "Usage: program <dir>"
  sys.exit()


if not os.path.exists(sys.argv[1]):
  sys.exit("Specified path not found: %s" % sys.argv[1])

ru = re.compile("[а-яА-Я] *")
skip_mark = "<!-- NLC -->"
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
      print "%s - %d russian symbols" % (os.path.join(root, f), rucount)
      
  #print root,dirs,files
