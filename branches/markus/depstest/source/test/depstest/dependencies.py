#! /usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2011, International Business Machines
# Corporation and others. All Rights Reserved.
#
# file name: dependencies.py
#
# created on: 2011may26

"""Reader module for dependency data for the ICU dependency tester.

Reads dependencies.txt and makes the data available.

Attributes:
  files: Set of "library/filename.o" files mentioned in the dependencies file.
  items: Map from library or group names to item maps.
         Each item has a "type" ("library" or "group") and optional
         sets of "files" (as in the files attribute) and "deps" (libraries & groups).
         A group item also has a "library" name.
  libraries: Set of library names mentioned in the dependencies file.
  system_symbols: Set of standard-library symbols specified in the dependencies file.
"""
__author__ = "Markus W. Scherer"

import sys

files = set()
items = {}
libraries = set()
system_symbols = set()

_line_number = 0

def _CheckLibraryName(name):
  global _line_number
  if not name:
    sys.exit("Error:%d: \"library: \" without name" % _line_number)
  if name.endswith(".o"):
    sys.exit("Error:%d: invalid library name %s"  % (_line_number, name))

def _CheckGroupName(name):
  global _line_number
  if not name:
    sys.exit("Error:%d: \"group: \" without name" % _line_number)
  if "/" in name or name.endswith(".o"):
    sys.exit("Error:%d: invalid group name %s"  % (_line_number, name))

def _CheckFileName(name):
  global _line_number
  if "/" in name or not name.endswith(".o"):
    sys.exit("Error:%d: invalid file name %s"  % (_line_number, name))

def _RemoveComment(line):
  global _line_number
  _line_number = _line_number + 1
  # TODO: print ">>" + line.rstrip()
  index = line.find("#")  # Remove trailing comment.
  if index >= 0: line = line[:index]
  return line.rstrip()  # Remove trailing newlines etc.

def _ReadLine(f):
  while True:
    line = _RemoveComment(f.next())
    if line: return line

def _ReadFiles(deps_file, item, library_name):
  global files
  item_files = item.get("files")
  while True:
    line = _ReadLine(deps_file)
    if not line: continue
    if not line.startswith("    "): return line
    if item_files == None:
      item["files"] = set()
      item_files = item["files"]
    for file_name in line.split():
      _CheckFileName(file_name)
      file_name = library_name + "/" + file_name
      if file_name in files:
        sys.exit("Error:%d: file %s listed in multiple groups" % (_line_number, file_name))
      files.add(file_name)
      item_files.add(file_name)

def _ReadDeps(deps_file, item, library_name):
  global items, _line_number
  item_deps = item.get("deps")
  while True:
    line = _ReadLine(deps_file)
    if not line: continue
    if not line.startswith("    "): return line
    if item_deps == None:
      item["deps"] = set()
      item_deps = item["deps"]
    for dep in line.split():
      _CheckGroupName(dep)
      if item["type"] == "library" and dep in items and items[dep]["type"] != "library":
            sys.exit("Error:%d: library %s depends on previously defined group %s" %
                     (_line_number, library_name, dep))
      if dep not in items:
        # Add this dependency as a new group.
        items[dep] = {"type": "group", "library": library_name}
      item_deps.add(dep)

def _ReadSystemSymbols(deps_file):
  global system_symbols, _line_number
  while True:
    line = _ReadLine(deps_file)
    if not line: continue
    if not line.startswith("    "): return line
    line = line.lstrip()
    if '"' in line:
      # One double-quote-enclosed symbol on the line, allows spaces in a symbol name.
      symbol = line[1:-1]
      if line.startswith('"') and line.endswith('"') and '"' not in symbol:
        system_symbols.add(symbol)
      else:
        sys.exit("Error:%d: invalid quoted symbol name %s" % (_line_number, line))
    else:
      # One or more space-separate symbols.
      for symbol in line.split(): system_symbols.add(symbol)

def Load():
  """Reads "dependencies.txt" and populates the module attributes."""
  global items, libraries, _line_number
  deps_file = open("dependencies.txt")
  try:
    line = None
    current_type = None
    while True:
      # TODO: print "D  Load() before reading a line"
      while not line: line = _RemoveComment(deps_file.next())
      # TODO: print "D  Load() after reading line >>" + line

      if line.startswith("library: "):
        current_type = "library"
        name = line[9:].lstrip()
        _CheckLibraryName(name)
        if name in items:
          sys.exit("Error:%d: library definition using duplicate name %s" % (_line_number, name))
        # TODO: print "D  adding library " + name
        libraries.add(name)
        items[name] = {"type": "library"}
        line = _ReadFiles(deps_file, items[name], name)
      elif line.startswith("group: "):
        current_type = "group"
        name = line[7:].lstrip()
        _CheckGroupName(name)
        if name not in items:
          sys.exit("Error:%d: group %s defined before mentioned as a dependency" %
                   (_line_number, name))
        item = items[name]
        line = _ReadFiles(deps_file, item, item["library"])
      elif line == "  deps":
        if current_type == "library":
          line = _ReadDeps(deps_file, items[name], name)
        elif current_type == "group":
          item = items[name]
          line = _ReadDeps(deps_file, item, item["library"])
        else:
          sys.exit("Error:%d: deps before any library or group" % _line_number)
      elif line == "system_symbols:":
        line = _ReadSystemSymbols(deps_file)
      else:
        sys.exit("Syntax error:%d: %s" % (_line_number, line))
  except StopIteration:
    pass
