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

    if(reason==UPLUG_REASON_QUERY) {
        uplug_setPlugName(data, "Just a Test High-Level Plugin");
        uplug_setPlugLevel(data, UPLUG_LEVEL_HIGH);
    }

    return UPLUG_TOKEN;
}


UPlugTokenReturn myPluginLow (
                  UPlugData *data,
                  UPlugReason reason,
                  UErrorCode *status) {
    fprintf(stderr,"MyPluginLow: data=%p, reason=%s, status=%s\n", (void*)data, udbg_enumName(UDBG_UPlugReason,(int32_t)reason), u_errorName(*status));

    if(reason==UPLUG_REASON_QUERY) {
        uplug_setPlugName(data, "Low Plugin");
        uplug_setPlugLevel(data, UPLUG_LEVEL_LOW);
    }

    return UPLUG_TOKEN;
}

/**
 * Says it's low, but isn't.
 */
UPlugTokenReturn myPluginBad (
                  UPlugData *data,
                  UPlugReason reason,
                  UErrorCode *status) {
    fprintf(stderr,"MyPluginLow: data=%p, reason=%s, status=%s\n", (void*)data, udbg_enumName(UDBG_UPlugReason,(int32_t)reason), u_errorName(*status));

    if(reason==UPLUG_REASON_QUERY) {
        uplug_setPlugName(data, "Bad Plugin");
        uplug_setPlugLevel(data, UPLUG_LEVEL_LOW);
    } else if(reason == UPLUG_REASON_LOAD) {
        void *ctx = uprv_malloc(12345);
        
        uplug_setContext(data, ctx);
        fprintf(stderr,"I'm %p and I did a bad thing and malloced %p\n", (void*)data, (void*)ctx);
    } else if(reason == UPLUG_REASON_UNLOAD) {
        void * ctx = uplug_getContext(data);
        
        uprv_free(ctx);
    }
    

    return UPLUG_TOKEN;
}


UPlugTokenReturn myPluginHigh (
                  UPlugData *data,
                  UPlugReason reason,
                  UErrorCode *status) {
    fprintf(stderr,"MyPluginHigh: data=%p, reason=%s, status=%s\n", (void*)data, udbg_enumName(UDBG_UPlugReason,(int32_t)reason), u_errorName(*status));

    if(reason==UPLUG_REASON_QUERY) {
        uplug_setPlugName(data, "High Plugin");
        uplug_setPlugLevel(data, UPLUG_LEVEL_HIGH);
    }

    return UPLUG_TOKEN;
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
    
    if(reason==UPLUG_REASON_QUERY) {
        uplug_setPlugLevel(data, UPLUG_LEVEL_LOW);
        uplug_setPlugName(data, "Memory Plugin");
    } else if(reason==UPLUG_REASON_LOAD) {
        u_setMemoryFunctions(uplug_getContext(data), &myMemAlloc, &myMemRealloc, &myMemFree, status);
        fprintf(stderr, "MEM: status now %s\n", u_errorName(*status));
    } else if(reason==UPLUG_REASON_UNLOAD) {
        fprintf(stderr, "MEM: not possible to unload this plugin (no way to reset memory functions)...\n");
        uplug_setPlugNoUnload(data, TRUE);
    }

    return UPLUG_TOKEN;
}

