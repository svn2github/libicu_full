/*
******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
*  FILE NAME : icuplug.c
*
*   Date         Name        Description
*   10/29/2009   sl          New.
******************************************************************************
*/

#include "unicode/icuplug.h"
#include "icuplugimp.h"
#include "cstring.h"
#include "cmemory.h"
#include "putilimp.h"
#include "ucln.h"

#define UPLUG_TRACE 1
#if UPLUG_TRACE
#include <stdio.h>
#define DBG(x) printf("%s:%d: ",__FILE__,__LINE__); printf x;
#else
#define DBG(x)
#endif

#pragma mark overall globals

#define UPLUG_LIBRARY_INITIAL_COUNT 8
#define UPLUG_PLUGIN_INITIAL_COUNT 12

#pragma mark NOTES
/*
 - have a callback to determine HIGH or LOW level
 - automatically run all LOW level then HIGH level plugins in order
- have icuinfo give diagnostics and info
 - have icuinfo give sample config file and location.
 
  */

#pragma mark internal

/**
 * Remove an item
 * @param list the full list
 * @param listSize the number of entries in the list
 * @param memberSize the size of one member
 * @param itemToRemove the item number of the member
 * @return the new listsize 
 */
static int32_t uplug_removeEntryAt(void *list, int32_t listSize, int32_t memberSize, int32_t itemToRemove) {
    uint8_t *bytePtr = (uint8_t *)list;
    
    /* get rid of some bad cases first */
    if(listSize<1) {
        return listSize;
    }
    
    /* is there anything to move? */
    if(listSize > itemToRemove+1) {
        memmove(bytePtr+(itemToRemove*memberSize), bytePtr+((itemToRemove+1)*memberSize), memberSize);
    }
    
    return listSize-1;
}


#pragma mark Library management


/**
 * Library management. Internal. 
 * @internal
 */
struct UPlugLibrary;

/**
 * Library management. Internal. 
 * @internal
 */
typedef struct UPlugLibrary {
    void *lib;                           /**< library ptr */
    char name[UPLUG_NAME_MAX]; /**< library name */
    uint32_t ref;                        /**< reference count */
} UPlugLibrary;

UPlugLibrary   staticLibraryList[UPLUG_LIBRARY_INITIAL_COUNT];
UPlugLibrary * libraryList = staticLibraryList;
int32_t libraryCount = 0;
int32_t libraryMax = UPLUG_LIBRARY_INITIAL_COUNT;

/**
 * Search for a library. Doesn't lock
 * @param libName libname to search for
 * @return the library's struct
 */
static int32_t searchForLibraryName(const char *libName) {
    int32_t i;
    
    for(i=0;i<libraryCount;i++) {
        if(!uprv_strcmp(libName, libraryList[i].name)) {
            return i;
        }
    }
    return -1;
}

static int32_t searchForLibrary(void *lib) {
    int32_t i;
    
    for(i=0;i<libraryCount;i++) {
        if(lib==libraryList[i].lib) {
            return i;
        }
    }
    return -1;
}

U_INTERNAL char * U_EXPORT2
uplug_findLibrary(void *lib, UErrorCode *status) {
    int32_t libEnt;
    char *ret = NULL;
    if(U_FAILURE(*status)) {
        return NULL;
    }
    libEnt = searchForLibrary(lib);
    if(libEnt!=-1) { 
        ret = libraryList[libEnt].name;
    } else {
        *status = U_MISSING_RESOURCE_ERROR;
    }
    return ret;
}

U_INTERNAL void * U_EXPORT2
uplug_openLibrary(const char *libName, UErrorCode *status) {
    int32_t libEntry = -1;
    void *lib = NULL;
    
    DBG(("uplug_openLibrary(%s,%s)\n", libName, u_errorName(*status)))
    
    if(U_FAILURE(*status)) return NULL;

    DBG(("uplug_openLibrary(%s,%s)\n", libName, u_errorName(*status)))
    
    libEntry = searchForLibraryName(libName);
    DBG(("uplug_openLibrary(%s,%s) libEntry %d\n", libName, u_errorName(*status), libEntry))
    if(libEntry == -1) {
        libEntry = libraryCount++;
        
        /* Some operating systems don't want 
           DL operations from multiple threads. */
        libraryList[libEntry].lib = uprv_dl_open(libName, status);
        DBG(("uplug_openLibrary(%s,%s) libEntry %d, lib %p\n", libName, u_errorName(*status), libEntry, lib))
        
        if(libraryList[libEntry].lib == NULL || U_FAILURE(*status)) {
            /* cleanup. */
            libraryList[libEntry].lib = NULL; /* failure with open */
            libraryList[libEntry].name[0] = 0;
            DBG(("uplug_openLibrary(%s,%s) libEntry %d, lib %p\n", libName, u_errorName(*status), libEntry, lib))
            /* no need to free - just won't increase the count. */
            DBG(("uplug_openLibrary(%s,%s) libEntry %d, lib %p\n", libName, u_errorName(*status), libEntry, lib))
            libraryCount--;
        } else { /* is it still there? */
            /* link it in */
            strncpy(libraryList[libEntry].name,libName,UPLUG_NAME_MAX);
            libraryList[libEntry].ref=1;
            lib = libraryList[libEntry].lib;
        }

    } else {
        lib = libraryList[libEntry].lib;
        libraryList[libEntry].ref++;
    }
    DBG(("uplug_openLibrary(%s,%s) libEntry %d, lib %p, ref %d\n", libName, u_errorName(*status), libEntry, lib, libEntry?libraryList[libEntry].ref:-1))
    return lib;
}

