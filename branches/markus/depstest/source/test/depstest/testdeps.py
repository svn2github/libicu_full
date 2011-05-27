#! /usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2011, International Business Machines
# Corporation and others. All Rights Reserved.
#
# file name: testdeps.py
#
# created on: 2011may24

"""ICU dependency tester.

This probably works only on Linux.

The exit code is 0 if everything is fine, 1 for errors, 2 for only warnings.

Sample invocation:
  ~/svn.icu/trunk/src/source/test/depstest$ ./testdeps.py ~/svn.icu/trunk/dbg
"""

__author__ = "Markus W. Scherer"

import glob
import os.path
import subprocess
import sys

import dependencies

_ignored_symbols = set()
_obj_files = {}
_symbols_to_files = {}
_return_value = 0

def _ReadObjFile(root_path, library_name, obj_name):
  global _ignored_symbols, _obj_files, _symbols_to_files
  lib_obj_name = library_name + "/" + obj_name
  if lib_obj_name in _obj_files:
    print "duplicate " + lib_obj_name
    return

  path = os.path.join(root_path, library_name, obj_name)
  nm_result = subprocess.Popen(["nm", "--demangle", "--format=sysv",
                                "--extern-only", "--no-sort", path],
                               stdout=subprocess.PIPE).communicate()[0]
  obj_imports = set()
  obj_exports = set()
  for line in nm_result.splitlines():
    fields = line.split("|")
    if len(fields) == 1: continue
    name = fields[0].strip()
    # Ignore symbols like '__cxa_pure_virtual',
    # 'vtable for __cxxabiv1::__si_class_type_info' or
    # 'DW.ref.__gxx_personality_v0'.
    if name.startswith("_") or "__" in name:
      _ignored_symbols.add(name)
      continue
    type = fields[2].strip()
    if type == "U":
      obj_imports.add(name)
    else:
      # TODO: Investigate weak symbols (V, W) with or without values.
      obj_exports.add(name)
      _symbols_to_files[name] = lib_obj_name
  _obj_files[lib_obj_name] = {"imports": obj_imports, "exports": obj_exports}

def _ReadLibrary(root_path, library_name):
  obj_paths = glob.glob(os.path.join(root_path, library_name, "*.o"))
  for path in obj_paths:
    _ReadObjFile(root_path, library_name, os.path.basename(path))

def _GetExports(name, parents):
  global _ignored_symbols, _obj_files, _symbols_to_files, _return_value
  item = dependencies.items[name]
  item_type = item["type"]
  if name in parents:
    sys.exit("Error: %s %s has a circular dependency on itself: %s" %
             (item_type, name, parents))
  # TODO: print "** %s %s" % (parents, name)
  # Check if already cached.
  exports = item.get("exports")
  if exports != None: return exports
  # Calculcate recursively.
  parents.append(name)
  imports = set()
  exports = set()
  files = item.get("files")
  if files:
    for file_name in files:
      obj_file = _obj_files[file_name]
      imports |= obj_file["imports"]
      exports |= obj_file["exports"]
  deps = item.get("deps")
  if deps:
    for dep in deps:
      exports |= _GetExports(dep, parents)
  item["exports"] = exports
  imports -= exports | dependencies.system_symbols | _ignored_symbols
  if imports:
    for symbol in imports:
      sys.stderr.write("Error:  %s %s  imports  %s  but does not depend on  %s\n" %
                       (item_type, name, symbol, _symbols_to_files.get(symbol)))
      _return_value = 1
  del parents[-1]
  return exports

def main():
  global _ignored_symbols, _obj_files, _return_value
  if len(sys.argv) <= 1:
    sys.exit(("Command line error: " +
             "need one argument with the root path to the built ICU libraries/*.o files."))
  dependencies.Load()
  for library_name in dependencies.libraries:
    _ReadLibrary(sys.argv[1], library_name)
  o_files_set = set(_obj_files.keys())
  files_missing_from_deps = o_files_set - dependencies.files
  files_missing_from_build = dependencies.files - o_files_set
  if files_missing_from_deps:
    sys.stderr.write("Error: files missing from dependencies.txt:\n%s\n" %
                     files_missing_from_deps)
    _return_value = 1
  if files_missing_from_build:
    sys.stderr.write("Error: files in dependencies.txt but not built:\n%s\n" %
                     files_missing_from_build)
    _return_value = 1
  if _ignored_symbols:
    sys.stderr.write(("Warning: ignored symbols, " +
                      "ones from ICU should be internal or be renamed:\n%s\n") %
                     _ignored_symbols)
    _return_value = 2
  # TODO: only check further if not _return_value
  # TODO: check all libraries
  _GetExports("uts46", [])
  # TODO: print ".o files:\n%s" % _obj_files
  # TODO: print "items:\n%s" % dependencies.items
  return _return_value

if __name__ == "__main__":
  sys.exit(main())
