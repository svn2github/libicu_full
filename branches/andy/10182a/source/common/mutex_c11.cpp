//
//   Example ICU build time custom mutex cpp file.
//
//   Must implement these functions:
//     umtx_lock(UMutex *mutex);
//     umtx_unlock(UMutex *mutex);
//     umtx_initImplPreInit(UInitOnce &uio);
//     umtx_initImplPostInit(UInitOnce &uio);


U_CAPI void  U_EXPORT2
umtx_lock(UMutex *mutex) {
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    mutex->fMutex.lock();
    // TODO: handle exceptions?
}


U_CAPI void  U_EXPORT2
umtx_unlock(UMutex* mutex)
{
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    mutex->fMutex.unlock();
}

// A mutex and a condition variable that are used by the implementaion of umtx_initOnce
//   (C++11 specific. Platforms may use whatever strategy is convenient to
//   block threads that need to wait for an initialization to complete.)

static std::mutex               initMutex;
static std::condition_variable  initCondition;

// This function is called from umtx_initOnce() when a test of a UInitOnce::fState 
//   reveals that initialization has not completed, that we either need to call the
//   function on this thread, or wait for some other thread to complete the initialization.
//
// The actual call to the init function is made inline by template code
//   that knows the C++ types involved. This function returns TRUE if
//   the caller needs to invoke the Init function, or FALSE if the initialization
//   has completed on another thread.
//
U_CAPI UBool U_EXPORT2 
umtx_initImplPreInit(UInitOnce &uio) {
    std::unique_lock<std::mutex> initLock(initMutex);
    int32_t state = uio.fState;
    if (state == 0) {
        umtx_storeRelease(uio.fState, 1);
        return TRUE;   // Caller will next call the init function.
    } else {
        while (uio.fState == 1) {
            // Another thread is currently running the initialization.
            // Wait until it completes.
            initCondition.wait(initLock);
        }
        U_ASSERT(uio.fState == 2);
        return FALSE;
    }
}


// This function is called by the thread that ran an initialization function,
// just after completing the function.
//   Some threads may be waiting on the condition, requiring the broadcast wakeup.
//   Some threads may be racing to test the fState variable outside of the mutex, 
//   requiring the use of store/release when changing its value.

U_CAPI void U_EXPORT2 
umtx_initImplPostInit(UInitOnce &uio) {
    std::unique_lock<std::mutex> initLock(initMutex);
    umtx_storeRelease(uio.fState, 2);
    initCondition.notify_all();
}