U_INTERNAL void U_EXPORT2
uplug_closeLibrary(void *lib, UErrorCode *status) {
    int32_t i;
    
    DBG(("uplug_closeLibrary(%p,%s) list %p\n", lib, u_errorName(*status), (void*)libraryList))
    if(U_FAILURE(*status)) return;
    
    DBG(("uplug_closeLibrary(%p,%s) list %p\n", lib, u_errorName(*status), (void*)libraryList))
    for(i=0;i<libraryCount;i++) {
        if(lib==libraryList[i].lib) {
            DBG(("uplug_closeLibrary(%p,%s)  found #%d %p ref %d\n", lib, u_errorName(*status), i, (void*)&libraryList[i], libraryList[i].ref))
            if(--(libraryList[i].ref) == 0) {
                DBG(("uplug_closeLibrary(%p,%s)  entryToClose #%d %p ref %d\n", lib, u_errorName(*status), i, (void*)&libraryList[i], libraryList[i].ref))
                uprv_dl_close(libraryList[i].lib, status);
                libraryCount = uplug_removeEntryAt(libraryList, libraryCount, sizeof(*libraryList), i);
            }
            return;
        }
    }
    *status = U_INTERNAL_PROGRAM_ERROR; /* could not find the entry! */
}

#pragma mark Plugin globals 

UPlugData pluginList[UPLUG_PLUGIN_INITIAL_COUNT];
int32_t pluginCount = 0;

#pragma mark Plug Iteration

  
static int32_t uplug_pluginNumber(UPlugData* d) {
    UPlugData *pastPlug = &pluginList[pluginCount];
    if(d<=pluginList) {
        return 0;
    } else if(d>=pastPlug) {
        return pluginCount;
    } else {
        return (d-pluginList)/sizeof(pluginList[0]);
    }
}


U_CAPI UPlugData * U_EXPORT2
uplug_nextPlug(UPlugData *prior) {
    if(prior==NULL) {
        return pluginList;
    } else {
        UPlugData *nextPlug = &prior[1];
        UPlugData *pastPlug = &pluginList[pluginCount];
    
        if(nextPlug>=pastPlug) {
            return NULL;
        } else {
            return nextPlug;
        }
    }
}

#pragma mark plug load and unload


static void uplug_unloadPlug(UPlugData *plug, UErrorCode *status) {
    UPlugTokenReturn token;
    if(plug==NULL||U_FAILURE(*status)) {
        return;
    }
    if(plug->awaitingLoad) {  /* shouldn't happen. Plugin hasn'tbeen loaded yet.*/
        *status = U_INTERNAL_PROGRAM_ERROR;
        return; 
    }
    token = (*(plug->entrypoint))(plug, UPLUG_REASON_UNLOAD, status);
    if(token!=U_PLUG_TOKEN) {
        *status = U_INTERNAL_PROGRAM_ERROR;
    }
}

static void uplug_loadPlug(UPlugData *plug, UErrorCode *status) {
    UPlugTokenReturn token;
    if(plug==NULL||U_FAILURE(*status)) {
        return;
    }
    if(!plug->awaitingLoad) {  /* shouldn't happen. Plugin hasn'tbeen loaded yet.*/
        *status = U_INTERNAL_PROGRAM_ERROR;
        return; 
    }
    token = (*(plug->entrypoint))(plug, UPLUG_REASON_LOAD, status);
    plug->awaitingLoad = FALSE;
    if(token!=U_PLUG_TOKEN) {
        *status = U_INTERNAL_PROGRAM_ERROR;
    }
}

