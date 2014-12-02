About CMAKE and ICU.

* right now, data generation, tests, samples aren't working.

* Run cmake like this:

cmake -DENABLE_TESTS=OFF -DENABLE_LAYOUT=OFF -DENABLE_SAMPLES=OFF -G Ninja
cmake -DENABLE_TESTS=OFF -DENABLE_LAYOUT=OFF -DENABLE_SAMPLES=OFF -G 'Unix Makefiles'

( use "cmake --help" to see what options are possible for -G )

* to regenerate the cmake lists:

  ( cd source/test/depstest/ && ./gencmake.py )

  (this updates source/cmake-modules/IcuPaths.cmake - don't forget to check in)
  ( if it's wrong, change:  source/test/depstest/dependencies.txt )




