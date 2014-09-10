ICU Paragraph Layout Library
===

As of ICU 54, the [Layout Engine](http://userguide.icu-project.org/layoutengine) is now deprecated.
However, this library (the paragraph layout library) may still be used as a separate library on top
of HarfBuzz.

Prerequisites:

* HarfBuzz and icu-le-hb
* ICU
* CMake
* pkg-config (make sure each package you install is pkg-config accessible)

To build libiculx:

* Build and install a complete ICU with `--disable-layout`.
* Build and install [HarfBuzz](http://harfbuzz.org).
* Build and install the [icu-le-hb](http://harfbuzz.org) library.
* Now, build layoutex with CMake: (Unix Makefile build shown)

      `cmake -G 'Unix Makefiles' && make`

* you now have `libiculx`.


Copyright (C) 2014, International Business Machines and Others. All Rights Reserved.