static UPlugData *uplug_allocatePlug(UPlugEntrypoint *entrypoint, const char *config, void *lib, const char *symName,
                            UErrorCode *status) {
    UPlugData *data = NULL;
    if(U_FAILURE(*status)) {
        return NULL;
    }
    data = &pluginList[pluginCount++];

    if(config!=NULL) {
        strncpy(data->config, config, UPLUG_NAME_MAX);
    } else {
        data->config[0] = 0;
    }
    
    if(symName!=NULL) {
        strncpy(data->sym, symName, UPLUG_NAME_MAX);
    } else {
        data->sym[0] = 0;
    }
    
    data->entrypoint = entrypoint;
    data->lib = lib;
    data->token = U_PLUG_TOKEN;
    data->structSize = sizeof(UPlugData);
    data->name[0]=0;
    data->level = UPLUG_LEVEL_INVALID; /* initialize to null state */
    data->awaitingLoad = TRUE;
    data->dontUnload = FALSE;
    
    return data;
}

static void uplug_deallocatePlug(UPlugData *plug, UErrorCode *status) {
    UErrorCode subStatus = U_ZERO_ERROR;
    if(!plug->dontUnload) {
        uplug_closeLibrary(plug->lib, &subStatus);
    }
    plug->lib = NULL;
    if(U_SUCCESS(*status) && U_FAILURE(subStatus)) {
        *status = subStatus;
    }
    /* shift plugins up and decrement count. */
    pluginCount = uplug_removeEntryAt(pluginList, pluginCount, sizeof(plug[0]), uplug_pluginNumber(plug));
}

static void uplug_doUnloadPlug(UPlugData *plugToRemove, UErrorCode *status) {
    if(plugToRemove != NULL) {
        uplug_unloadPlug(plugToRemove, status);
        uplug_deallocatePlug(plugToRemove, status);
    }
}

U_CAPI void U_EXPORT2
uplug_removePlug(UPlugData *plug, UErrorCode *status)  {
    UPlugData *cursor = NULL;
    UPlugData *prior = NULL;
    UPlugData *plugToRemove = NULL;
    if(U_FAILURE(*status)) return;
    
    for(cursor=pluginList;cursor!=NULL;) {
        if(cursor==plug) {
            plugToRemove = plug;
            cursor=NULL;
        } else {
            cursor = uplug_nextPlug(cursor);
        }
        prior = cursor;
    }
    
    uplug_doUnloadPlug(plugToRemove, status);
}


#pragma mark High Level - Implementor's API

U_CAPI void U_EXPORT2 
uplug_setPlugNoUnload(UPlugData *data, UBool dontUnload)
{
    data->dontUnload = dontUnload;
}


U_CAPI void U_EXPORT2
uplug_setPlugLevel(UPlugData *data, UPlugLevel level) {
    data->level = level;
}


U_CAPI UPlugLevel U_EXPORT2
uplug_getPlugLevel(UPlugData *data) {
    return data->level;
}


U_CAPI void U_EXPORT2
uplug_setPlugName(UPlugData *data, const char *name) {
    strncpy(data->name, name, UPLUG_NAME_MAX);
}


U_CAPI const char * U_EXPORT2
uplug_getPlugName(UPlugData *data) {
    return data->name;
}


U_CAPI const char * U_EXPORT2
uplug_getSymbolName(UPlugData *data) {
    return data->sym;
}

U_CAPI const char * U_EXPORT2
uplug_getLibraryName(UPlugData *data, UErrorCode *status) {
    return uplug_findLibrary(data->lib, status);
}

U_CAPI void * U_EXPORT2
uplug_getLibrary(UPlugData *data) {
    return data->lib;
}

U_CAPI void * U_EXPORT2
uplug_getContext(UPlugData *data) {
    return data->context;
}


U_CAPI void U_EXPORT2
uplug_setContext(UPlugData *data, void *context) {
    data->context = context;
}

U_CAPI const char* U_EXPORT2
uplug_getConfiguration(UPlugData *data) {
    return data->config;
}


#pragma mark Plug Install

static UPlugData* uplug_installPlugFromEntrypointAndLibrary(UPlugEntrypoint *entrypoint, const char *config, void *lib, const char *sym,
                            UErrorCode *status) {
    UPlugData *plug = NULL;

    plug = uplug_allocatePlug(entrypoint, config, lib, sym, status);

    uplug_loadPlug(plug, status);
    if(U_SUCCESS(*status)) {
        return plug;
    } else {
        uplug_deallocatePlug(plug, status);
        return NULL;
    }
}

U_CAPI UPlugData* U_EXPORT2
uplug_installPlugFromEntrypoint(UPlugEntrypoint *entrypoint, const char *config, UErrorCode *status) {
    return uplug_installPlugFromEntrypointAndLibrary(entrypoint, config, NULL, NULL, status);
}


