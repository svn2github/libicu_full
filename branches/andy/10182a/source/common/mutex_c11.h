
// Example of an ICU build time customized mutex header.
//
//   Must define struct UMutex and an initializer that will work with static instances.
//   All UMutex instances in ICU code will be static.

#ifndef ICU_MUTEX_C11_H
#define ICU_MUTEX_C11_H

#include <mutex>
#include <condition_variable>

struct UMutex {
    std::mutex      fMutex;
};

#define U_MUTEX_INITIALIZER  {}

#endif
