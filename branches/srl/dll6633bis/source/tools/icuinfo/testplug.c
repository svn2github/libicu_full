#include "unicode/icuplug.h"
#include <stdio.h>
#include "unicode/udbgutil.h"
#include "unicode/uclean.h"
#include "cmemory.h"

UPlugTokenReturn myPlugin (
                  UPlugData *data,
                  UPlugReason reason,
                  UErrorCode *status) {
    fprintf(stderr,"MyPlugin: data=%p, reason=%s, status=%s\n", (void*)data, udbg_enumName(UDBG_UPlugReason,(int32_t)reason), u_errorName(*status));

    return U_PLUG_TOKEN;
}


/*  Debug Memory Plugin (see hpmufn.c) */
static void * U_CALLCONV myMemAlloc(const void *context, size_t size) {
    void *retPtr = (void *)malloc(size);
    fprintf(stderr, "MEM: malloc(%d) = %p\n", size, retPtr);
    return retPtr;
}

static void U_CALLCONV myMemFree(const void *context, void *mem) {
    free(mem);
    fprintf(stderr, "MEM: free(%p)\n", mem);
}

static void * U_CALLCONV myMemRealloc(const void *context, void *mem, size_t size) {
    void *retPtr;
    
    if(mem==NULL) {
        retPtr = NULL;
    } else {
        retPtr = realloc(mem, size);
    }
    fprintf(stderr, "MEM: realloc(%p, %d) = %p\n", mem, size, retPtr);
    return retPtr;
}

UPlugTokenReturn debugMemoryPlugin (
                  UPlugData *data,
                  UPlugReason reason,
                  UErrorCode *status) {
    fprintf(stderr,"debugMemoryPlugin: data=%p, reason=%s, status=%s\n", (void*)data, udbg_enumName(UDBG_UPlugReason,(int32_t)reason), u_errorName(*status));
    
    if(reason==UPLUG_REASON_LOAD) {
        u_setMemoryFunctions(uplug_getContext(data), &myMemAlloc, &myMemRealloc, &myMemFree, status);
        fprintf(stderr, "MEM: status now %s\n", u_errorName(*status));
    } else if(reason==UPLUG_REASON_UNLOAD) {
        fprintf(stderr, "MEM: can't unload...\n");
        uplug_setPlugNoUnload(data, TRUE);
    }

    return U_PLUG_TOKEN;
}