U_CAPI UPlugData* U_EXPORT2
uplug_installPlugFromLibrary(const char *libName, const char *sym, const char *config, UErrorCode *status) {
    void *lib = NULL;
    UPlugData *plug = NULL;
    void *entrypoint = NULL;
    if(U_FAILURE(*status)) { return NULL; }
    lib = uplug_openLibrary(libName, status);
    if(lib!=NULL && U_SUCCESS(*status)) {
        entrypoint = uprv_dl_sym(lib, sym, status);
        
        if(entrypoint!=NULL&&U_SUCCESS(*status)) {
            plug = uplug_installPlugFromEntrypointAndLibrary((UPlugEntrypoint*)entrypoint, config, lib, sym, status);
            if(plug!=NULL&&U_SUCCESS(*status)) {
                plug->lib = lib; /* plug takes ownership of library */
                lib = NULL; /* library is now owned by plugin. */
            }
        }
        if(lib!=NULL) { /* still need to close the lib */
            UErrorCode subStatus = U_ZERO_ERROR;
            uplug_closeLibrary(lib, &subStatus); /* don't care here */
        }
    }
    return plug;
}


#pragma mark SPI

#include <stdio.h>


static UBool U_CALLCONV uplug_cleanup(void)
{
    int32_t i;
    
    UPlugData *pluginToRemove;
    /* cleanup plugs */
    for(i=0;i<pluginCount;i++) {
        UErrorCode subStatus = U_ZERO_ERROR;
        
        pluginToRemove = &pluginList[i];

        /* unload and deallocate */
        uplug_doUnloadPlug(pluginToRemove, &subStatus);
    }
    
    /* close other held libs? */
    return TRUE;
}



U_CAPI void U_EXPORT2
uplug_init(UErrorCode *status) {
    const char *plugin_dir;
    
    plugin_dir = getenv("ICU_PLUGINS");
    
#if defined(DEFAULT_ICU_PLUGINS) 
    if(plugin_dir == NULL || !*plugin_file) {
        plugin_dir = DEFAULT_ICU_PLUGINS;
    }
#endif

    if(plugin_dir != NULL && *plugin_dir) {
        char plugin_file[2048];
        FILE *f;
        
        
        strncpy(plugin_file, plugin_dir, 2047);
        strncat(plugin_file, U_FILE_SEP_STRING,2047);
        strncat(plugin_file, "icuplugins",2047);
        strncat(plugin_file, U_ICU_VERSION_SHORT ,2047);
        strncat(plugin_file, ".txt" ,2047);
        
        fprintf(stderr, ">> pluginfile [%s]\n", plugin_file);
        
        f = fopen(plugin_file, "r");

        if(f != NULL) {
            char linebuf[1024];
            char *p, *libName=NULL, *symName=NULL, *config=NULL;
            
            
            while(fgets(linebuf,1023,f)) {
                if(!*linebuf || *linebuf=='#') {
                    continue;
                } else {
                    p = linebuf;
                    while(*p&&isspace(*p))
                        p++;
                    if(!*p || *p=='#') continue;
                    libName = p;
                    while(*p&&!isspace(*p)) {
                        p++;
                    }
                    if(!*p || *p=='#') continue; /* no tab after libname */
                    *p=0; /* end of libname */
                    p++;
                    while(*p&&isspace(*p)) {
                        p++;
                    }
                    if(!*p||*p=='#') continue; /* no symname after libname +tab */
                    symName = p;
                    while(*p&&!isspace(*p)) {
                        p++;
                    }
                    
                    if(*p) { /* has config */
                        *p=0;
                        ++p;
                        while(*p&&isspace(*p)) {
                            p++;
                        }
                        if(*p) {
                            config = p;
                        }
                    }
                    
                    /* chop whitespace at the end of the config */
                    if(config!=NULL&&*config!=0) {
                        p = config+strlen(config);
                        while(p>config&&isspace(*(--p))) {
                            *p=0;
                        }
                    }
                
                    /* OK, we're good. */
                    fprintf(stderr, "PLUGIN libName=[%s], sym=[%s], config=[%s]\n", libName, symName, config);
                    
                    { 
                        UErrorCode subStatus = U_ZERO_ERROR;
                        UPlugData *plug =  uplug_installPlugFromLibrary(libName, symName, config, &subStatus);
                        fprintf(stderr, " -> %p, %s\n", (void*)plug, u_errorName(subStatus));
                    }
                }
            }
        }
    }
    ucln_registerCleanup(UCLN_UPLUG, uplug_cleanup);
}


