/*
*******************************************************************************
*
*   Copyright (C) 1999-2000, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  toolutil.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999nov19
*   created by: Markus W. Scherer
*
*   This file contains utility functions for ICU tools like genccode.
*/

#ifdef WIN32
#   define VC_EXTRALEAN
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif
#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "cmemory.h"
#include "cstring.h"
#include "toolutil.h"

U_CFUNC const char *
getLongPathname(const char *pathname) {
#ifdef WIN32
    /* anticipate problems with "short" pathnames */
    static WIN32_FIND_DATA info;
    HANDLE file=FindFirstFile(pathname, &info);
    if(file!=INVALID_HANDLE_VALUE) {
        if(info.cAlternateFileName[0]!=0) {
            /* this file has a short name, get and use the long one */
            const char *basename=findBasename(pathname);
            if(basename!=pathname) {
                /* prepend the long filename with the original path */
                uprv_memmove(info.cFileName+(basename-pathname), info.cFileName, uprv_strlen(info.cFileName)+1);
                uprv_memcpy(info.cFileName, pathname, basename-pathname);
            }
            pathname=info.cFileName;
        }
        FindClose(file);
    }
#endif
    return pathname;
}

U_CFUNC const char *
findBasename(const char *filename) {
    const char *basename=uprv_strrchr(filename, U_FILE_SEP_CHAR);
    if(basename!=NULL) {
        return basename+1;
    } else {
        return filename;
    }
}
