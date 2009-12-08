/*
******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
*  FILE NAME : icuplugimp.h
*
*   Date         Name        Description
*   10/29/2009   sl          New.
******************************************************************************
*/


#ifndef ICUPLUGIMP_H
#define ICUPLUGIMP_H

#include <unicode/icuplug.h>

struct UPlugData {
    UPlugEntrypoint  *entrypoint; /**< plugin entrypoint */
    uint32_t structSize;    /**< initialized to the size of this structure */
    uint32_t token;         /**< must be U_PLUG_TOKEN */
    void *lib;              /**< plugin library, or NULL */
    char sym[UPLUG_NAME_MAX];        /**< plugin symbol, or NULL */
    char config[UPLUG_NAME_MAX];     /**< configuration data */
    void *context;          /**< user context data */
    char name[UPLUG_NAME_MAX];   /**< name of plugin */
    UPlugLevel  level; /**< level of plugin */
    UBool   awaitingLoad; /**< TRUE if the plugin is awaiting a load call */
    UBool   dontUnload; /**< TRUE if plugin must stay resident (leak plugin and lib) */
};


/**
 * Open a library, adding a reference count if needed.
 * @param libName library name to load
 * @param status error code
 * @return the library pointer, or NULL
 */
U_INTERNAL void * U_EXPORT2
uplug_openLibrary(const char *libName, UErrorCode *status);

/**
 * Close a library, if its reference count is 0
 * @param lib the library to close
 * @param status error code
 */
U_INTERNAL void U_EXPORT2
uplug_closeLibrary(void *lib, UErrorCode *status);

/**
 * Get a library's name, or NULL if not found.
 * @param lib the library's name
 * @param status error code
 * @return the library name, or NULL if not found.
 */
U_INTERNAL  char * U_EXPORT2
uplug_findLibrary(void *lib, UErrorCode *status);

#endif