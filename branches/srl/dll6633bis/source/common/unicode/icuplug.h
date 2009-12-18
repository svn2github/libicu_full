/*
******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
*  FILE NAME : icuplug.h
*
*   Date         Name        Description
*   10/29/2009   sl          New.
******************************************************************************
*/


#ifndef ICUPLUG_H
#define ICUPLUG_H

#include <unicode/utypes.h>


/* === Basic types === */

/**
 * Opaque structure passed to/from a plugin. 
 * use the APIs to access it.
 */

struct UPlugData;
typedef struct UPlugData UPlugData;

/**
 * Random Token to identify a valid ICU plugin. Plugins must return this 
 * from the entrypoint.
 */
#define U_PLUG_TOKEN 0x54762486

#define UPLUG_NAME_MAX              1024


typedef uint32_t UPlugTokenReturn;

/**
 * Reason code for the entrypoint's call
 */
typedef enum {
    UPLUG_REASON_QUERY = 0,     /**< The plugin is being queried for info. **/
    UPLUG_REASON_LOAD = 1,     /**< The plugin is being loaded. **/
    UPLUG_REASON_UNLOAD = 2,   /**< The plugin is being unloaded. **/
    UPLUG_REASON_COUNT         /**< count of known reasons **/
} UPlugReason;


/**
 * Level of plugin loading
 */
typedef enum {
    UPLUG_LEVEL_INVALID = 0,     /**< The plugin is invalid, hasn't called uplug_setLevel, or can't load. **/
    UPLUG_LEVEL_UNKNOWN = 1,     /**< The plugin is waiting to be installed. **/
    UPLUG_LEVEL_LOW     = 2,     /**< The plugin must be called before u_init completes **/
    UPLUG_LEVEL_HIGH    = 3,     /**< The plugin can run at any time. **/
    UPLUG_LEVEL_COUNT         /**< count of known reasons **/
} UPlugLevel;

U_CDECL_BEGIN


/**
 * Entrypoint for an ICU plugin.
 * @param data the UPlugData handle. 
 * @param status the plugin's extended status code.
 * @return A valid plugin must return U_PLUG_TOKEN
 */
typedef UPlugTokenReturn (U_EXPORT2 UPlugEntrypoint) (
                  UPlugData *data,
                  UPlugReason reason,
                  UErrorCode *status);

/* === Needed for Implementing === */

U_CAPI void U_EXPORT2 
uplug_setPlugNoUnload(UPlugData *data, UBool dontUnload);

/**
 * Set the level of this plugin.
 * @param data plugin data handle
 * @param level the level of this plugin
 */
U_CAPI void U_EXPORT2
uplug_setPlugLevel(UPlugData *data, UPlugLevel level);

/**
 * Get the level of this plugin.
 * @param data plugin data handle
 * @return the level of this plugin
 */
U_CAPI UPlugLevel U_EXPORT2
uplug_getPlugLevel(UPlugData *data);

/**
 * Set the user-readable name of this plugin.
 * @param data plugin data handle
 * @param name the name of this plugin. The first UPLUG_NAME_MAX characters willi be copied into a new buffer.
 */
U_CAPI void U_EXPORT2
uplug_setPlugName(UPlugData *data, const char *name);

/**
 * Get the name of this plugin.
 * @param data plugin data handle
 * @return the name of this plugin
 */
U_CAPI const char * U_EXPORT2
uplug_getPlugName(UPlugData *data);

/**
 * Return the symbol name for this plugin, if known.
 * @param data plugin data handle
 * @return the symbol name, or NULL
 */
U_CAPI const char * U_EXPORT2
uplug_getSymbolName(UPlugData *data);

/**
 * Return the library name for this plugin, if known.
 * @param data plugin data handle
 * @return the library name, or NULL
 */
U_CAPI const char * U_EXPORT2
uplug_getLibraryName(UPlugData *data, UErrorCode *status);

/**
 * Return the library used for this plugin, if known.
 * Plugins could use this to load data out of their 
 * @param data plugin data handle
 * @return the library, or NULL
 */
U_CAPI void * U_EXPORT2
uplug_getLibrary(UPlugData *data);

/**
 * Return the plugin-specific context data.
 * @param data plugin data handle
 * @return the context, or NULL if not set
 */
U_CAPI void * U_EXPORT2
uplug_getContext(UPlugData *data);

/**
 * Set the plugin-specific context data.
 * @param data plugin data handle
 * @param context new context to set
 */
U_CAPI void U_EXPORT2
uplug_setContext(UPlugData *data, void *context);


/**
 * Get the configuration string, if available.
 * The string is in the platform default codepage.
 * @param data plugin data handle
 * @return configuration string, or else null.
 */
U_CAPI const char * U_EXPORT2
uplug_getConfiguration(UPlugData *data);

/* === Iteration and manipulation === */

/**
 * Return all currently installed plugins, from newest to oldest
 * Usage Example:
 * \code
 *    UPlugData *plug = NULL;
 *    while(plug=uplug_nextPlug(plug)) {
 *        ... do something with 'plug' ...
 *    }
 * \endcode
 * Not thread safe- do not call while plugs are added or removed.
 * @param prior pass in 'NULL' to get the first (most recent) plug, 
 *  otherwise pass the value returned on a prior call to uplug_nextPlug
 * @param return the next oldest plugin, or NULL if no more.
 */
U_CAPI UPlugData* U_EXPORT2
uplug_nextPlug(UPlugData *prior);

/**
 * Inject a plugin as if it were loaded from a library.
 * This is useful for testing plugins. 
 * Note that it will have a 'NULL' library pointer associated
 * with it, and therefore no llibrary will be closed at cleanup time.
 * @param entrypoint entrypoint to install
 * @param config user specified configuration string, if available, or NULL.
 * @param status error result
 * @return the new UPlugData associated with this plugin, or NULL if error.
 */
U_CAPI UPlugData* U_EXPORT2
uplug_installPlugFromEntrypoint(UPlugEntrypoint *entrypoint, const char *config, UErrorCode *status);


/**
 * Inject a plugin from a library, as if the information came from a config file
 * @param libName DLL name to load
 * @param sym symbol of plugin (UPlugEntrypoint function)
 * @param config configuration string, or NULL
 * @param status error result
 * @return the new UPlugData associated with this plugin, or NULL if error.
 */
U_CAPI UPlugData* U_EXPORT2
uplug_installPlugFromLibrary(const char *libName, const char *sym, const char *config, UErrorCode *status);

/**
 * Remove a plugin. 
 * Will request the plugin to be unloaded, and close the library if needed
 * @param data plugin handle to close
 * @param status error result
 */
U_CAPI void U_EXPORT2
uplug_removePlug(UPlugData *plug, UErrorCode *status);

/* === SPI === */

/**
 * Initialize the plugins 
 * @param status error result
 * @internal - Internal use only.
 */
U_CAPI void U_EXPORT2
uplug_init(UErrorCode *status);


U_CDECL_END

#endif
