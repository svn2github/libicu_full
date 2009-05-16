/*
*******************************************************************************
*
*   Copyright (C) 2000-2008, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File reslist.c
*
* Modification History:
*
*   Date        Name        Description
*   02/21/00    weiv        Creation.
*******************************************************************************
*/

#include <assert.h>
#include <stdio.h>
#include "reslist.h"
#include "unewdata.h"
#include "unicode/ures.h"
#include "unicode/putil.h"
#include "errmsg.h"

#include "uarrsort.h"

#define BIN_ALIGNMENT 16

static UBool gIncludeCopyright = FALSE;

#define PACK_KEYS 0

enum {
    URES_STRING_MINI_ASCII=10,  /* 0xA for ASCII */
    URES_STRING_MINI_WINDOW0=11,/* 0xB for Window */
    URES_STRING_COMPACT=12      /* 0xC for Compact */
};

/*
 * res_none() returns the address of kNoResource,
 * for use in non-error cases when no resource is to be added to the bundle.
 * (NULL is used in error cases.)
 */
static const struct SResource kNoResource = { URES_NONE };

/*
 * Used for fNext in strings that are created as new infixes,
 * so that they can be identified during bundle_close() and released.
 */
static const struct SResource kOutsideBundleTree = { URES_NONE };

static const UDataInfo dataInfo= {
    sizeof(UDataInfo),
    0,

    U_IS_BIG_ENDIAN,
    U_CHARSET_FAMILY,
    sizeof(UChar),
    0,

    {0x52, 0x65, 0x73, 0x42},     /* dataFormat="ResB" */
    {1, 2, 0, 0},                 /* formatVersion */
    {1, 4, 0, 0}                  /* dataVersion take a look at version inside parsed resb*/
};

static uint8_t calcPadding(uint32_t size) {
    /* returns space we need to pad */
    return (uint8_t) ((size % sizeof(uint32_t)) ? (sizeof(uint32_t) - (size % sizeof(uint32_t))) : 0);

}

void setIncludeCopyright(UBool val){
    gIncludeCopyright=val;
}

UBool getIncludeCopyright(void){
    return gIncludeCopyright;
}

static void
bundle_compactStrings(struct SRBRoot *bundle, UErrorCode *status);

static void
writeStringBytes(struct SRBRoot *bundle,
                 const uint8_t *bytes, int32_t length,
                 UErrorCode *status) {
    if (U_FAILURE(*status) || length <= 0) {
        return;
    }
    if ((bundle->fStringBytesLength + length) > bundle->fStringBytesCapacity) {
        uint8_t *stringBytes;
        int32_t capacity = 2 * bundle->fStringBytesCapacity + length + 1024;
        capacity &= ~3;  /* ensures padding fits if fStringBytesLength needs it */
        stringBytes = (uint8_t *)uprv_malloc(capacity);
        if (stringBytes == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        uprv_memcpy(stringBytes, bundle->fStringBytes, bundle->fStringBytesLength);
        uprv_free(bundle->fStringBytes);
        bundle->fStringBytes = stringBytes;
        bundle->fStringBytesCapacity = capacity;
    }
    uprv_memcpy(bundle->fStringBytes + bundle->fStringBytesLength, bytes, length);
    bundle->fStringBytesLength += length;
}

static void printCounters(const struct SRBRoot *bundle, const char *bundleName, uint32_t bundleSize) {
    /* file,size,#strings,str.size,save,#mini,mini.save,incompressible,comp.pad,keys.save,dedup.save,key.size */
    printf("potential: %s,%lu,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",
           bundleName, (long)bundleSize,
           (long)bundle->fStringsCount, (long)bundle->fStringsSize,
           (long)(bundle->fStringsSize - bundle->fPotentialSize),
           (long)bundle->fMiniCount, (long)bundle->fMiniReduction,
           (long)bundle->fIncompressibleStrings,
           (long)bundle->fCompressedPadding,
           (long)bundle->fKeysReduction,
           (long)bundle->fStringDedupReduction,
           (long)(bundle->fKeyPoint - URES_STRINGS_BOTTOM));
}

/* not Han, not Hangul, not an unpaired surrogate? */
static UBool isCompressible(UChar32 c) {
    return
        (c < LAST_WINDOW_LIMIT) &&
        !(  (0x3400 <= c && c <= 0x9fff) ||
            (0xac00 <= c && c <= 0xdfff) ||
            (0x20000 <= c && c <= 0x3ffff));
}

/* 2-byte-encodable in CJK mode */
static UBool is2ByteCJK(UChar32 c) {
    return (0x3000 <= c && c <= 0x4aff) || (0x4e00 <= c && c <= 0x9fff) || (0xac00 <= c && c <= 0xd7ff);
}

static UBool isDirectASCII(UChar32 c) {
    /* ASCII character with direct single-byte encoding */
    if (c <= 0x7e) {
        if (c >= 0x20 || (c == 9 || c == 0xa || c == 0xd)) {
            return TRUE;
        }
    }
    return FALSE;
}

/* minimum possible bytes for the string's characters with a simple, compact encoding */
static int32_t
minBytes(const UChar *s, int32_t length) {
    int32_t i, numBytes;
    UChar32 c;

    numBytes = 0;
    for (i = 0; i < length;) {
        U16_NEXT(s, i, length, c);
        /* don't count ASCII in its window */
        if (isCompressible(c)) {
            ++numBytes;
        } else if (is2ByteCJK(c)) {
            numBytes += 2;
        } else {
            numBytes += 3;
        }
    }
    return numBytes;
}

/* same as minBytes() > minNumBytes but faster */
static UBool
minBytesAreGE(const UChar *s, int32_t length, int32_t minNumBytes) {
    int32_t i, numBytes;
    UChar32 c;

    if (minNumBytes <= 0) {
        return TRUE;
    }
    numBytes = 0;
    for (i = 0; i < length;) {
        U16_NEXT(s, i, length, c);
        /* don't count ASCII in its window */
        if (isCompressible(c)) {
            ++numBytes;
        } else if (is2ByteCJK(c)) {
            numBytes += 2;
        } else {
            numBytes += 3;
        }
        if (numBytes >= minNumBytes) {
            return TRUE;
        }
    }
    return FALSE;
}

static void
addToStats(struct SRBRoot *bundle, const UChar *s, int32_t length) {
    int32_t i, window;
    UChar32 c;

    for (i = 0; i < length;) {
        U16_NEXT(s, i, length, c);
        /* don't count ASCII in its window */
        if (isCompressible(c) && !isDirectASCII(c)) {
            window = c >> 5;
            ++bundle->fWindowCounts[window];
        }
    }
}

static int32_t
findTopWindow(int32_t *windowCounts, int32_t *topCount) {
    int32_t i, topIndex, topWindowCount;
    topIndex = -1;
    topWindowCount = 0;
    for (i = 0; i <= ((LAST_WINDOW_LIMIT - 0x80) >> 5); ++i) {
        if (windowCounts[i] != 0) {
            /* 4 pieces of 0x20 characters for a 0x80-wide window */
            int32_t count;
            /* adjust the window start to prefer multiples of 0x80 */
            while ((i & 3) != 0 && windowCounts[i - 1] == 0 && windowCounts[i + 3] == 0) {
                --i;
            }
            count = windowCounts[i] + windowCounts[i + 1] +
                    windowCounts[i + 2] + windowCounts[i + 3];
            if (count > topWindowCount) {
                topIndex = i;
                topWindowCount = count;
            }
            /* un-adjust the loop counter again */
            while (windowCounts[i] == 0) {
                ++i;
            }
        }
    }
    if (topIndex >= 0) {
        *topCount = topWindowCount;  /* TODO: remove topCount arg after debugging */
        /* delete the counts for this window for looking for the next one */
        windowCounts[topIndex] = 0;
        windowCounts[topIndex + 1] = 0;
        windowCounts[topIndex + 2] = 0;
        windowCounts[topIndex + 3] = 0;
        return topIndex << 5;
    } else {
        return -1;
    }
}

/* make sure that a top window is not found straddling this boundary */
static void
noWindowAcross(int32_t *windowCounts, int32_t boundary) {
    boundary = (boundary - 0x80) >> 5;
    windowCounts[boundary] += windowCounts[boundary + 1] +
                              windowCounts[boundary + 2] +
                              windowCounts[boundary + 3];
    windowCounts[boundary + 1] = 0;
    windowCounts[boundary + 2] = 0;
    windowCounts[boundary + 3] = 0;
}

static void
findTopWindows(struct SRBRoot *bundle) {
    int32_t topWindowCounts[3] = { 0, 0, 0 };
    int32_t *windowCounts = bundle->fWindowCounts;
    int32_t *windows = bundle->fWindows;
    int32_t window;
    int32_t i;

    /* make sure that a top window is not found straddling UTF-8/16 boundaries */
    noWindowAcross(windowCounts, 0x80);  /* UTF-8 1/2-byte boundary */
    noWindowAcross(windowCounts, 0x800);  /* UTF-8 2/3-byte boundary */
    noWindowAcross(windowCounts, 0xd800);  /* UTF-16 start surrogates */
    noWindowAcross(windowCounts, 0xdc00);  /* UTF-16 lead/trail surrogates boundary*/
    noWindowAcross(windowCounts, 0xe000);  /* UTF-16 end surrogates */
    /* UTF-8 3-byte lead byte boundaries */
    for (window = 0x1000; window < 0x10000 && window < LAST_WINDOW_LIMIT; window += 0x1000) {
        noWindowAcross(windowCounts, window);
    }
    /* end of BMP, and supplementary character UTF-16 trail surrogate boundaries */
    for (window = 0x10000; window < LAST_WINDOW_LIMIT; window += 0x400) {
        noWindowAcross(windowCounts, window);
    }
    /*
     * TODO: Build back to 2 top windows.
     * Change fWindows length, findTopWindows() loop limit,
     * string_preWrite() CJK condition, reduce single-byte infix lead2 from 3 to 2.
     * Change printing of top windows.
     */
    windows[0] = windows[1] = windows[2] = 0x1fffff;
    for (i = 0; i < 3; ++i) {
        window = findTopWindow(windowCounts, topWindowCounts + i);
        if (window >= 0) {
            windows[i] = window;
        } else {
            break;
        }
    }
    printf("top windows: %04lx,%ld,%04lx,%ld,%04lx,%ld\n",
           windows[0], topWindowCounts[0],
           windows[1], topWindowCounts[1],
           windows[2], topWindowCounts[2]);
}

static int32_t
getWindow(const int32_t *windows, UChar32 c, int32_t *windowIndex) {
    int32_t i;
    for (i = 0; i <= 2; ++i) {
        int32_t w = windows[i];
        if (w <= c && c <= (w + 0x7f)) {
            *windowIndex = i;
            return w;
        }
    }
    return -1;
}

static uint32_t
encodeMiniString(const struct SRBRoot *bundle, const UChar *s, int32_t length) {
    UChar32 chars[4] = { 0 };  /* TODO: keep initializer only for debug output */
    int32_t i, count;
    UChar32 c;
    uint32_t result;

    if (length > 8) {
        return 0xffffffff;  /* too long */
    }
    if (length == 0) {
        return (uint32_t)URES_STRING_MINI_ASCII << 28;  /* bits 27..0 = 0 for empty string */
    }
    /* try to encode 1 character */
    if (length <= 2) {
        i = 0;
        U16_NEXT(s, i, length, c);
        if (i == length) {
            /* bits 27..21 = 1 for 1 character */
            result = ((uint32_t)URES_STRING_MINI_ASCII << 28) | ((uint32_t)1 << 21) | (uint32_t)c;
            /* TODO: remove printf("mini: single %08lx\n", (long)result); */
            return result;
        }
    }
    /* try to encode 1..4 ASCII characters */
    if (length <= 4) {
        result = 0;
        for (i = 0;; ++i) {
            if (i < length) {
                if (!isDirectASCII(c = s[i])) {
                    break;  /* does not fit */
                }
            } else {
                c = 0;
            }
            result = (result << 7) | (uint32_t)c;
            if (i == 3) {
                /* bits 27..21 = 9/A/D/20..7E for 1..4 ASCII chars */
                return ((uint32_t)URES_STRING_MINI_ASCII << 28) | result;
            }
        }
    }
    /* read code points */
    for (count = 0, i = 0; i < length;) {
        if (count == 4) {
            return 0xffffffff;  /* more than 4 characters, does not fit */
        }
        U16_NEXT(s, i, length, c);
        chars[count++] = c;
    }
    /* try to encode 1..4 window 0 characters in a separate resource type */
    if (count <= 4) {
        int32_t window0 = bundle->fWindows[0];
        /*
         * The last 7-bit unit must not be 0 -- the last character must not be
         * the window start -- because trailing 0 units indicate the string end.
         */
        if (chars[count - 1] != window0) {
            result = 0;
            for (i = 0;; ++i) {
                if (i < count) {
                    c = chars[i] - window0;
                    if (!(0 <= c && c <= 0x7f)) {
                        break;  /* does not fit */
                    }
                } else {
                    c = 0;
                }
                result = (result << 7) | (uint32_t)c;
                if (i == 3) {
                    /* TODO: remove printf("mini: wwww[%d]w0 %08lx <%04lx %04lx %04lx %04lx>\n",
                           (int)count, (long)result,
                           (long)chars[0], (long)chars[1], (long)chars[2], (long)chars[3]); */
                    return ((uint32_t)URES_STRING_MINI_WINDOW0 << 28) | result;
                }
            }
        }
    }
    return 0xffffffff;  /* does not fit */
}

static int32_t
encodeInfixReference(uint32_t resWord, uint8_t lead2, uint8_t *dest) {
    UResType type = (UResType)(resWord >> 28);
    uint8_t typeBit;
#if 1
    /*
     * TODO: For now assume that the infix is always in URES_STRING_COMPACT form.
     * Need to decide if this is the only allowed form, in which case we won't
     * need the typeBit, or if we want to allow another encoding form,
     * such as zip or similar. See string_write() for more comments.
     */
    assert(type == URES_STRING_COMPACT);
    typeBit = 0;
#else
    if (type == URES_STRING) {
        typeBit = 0;
    } else if (type == URES_STRING_COMPACT) {
        typeBit = 0x80;
    } else {
        return -1;  /* internal program error */
    }
#endif
    resWord &= 0xfffffff;
    if (resWord <= 0x7fff) {
        dest[0] = lead2;
        dest[1] = (uint8_t)(typeBit | (resWord >> 8));
        dest[2] = (uint8_t)resWord;
        return 3;
    } else if (resWord <= 0x7fffff) {
        dest[0] = lead2 + 1;
        dest[1] = (uint8_t)(typeBit | (resWord >> 16));
        dest[2] = (uint8_t)(resWord >> 8);
        dest[3] = (uint8_t)resWord;
        return 4;
    } else {
        dest[0] = lead2 + 2;
        dest[1] = (uint8_t)(typeBit | (resWord >> 24));
        dest[2] = (uint8_t)(resWord >> 16);
        dest[3] = (uint8_t)(resWord >> 8);
        dest[4] = (uint8_t)resWord;
        return 5;
    }
}

static UChar32
nextChar(const UChar *s, int32_t i, int32_t length) {
    if (i < length) {
        UChar32 c;
        U16_NEXT(s, i, length, c);
        return c;
    } else {
        return U_SENTINEL;
    }
}

static int32_t
encodeSingleByteMode(struct SRBRoot *bundle, struct SResource *res,
                     int32_t window, const UChar *s, int32_t length,
                     UBool writeToStringBytes, UErrorCode *status) {
    uint8_t buffer[200];
    int32_t pos = 0;
    const int32_t *windows = bundle->fWindows;
    int32_t start = 0, segment = 0, numBytes = 0;
    if (U_FAILURE(*status)) {
        return 0;
    }
    for (;;) {
        struct SResource *infix = NULL;
        int32_t limit = length;
        if (segment < MAX_INFIXES && (infix = res->u.fString.fInfixes[segment]) != NULL) {
            limit = res->u.fString.fInfixIndexes[segment];
        }
        while (start < limit) {
            int32_t w, windowIndex;
            UChar32 c;
            U16_NEXT(s, start, limit, c);
            if (isDirectASCII(c)) {
                /* 9/A/D/20..7E */
                buffer[pos++] = (uint8_t)c;
            } else if (window <= c && c <= (window + 0x7f)) {
                /* 80..FF */
                buffer[pos++] = (uint8_t)(c - window + 0x80);
            } else if ((w = getWindow(windows, c, &windowIndex)) >= 0) {
                /* windowIndex+xx */
                UChar32 next;
                buffer[pos++] = (uint8_t)windowIndex;
                next = nextChar(s, start, limit);  /* one-character lookahead */
                if (w <= next && next <= (w + 0x7f)) {
                    /* switch to this window */
                    window = w;
                    buffer[pos++] = (uint8_t)(c - w + 0x80);
                } else {
                    /* single-quote from this window */
                    buffer[pos++] = (uint8_t)(c - w);
                }
            } else {
                /* 0F..1F+xx+yy */
                buffer[pos++] = (uint8_t)(0xf + (c >> 16));
                buffer[pos++] = (uint8_t)(c >> 8);
                buffer[pos++] = (uint8_t)c;
            }
            /* 8 = 5 + 3 = max infix bytes + max regular-character bytes */
            if (pos > (sizeof(buffer) - 8)) {
                numBytes += pos;
                if (writeToStringBytes) {
                    writeStringBytes(bundle, buffer, pos, status);
                    if (U_FAILURE(*status)) {
                        return 0;
                    }
                }
                pos = 0;
            }
        }
        if (infix != NULL) {
            /* replace the infix with a reference to the already-written string */
            assert(infix->u.fString.fIsInfix);
            assert(infix->u.fString.fRes != 0xffffffff);
            pos += encodeInfixReference(infix->u.fString.fRes, 3, buffer + pos);
        } else {
            break;
        }
        start = limit + infix->u.fString.fLength;
        ++segment;
    }
    if (pos > 0) {
        numBytes += pos;
        if (writeToStringBytes) {
            writeStringBytes(bundle, buffer, pos, status);
        }
    }
    return numBytes;
}

static int32_t
encodeCJK(struct SRBRoot *bundle, struct SResource *res,
          const UChar *s, int32_t length,
          UBool writeToStringBytes, UErrorCode *status) {
    uint8_t buffer[200];
    int32_t pos = 0;
    int32_t start = 0, segment = 0, numBytes = 0;
    if (U_FAILURE(*status)) {
        return 0;
    }
    for (;;) {
        struct SResource *infix = NULL;
        int32_t limit = length;
        if (segment < MAX_INFIXES && (infix = res->u.fString.fInfixes[segment]) != NULL) {
            limit = res->u.fString.fInfixIndexes[segment];
        }
        while (start < limit) {
            UChar32 c;
            U16_NEXT(s, start, limit, c);
            if (is2ByteCJK(c)) {
                buffer[pos++] = (uint8_t)(c >> 8);
                buffer[pos++] = (uint8_t)c;
            } else if ((0x20 <= c && c <= 0x7e) || c == 9 || c == 0xa) {
                if (c >= 0x20) {
                    if (c <= 0x4f) {
                        c -= 0x20;  /* 20..4F -> 00..2F */
                    } else if (c <= 0x5b) {
                        c += 0xa0 - 0x50;  /* 50..5B -> A0..AB */
                    } else {
                        c += 0xd8 - 0x5c;  /* 5C..7E -> D8..FA */
                    }
                } else if (c == 9) {
                    c = 0xfb;
                } else /* c == 0xa LF */ {
                    c = 0xfc;
                }
                buffer[pos++] = (uint8_t)c;
            } else if (c <= 0xffff) {
                buffer[pos++] = (uint8_t)0xfd;
                buffer[pos++] = (uint8_t)(c >> 8);
                buffer[pos++] = (uint8_t)c;
            } else if (0x20000 <= c && c <= 0x2ffff) {
                buffer[pos++] = (uint8_t)0xfe;
                buffer[pos++] = (uint8_t)(c >> 8);
                buffer[pos++] = (uint8_t)c;
            } else {
                buffer[pos++] = (uint8_t)0xff;
                buffer[pos++] = (uint8_t)(c >> 16);
                buffer[pos++] = (uint8_t)(c >> 8);
                buffer[pos++] = (uint8_t)c;
            }
            /* 9 = 5 + 4 = max infix bytes + max regular-character bytes */
            if (pos > (sizeof(buffer) - 9)) {
                numBytes += pos;
                if (writeToStringBytes) {
                    writeStringBytes(bundle, buffer, pos, status);
                    if (U_FAILURE(*status)) {
                        return 0;
                    }
                }
                pos = 0;
            }
        }
        if (infix != NULL) {
            /* replace the infix with a reference to the already-written string */
            assert(infix->u.fString.fIsInfix);
            assert(infix->u.fString.fRes != 0xffffffff);
            pos += encodeInfixReference(infix->u.fString.fRes, 0x4b, buffer + pos);
        } else {
            break;
        }
        start = limit + infix->u.fString.fLength;
        ++segment;
    }
    if (pos > 0) {
        numBytes += pos;
        if (writeToStringBytes) {
            writeStringBytes(bundle, buffer, pos, status);
        }
    }
    return numBytes;
}

/*
 * Encode a length value in a variable number of bytes.
 * The .res format limits the bundle size to 1<<30 bytes,
 * which becomes the upper bound for a string length using 1 byte per UChar
 * in a simple compression scheme.
 * However, with infix expansion, the string could theoretically be longer.
 * Encode 31 bits for the full range of non-negative int32_t values.
 * Favor small numbers because most strings are short.
 */
static int32_t
encodeLength(int32_t length, uint8_t bytes[5]) {
    --length;  /* empty strings use the mini format */
    if (length <= 0x37) {
        if (bytes != NULL) {
            bytes[0] = (uint8_t)length;
        }
        return 1;
    } else if (length <= 0x3ff) {
        if (bytes != NULL) {
            bytes[0] = (uint8_t)(0x38 | (length >> 7));
            bytes[1] = (uint8_t)(length & 0x7f);
        }
        return 2;
    } else if (length <= 0x1ffff) {
        if (bytes != NULL) {
            bytes[0] = (uint8_t)(0x38 | (length >> 14));
            bytes[1] = (uint8_t)((length >> 7) | 0x80);
            bytes[2] = (uint8_t)(length & 0x7f);
        }
        return 3;
    } else if (length <= 0xffffff) {
        if (bytes != NULL) {
            bytes[0] = (uint8_t)(0x38 | (length >> 21));
            bytes[1] = (uint8_t)((length >> 14) | 0x80);
            bytes[2] = (uint8_t)((length >> 7) | 0x80);
            bytes[3] = (uint8_t)(length & 0x7f);
        }
        return 4;
    } else {
        if (bytes != NULL) {
            bytes[0] = (uint8_t)(0x38 | (length >> 28));
            bytes[1] = (uint8_t)((length >> 21) | 0x80);
            bytes[2] = (uint8_t)((length >> 14) | 0x80);
            bytes[3] = (uint8_t)((length >> 7) | 0x80);
            bytes[4] = (uint8_t)(length & 0x7f);
        }
        return 5;
    }
}

static int32_t
findBestWindow(struct SRBRoot *bundle, struct SResource *res,
               const UChar *s, int32_t length,
               UErrorCode *status) {
    int32_t lengthBytes, fewestBytes;
    int32_t i, bestWindow;

    if (res->u.fString.fIsInfix) {
        /*
         * Must use the compact encoding, not UTF-16, because
         * an infix must be written to the fStringBytes before
         * a referring string so that its offset and fRes are known.
         */
        fewestBytes = 0x7fffffff;
        bestWindow = 0;
    } else {
        /* Use UTF-16 if that's the shortest encoding. */
        fewestBytes = 4 + (res->u.fString.fLength + 1) * 2;
        fewestBytes += calcPadding(fewestBytes);
        bestWindow = -1;
    }

    lengthBytes = encodeLength(length, NULL);

    /*
     * Possible code optimizations:
     * - pass fewestBytes into encoders so that they can stop
     *   when they exceed it
     * - get the set of used windows from the first decoder so we can skip
     *   trying the other windows if they are unused
     * - have single-byte encoder check for is2ByteCJK() so we can skip
     *   the CJK encoder if no such characters occur
     * - have single-byte encoder detect when it uses exactly one byte
     *   per code point, and stop here in that case
     */
    for (i = 0; i <= 3; ++i) {
        int32_t numBytes;
        if (i <= 2) {
            numBytes = encodeSingleByteMode(bundle, res, bundle->fWindows[i],
                                            s, length, FALSE, status);
        } else {
            numBytes = encodeCJK(bundle, res, s, length, FALSE, status);
        }
        numBytes += lengthBytes;
        if (numBytes < fewestBytes) {
            fewestBytes = numBytes;
            bestWindow = i;
        }
    }
    return bestWindow;
}

/* Writing Functions */

/*
 * type_preWrite() functions calculate ("preflight") and return
 * the size of their data in the binary file.
 * Most type_preWrite() functions may return any size, but res_preWrite()
 * will always return a multiple of 4.
 *
 * The type_preWrite() size is the number of bytes that type_write()
 * will write. It excludes keys, compact strings (in bundle->fStringBytes)
 * and the resource item word that type_write() returns.
 *
 * string_preWrite() also writes compact-string bytes to bundle->fStringBytes.
 *
 * table_preWrite() and array_preWrite() set bundle->fSizeExceptRoot
 * to their children's total size if they pre-write the root resource,
 * then add the container's own size and return the sum.
 * table_preWrite() also determines whether to use URES_TABLE or
 * URES_TABLE32.
 */
static uint32_t
res_preWrite(struct SRBRoot *bundle, struct SResource *res,
             UErrorCode *status);

/*
 * type_write() functions write their data to mem, update the byteOffset
 * in parallel, and return the resource item word that will be stored in
 * the container holding this item.
 * (A kingdom for C++ and polymorphism...)
 */
static uint32_t
res_write(UNewDataMemory *mem, uint32_t *byteOffset,
          struct SRBRoot *bundle, struct SResource *res,
          UErrorCode *status);

static uint32_t
string_preWrite(struct SRBRoot *bundle, struct SResource *res,
                UErrorCode *status) {
    struct SResource *same;
    const UChar *s = res->u.fString.fChars;
    int32_t length = res->u.fString.fLength;
    uint32_t size;  /* TODO: remove variable after debugging */
    int32_t bestWindow;
    if (res->fNext == &kOutsideBundleTree) {
        /* TODO: remove after debugging */
        /* not reachable by enumerating the tree, won't be written as UTF-16 string */
        size = 0;
    } else {
        size = 4 + (res->u.fString.fLength + 1) * U_SIZEOF_UCHAR;
        size += calcPadding(size);  /* TODO: need padding in size only for dev stats */
    }
    if (res->u.fString.fRes != 0xffffffff || res->u.fString.fWriteUTF16) {
        /*
         * We visited this string already and either wrote its contents or
         * determined to use the standard UTF-16 form.
         * The first visitor returned the size for that.
         */
        return 0;
    } else if ((same = res->u.fString.fSame) != NULL) {
        /* This is a duplicate. */
        if (same->u.fString.fRes != 0xffffffff || same->u.fString.fWriteUTF16) {
            /* The original has been visited already. */
            res->u.fString.fRes = same->u.fString.fRes;
            return 0;
        } else {
            /*
             * Get the size of the contents of the first-parsed version of this string
             * which got inserted into a later part of the tree.
             *
             * Note: This assigns the correct size to each string resource
             * because the order in which res_preWrite() visits resources is
             * the same as in res_write(), and the UTF-16 size is counted for
             * the same resource for which it will later be written.
             *
             * The exception to the resource-visiting order is that
             * bundle_preWrite() calls string_preWrite() for infix strings.
             * This is harmless because infix strings are never written in
             * UTF-16 form.
             */
            size = string_preWrite(bundle, same, status);
            res->u.fString.fRes = same->u.fString.fRes;
            return size;
        }
    } else if (
        !res->u.fString.fIsInfix &&
        (res->u.fString.fRes = encodeMiniString(bundle, s, length)) != 0xffffffff
    ) {
        ++bundle->fMiniCount;
        bundle->fMiniReduction += size;
        /* TODO: return 0; */
    } else if ((bestWindow = findBestWindow(bundle, res, s, length, status)) >= 0) {
        /* use the compact form */
        uint8_t buffer[8];
        int32_t lengthBytes = encodeLength(length, buffer);
        buffer[0] |= (uint8_t)(bestWindow << 6);
        res->u.fString.fRes =
            ((uint32_t)URES_STRING_COMPACT << 28) |
            ((uint32_t)(bundle->fKeyPoint + bundle->fStringBytesLength));
        writeStringBytes(bundle, buffer, lengthBytes, status);
        if (bestWindow <= 2) {
            encodeSingleByteMode(bundle, res, bundle->fWindows[bestWindow],
                                 s, length, TRUE, status);
        } else {
            encodeCJK(bundle, res, s, length, TRUE, status);
        }
        /* TODO: return 0; */
    } else {
        /* use the standard UTF-16 form */
        res->u.fString.fWriteUTF16 = TRUE;
        bundle->fPotentialSize += size;
        ++bundle->fIncompressibleStrings;
        printf("incompressible: length %ld  normalSize %ld\n",
               (long)length, (long)size);
        /* TODO: return size; */
    }

    return size; /* TODO: move into the two UTF-16 branches */
}

static uint32_t
array_preWrite(struct SRBRoot *bundle, struct SResource *res,
               UErrorCode *status) {
    struct SResource *current;
    uint32_t size = 0;

    if (U_FAILURE(*status)) {
        return 0;
    }
    for (current = res->u.fArray.fFirst; current != NULL; current = current->fNext) {
        size += res_preWrite(bundle, current, status);
    }
    if (res == bundle->fRoot) {
        size += bundle->fInfixUTF16Size;
        bundle->fSizeExceptRoot = size;
    }
    return size + (1 + res->u.fArray.fCount) * 4;
}

static int32_t
mapKey(struct SRBRoot *bundle, int32_t oldpos) {
    const KeyMapEntry *map = bundle->fKeyMap;
    int32_t i, start, limit;

#if PACK_KEYS
    if (oldpos < -1) {
        return oldpos;  /* packed key */
    }
#endif

    /* do a binary search for the old, pre-bundle_compactKeys() key offset */
    start = bundle->fPoolBundleKeysCount;
    limit = start + bundle->fKeysCount;
    while (start < limit - 1) {
        i = (start + limit) / 2;
        if (oldpos < map[i].oldpos) {
            limit = i;
        } else {
            start = i;
        }
    }
    assert(oldpos == map[start].oldpos);
    return map[start].newpos;
}

static uint32_t
table_preWrite(struct SRBRoot *bundle, struct SResource *res,
               UErrorCode *status) {
    struct SResource *current;
    uint32_t size = 0;
    uint32_t maxKey = 0;

    if (U_FAILURE(*status)) {
        return 0;
    }
    for (current = res->u.fTable.fFirst; current != NULL; current = current->fNext) {
        size += res_preWrite(bundle, current, status);
        if (bundle->fKeyMap != NULL) {
            current->fKey = mapKey(bundle, current->fKey);
        }
        maxKey |= current->fKey;
    }
    if (res == bundle->fRoot) {
        size += bundle->fInfixUTF16Size;
        bundle->fSizeExceptRoot = size;
    }
    if(res->u.fTable.fCount > (uint32_t)bundle->fMaxTableLength) {
        bundle->fMaxTableLength = res->u.fTable.fCount;
    }
    if (res->u.fTable.fCount <= 0xffff &&
        (bundle->fPoolBundleKeys == NULL ?
            maxKey <= 0xffff : (maxKey & 0x7fffffff) <= 0x7fff)
    ) {
        /* TODO: it might make sense to reserve more than half the 16-bit key space for pool bundle key offsets */
        /* 16-bit count, 16-bit key offsets, 32-bit values */
        size += 2 + res->u.fTable.fCount * 6;
    } else {
        /* 32-bit count, key offsets and values */
        res->u.fTable.fIs32Bit = TRUE;
        size += 4 + res->u.fTable.fCount * 8;
    }
    return size;
}

static uint32_t
res_preWrite(struct SRBRoot *bundle, struct SResource *res,
             UErrorCode *status) {
    uint32_t size;

    if (U_FAILURE(*status) || res == NULL) {
        return 0;
    }
    switch (res->fType) {
    case URES_STRING:
        size = string_preWrite(bundle, res, status);
        break;
    case URES_ALIAS:
        size = 4 + (res->u.fString.fLength + 1) * U_SIZEOF_UCHAR;
        break;
    case URES_INT_VECTOR:
        size = (1 + res->u.fIntVector.fCount) * 4;
        break;
    case URES_BINARY:
        /* TODO: reduce overhead from 16 to 12=MAX_BIN_START_PADDING */
        size = 4 + res->u.fBinaryValue.fLength + BIN_ALIGNMENT;
        break;
    case URES_INT:
        size = 0;
        break;
    case URES_ARRAY:
        size = array_preWrite(bundle, res, status);
        break;
    case URES_TABLE:
        size = table_preWrite(bundle, res, status);
        break;
    default:
        *status = U_INTERNAL_PROGRAM_ERROR;
        size = 0;
        break;
    }
    return size + calcPadding(size);
}

static uint32_t
bundle_preWrite(struct SRBRoot *bundle, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return 0;
    }
    /* TODO: count and use number of UChars in strings, not total UTF-16 strings byte size */
    if (bundle->fStringsSize > 0) {
        struct SResource *current;
        int32_t capacity = bundle->fStringsSize + bundle->fStringsSize / 4;
        capacity &= ~3;  /* ensures padding fits if fStringBytesLength needs it */
        bundle->fStringBytes = (uint8_t *)uprv_malloc(capacity);
        if (bundle->fStringBytes == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return 0;
        }
        bundle->fStringBytesCapacity = capacity;
        /*
         * Pre-write all infix strings because
         * 1. They need to be written before the strings that will refer to them.
         * 2. Writing them early gives them small offsets which are encoded
         *    in fewer bytes.
         * Go from shortest to longest string to minimize the number of bytes
         * saved for small strings and thus maximize the effectiveness of infixes.
         */
        for (current = bundle->fShortestString->u.fString.fLonger;
             current != bundle->fLongestString;
             current = current->u.fString.fLonger
        ) {
            if (current->u.fString.fIsInfix) {
                /* TODO: remove bundle->fInfixUTF16Size when writing compact strings for real */
                bundle->fInfixUTF16Size += string_preWrite(bundle, current, status);
                if (U_FAILURE(*status)) {
                    return 0;
                }
            }
        }
    }
    return res_preWrite(bundle, bundle->fRoot, status);
}

static uint32_t string_write(UNewDataMemory *mem, uint32_t *byteOffset,
                             struct SRBRoot *bundle, struct SResource *res,
                             UErrorCode *status) {
    struct SResource *same = res->u.fString.fSame;
    int32_t length;

    if (res->u.fString.fRes <= 0x0fffffff) {
    /* TODO: if (res->u.fString.fRes != 0xffffffff) { */
        /* The contents of this string were already written. */
        return res->u.fString.fRes;
    } else if (same != NULL) {
        /* This is a duplicate. */
        if (same->u.fString.fRes <= 0x0fffffff) {
        /* TODO: if (same->u.fString.fRes != 0xffffffff) { */
            /* The original has been visited already. */
            res->u.fString.fRes = same->u.fString.fRes;
        } else {
            /*
             * Write the contents of the first-parsed version of this string
             * which got inserted into a later part of the tree.
             */
            res->u.fString.fRes = string_write(mem, byteOffset, bundle, same, status);
        }
        return res->u.fString.fRes;
    }

    /* Write the UTF-16 string. */
    res->u.fString.fRes = (res->fType << 28) | (*byteOffset >> 2);
    length = res->u.fString.fLength;
    udata_write32(mem, length);
    udata_writeUString(mem, res->u.fString.fChars, length + 1);
    *byteOffset += 4 + (length + 1) * U_SIZEOF_UCHAR;
    return res->u.fString.fRes;
}

static uint32_t alias_write(UNewDataMemory *mem, uint32_t *byteOffset,
                            struct SRBRoot *bundle, struct SResource *res,
                            UErrorCode *status) {
    uint32_t resWord = (res->fType << 28) | (*byteOffset >> 2);
    int32_t length = res->u.fString.fLength;
    udata_write32(mem, length);
    udata_writeUString(mem, res->u.fString.fChars, length + 1);
    *byteOffset += 4 + (length + 1) * U_SIZEOF_UCHAR;
    return resWord;
}

static uint32_t array_write(UNewDataMemory *mem, uint32_t *byteOffset,
                            struct SRBRoot *bundle, struct SResource *res,
                            UErrorCode *status) {
    uint32_t *resources = NULL;
    uint32_t  resWord;
    uint32_t  i;

    struct SResource *current = NULL;

    if (U_FAILURE(*status)) {
        return 0;
    }

    if (res->u.fArray.fCount > 0) {
        resources = (uint32_t *) uprv_malloc(sizeof(uint32_t) * res->u.fArray.fCount);
        if (resources == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return 0;
        }
    }

    for (i = 0, current = res->u.fArray.fFirst; current != NULL; current = current->fNext) {
        resources[i++] = res_write(mem, byteOffset, bundle, current, status);
    }
    assert(i == res->u.fArray.fCount);

    resWord = (res->fType << 28) | (*byteOffset >> 2);
    udata_write32(mem, res->u.fArray.fCount);
    udata_writeBlock(mem, resources, sizeof(uint32_t) * res->u.fArray.fCount);
    uprv_free(resources);
    *byteOffset += (1 + res->u.fArray.fCount) * 4;

    return resWord;
}

static uint32_t intvector_write(UNewDataMemory *mem, uint32_t *byteOffset,
                                struct SRBRoot *bundle, struct SResource *res,
                                UErrorCode *status) {
    uint32_t resWord = (res->fType << 28) | (*byteOffset >> 2);
    uint32_t i = 0;
    udata_write32(mem, res->u.fIntVector.fCount);
    for(i = 0; i<res->u.fIntVector.fCount; i++) {
      udata_write32(mem, res->u.fIntVector.fArray[i]);
    }
    *byteOffset += (1 + res->u.fIntVector.fCount) * 4;

    return resWord;
}

static uint32_t bin_write(UNewDataMemory *mem, uint32_t *byteOffset,
                          struct SRBRoot *bundle, struct SResource *res,
                          UErrorCode *status) {
    uint32_t pad       = 0;
    uint32_t dataStart = *byteOffset + sizeof(res->u.fBinaryValue.fLength);
    uint32_t resWord;

    if (dataStart % BIN_ALIGNMENT) {
        pad = (BIN_ALIGNMENT - dataStart % BIN_ALIGNMENT);
        udata_writePadding(mem, pad);
        *byteOffset += pad;
    }

    resWord = (res->fType << 28) | (*byteOffset >> 2);
    udata_write32(mem, res->u.fBinaryValue.fLength);
    if (res->u.fBinaryValue.fLength > 0) {
        udata_writeBlock(mem, res->u.fBinaryValue.fData, res->u.fBinaryValue.fLength);
    }
    /* TODO: why so much? because of how fSize got precalculated? if so, then preflighting might be able to fix this */
    udata_writePadding(mem, BIN_ALIGNMENT - pad);
    *byteOffset += 4 + res->u.fBinaryValue.fLength + BIN_ALIGNMENT;

    return resWord;
}

static uint32_t int_write(UNewDataMemory *mem, uint32_t *byteOffset,
                          struct SRBRoot *bundle, struct SResource *res,
                          UErrorCode *status) {
    return (res->fType << 28) | (res->u.fIntValue.fValue & 0xFFFFFFF);
}

static uint32_t table_write(UNewDataMemory *mem, uint32_t *byteOffset,
                            struct SRBRoot *bundle, struct SResource *res,
                            UErrorCode *status) {
    uint32_t *resources = NULL;
    struct SResource *current;
    uint32_t  resWord;
    uint32_t  i;

    if (U_FAILURE(*status)) {
        return 0;
    }

    if (res->u.fTable.fCount > 0) {
        resources = (uint32_t *) uprv_malloc(sizeof(uint32_t) * res->u.fTable.fCount);
        if (resources == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return 0;
        }
    }

    for (i = 0, current = res->u.fTable.fFirst; current != NULL; current = current->fNext) {
        assert(i < res->u.fTable.fCount);
        resources[i++] = res_write(mem, byteOffset, bundle, current, status);
    }
    assert(i == res->u.fTable.fCount);

    resWord = *byteOffset >> 2;
    if(!res->u.fTable.fIs32Bit) {
        resWord |= (uint32_t)URES_TABLE << 28;
        udata_write16(mem, (uint16_t)res->u.fTable.fCount);
        for (current = res->u.fTable.fFirst; current != NULL; current = current->fNext) {
            int32_t key = current->fKey;
            if (key < 0) {
                key |= 0x8000;  /* offset in the pool bundle */
            }
            udata_write16(mem, (uint16_t)key);
        }
        *byteOffset += (1 + res->u.fTable.fCount)* 2;
        if ((res->u.fTable.fCount & 1) == 0) {
            /* 16-bit count and even number of 16-bit key offsets need padding */
            udata_writePadding(mem, 2);
            *byteOffset += 2;
        }
    } else {
        resWord |= (uint32_t)URES_TABLE32 << 28;
        udata_write32(mem, res->u.fTable.fCount);
        for (current = res->u.fTable.fFirst; current != NULL; current = current->fNext) {
            udata_write32(mem, (uint32_t)current->fKey);
        }
        *byteOffset += (1 + res->u.fTable.fCount)* 4;
    }

    udata_writeBlock(mem, resources, sizeof(uint32_t) * res->u.fTable.fCount);
    uprv_free(resources);
    *byteOffset += res->u.fTable.fCount * 4;

    return resWord;
}

uint32_t res_write(UNewDataMemory *mem, uint32_t *byteOffset,
                   struct SRBRoot *bundle, struct SResource *res,
                   UErrorCode *status) {
    uint32_t resWord;
    uint8_t paddingSize;

    if (U_FAILURE(*status) || res == NULL) {
        return 0;
    }
    switch (res->fType) {
    case URES_STRING:
        bundle->fPotentialSize += 4 /* resource item */;
        resWord = string_write    (mem, byteOffset, bundle, res, status);
        break;
    case URES_ALIAS:
        resWord = alias_write     (mem, byteOffset, bundle, res, status);
        break;
    case URES_INT_VECTOR:
        resWord = intvector_write (mem, byteOffset, bundle, res, status);
        break;
    case URES_BINARY:
        resWord = bin_write       (mem, byteOffset, bundle, res, status);
        break;
    case URES_INT:
        resWord = int_write       (mem, byteOffset, bundle, res, status);
        break;
    case URES_ARRAY:
        resWord = array_write     (mem, byteOffset, bundle, res, status);
        break;
    case URES_TABLE:
        resWord = table_write     (mem, byteOffset, bundle, res, status);
        break;
    default:
        *status = U_INTERNAL_PROGRAM_ERROR;
        resWord = 0;
        break;
    }
    paddingSize = calcPadding(*byteOffset);
    if (paddingSize > 0) {
        udata_writePadding(mem, paddingSize);
        *byteOffset += paddingSize;
    }
    return resWord;
}

void bundle_write(struct SRBRoot *bundle, const char *outputDir, const char *outputPkg, char *writtenFilename, int writtenFilenameLen, UErrorCode *status) {
    UNewDataMemory *mem        = NULL;
    uint8_t         pad        = 0;
    uint32_t        root       = 0;
    uint32_t        byteOffset = 0;
    uint32_t        top, size, rootSize;
    char            dataName[1024];
    int32_t         indexes[URES_INDEX_TOP];

    bundle->fIsDoneParsing = TRUE;

    bundle_compactKeys(bundle, status);
    /*
     * Add padding bytes to fKeys so that fKeyPoint is aligned
     * for use in bundle_preWrite().
     * Safe because the capacity is a multiple of 4.
     */
    while (bundle->fKeyPoint & 3) {
        bundle->fKeys[bundle->fKeyPoint++] = 0xaa;
    }

    bundle_compactStrings(bundle, status);
    findTopWindows(bundle);
    rootSize = bundle_preWrite(bundle, status);
    /*
     * Add padding bytes to fStringBytes as well.
     * Safe because the capacity is a multiple of 4.
     */
    bundle->fPotentialSize += bundle->fStringBytesLength;
    while (bundle->fStringBytesLength & 3) {
        bundle->fStringBytes[bundle->fStringBytesLength++] = 0xaa;
        ++bundle->fCompressedPadding;
    }

    /* all keys have been mapped */
    uprv_free(bundle->fKeyMap);
    bundle->fKeyMap = NULL;

    if (U_FAILURE(*status)) {
        return;
    }

    /*
     * TODO: Work in phases.
     * - Done: Compact the keys.
     * - Done: Enumerate all string values, eliminate duplicates,
     *   preflight their lengths and adjust their fType.
     *   Rewrite SResource.fKey offsets and delete fKeyMap.
     *   (Remove the mapping code from table_write().)
     * - Done: Now (not earlier) calculate strings' and containers' fSize and fChildrenSize,
     *   and table's subtype (16/32-bit key offsets).
     * - Open the file, write the header, the keys and the compressed strings.
     * - Then write all non-container items.
     * - Last write all container items.
     *   (Revisit whether it's useful for this to be separate.)
     */

#if 0
    {
        const char *keysStart = bundle->fKeys+URES_STRINGS_BOTTOM;
        const char *keysLimit = keysStart + bundle->fKeyPoint-URES_STRINGS_BOTTOM;
        const char *keys = keysStart;
        while (keys < keysLimit) {
            char c = *keys;
            if (c != 0 && !('a' <= c && c <= 'z') && !('A' <= c && c <= 'Z') && !('0' <= c && c <= '9')) {
                printf("other-key: %02x\n", (int)c);
                if (keys == keysStart || *(keys - 1) == 0) {
                    printf("initial-key: %02x\n", (int)c);
                }
                if (keys[1] == 0) {
                    printf("final-key: %02x\n", (int)c);
                }
            }
            ++keys;
        }
    }
#endif

    if (writtenFilename && writtenFilenameLen) {
        *writtenFilename = 0;
    }

    if (writtenFilename) {
       int32_t off = 0, len = 0;
       if (outputDir) {
           len = (int32_t)uprv_strlen(outputDir);
           if (len > writtenFilenameLen) {
               len = writtenFilenameLen;
           }
           uprv_strncpy(writtenFilename, outputDir, len);
       }
       if (writtenFilenameLen -= len) {
           off += len;
           writtenFilename[off] = U_FILE_SEP_CHAR;
           if (--writtenFilenameLen) {
               ++off;
               if(outputPkg != NULL)
               {
                   uprv_strcpy(writtenFilename+off, outputPkg);
                   off += (int32_t)uprv_strlen(outputPkg);
                   writtenFilename[off] = '_';
                   ++off;
               }

               len = (int32_t)uprv_strlen(bundle->fLocale);
               if (len > writtenFilenameLen) {
                   len = writtenFilenameLen;
               }
               uprv_strncpy(writtenFilename + off, bundle->fLocale, len);
               if (writtenFilenameLen -= len) {
                   off += len;
                   len = 5;
                   if (len > writtenFilenameLen) {
                       len = writtenFilenameLen;
                   }
                   uprv_strncpy(writtenFilename +  off, ".res", len);
               }
           }
       }
    }

    if(outputPkg)
    {
        uprv_strcpy(dataName, outputPkg);
        uprv_strcat(dataName, "_");
        uprv_strcat(dataName, bundle->fLocale);
    }
    else
    {
        uprv_strcpy(dataName, bundle->fLocale);
    }

    mem = udata_create(outputDir, "res", dataName, &dataInfo, (gIncludeCopyright==TRUE)? U_COPYRIGHT_STRING:NULL, status);
    if(U_FAILURE(*status)){
        return;
    }

    /* we're gonna put the main table at the end */
    byteOffset = bundle->fKeyPoint /* TODO: + fStringBytesLength */;
    root = (bundle->fRoot->fType << 28) | ((byteOffset + bundle->fSizeExceptRoot) >> 2);

    /* write the root item */
    udata_write32(mem, root);

    /* total size including the root item */
    top = byteOffset + rootSize;

    /*
     * formatVersion 1.1 (ICU 2.8):
     * write int32_t indexes[] after root and before the strings
     * to make it easier to parse resource bundles in icuswap or from Java etc.
     */
    uprv_memset(indexes, 0, sizeof(indexes));
    indexes[URES_INDEX_LENGTH]=             URES_INDEX_TOP;
    indexes[URES_INDEX_STRINGS_TOP]=        (int32_t)(bundle->fKeyPoint>>2);
    indexes[URES_INDEX_RESOURCES_TOP]=      (int32_t)(top>>2);
    indexes[URES_INDEX_BUNDLE_TOP]=         indexes[URES_INDEX_RESOURCES_TOP];
    indexes[URES_INDEX_MAX_TABLE_LENGTH]=   bundle->fMaxTableLength;
    /* TODO: bundle->fStringBytesLength, fStringsCount, fStringUCharsCount, fWindows[] */

    /*
     * formatVersion 1.2 (ICU 3.6):
     * write indexes[URES_INDEX_ATTRIBUTES] with URES_ATT_NO_FALLBACK set or not set
     * the memset() above initialized all indexes[] to 0
     */
    if(bundle->noFallback) {
        indexes[URES_INDEX_ATTRIBUTES]=URES_ATT_NO_FALLBACK;
    }

    /* write the indexes[] */
    udata_writeBlock(mem, indexes, sizeof(indexes));

    /* write the table key strings */
    udata_writeBlock(mem, bundle->fKeys+URES_STRINGS_BOTTOM,
                          bundle->fKeyPoint-URES_STRINGS_BOTTOM);
    /* TODO: write bundle->fStringBytes[] */

    /* write all of the bundle contents: the root item and its children */
    res_write(mem, &byteOffset, bundle, bundle->fRoot, status);
    assert(byteOffset == top);

    size = udata_finish(mem, status);
    if(top != size) {
        fprintf(stderr, "genrb error: wrote %u bytes but counted %u\n",
                (int)size, (int)top);
        *status = U_INTERNAL_PROGRAM_ERROR;
    }

    printCounters(bundle, writtenFilename, size);
}

/* Opening Functions */
struct SResource* res_open(struct SRBRoot *bundle, const char *tag,
                           const struct UString* comment, UErrorCode* status){
    struct SResource *res;
    int32_t key = bundle_addtag(bundle, tag, status);
    if (U_FAILURE(*status)) {
        return NULL;
    }

    res = (struct SResource *) uprv_malloc(sizeof(struct SResource));
    if (res == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    uprv_memset(res, 0, sizeof(struct SResource));
    res->fKey = key;

    ustr_init(&res->fComment);
    if(comment != NULL){
        ustr_cpy(&res->fComment, comment, status);
        if (U_FAILURE(*status)) {
            res_close(res);
            return NULL;
        }
    }
    return res;
}

struct SResource* res_none() {
    return (struct SResource*)&kNoResource;
}

struct SResource* table_open(struct SRBRoot *bundle, const char *tag, const struct UString* comment, UErrorCode *status) {
    struct SResource *res = res_open(bundle, tag, comment, status);
    if (U_FAILURE(*status)) {
        return NULL;
    }
    res->fType = URES_TABLE;
    res->u.fTable.fRoot = bundle;
    return res;
}

struct SResource* array_open(struct SRBRoot *bundle, const char *tag, const struct UString* comment, UErrorCode *status) {
    struct SResource *res = res_open(bundle, tag, comment, status);
    if (U_FAILURE(*status)) {
        return NULL;
    }
    res->fType = URES_ARRAY;
    return res;
}

static int32_t U_CALLCONV
string_hash(const UHashTok key) {
    const struct SResource *res = (struct SResource *)key.pointer;
    return uhash_hashUCharsN(res->u.fString.fChars, res->u.fString.fLength);
}

static UBool U_CALLCONV
string_comp(const UHashTok key1, const UHashTok key2) {
    const struct SResource *res1 = (struct SResource *)key1.pointer;
    const struct SResource *res2 = (struct SResource *)key2.pointer;
    return 0 == u_strCompare(res1->u.fString.fChars, res1->u.fString.fLength,
                             res2->u.fString.fChars, res2->u.fString.fLength,
                             FALSE);
}

enum {
    /* Pessimistically, it takes 4 encoding bytes to refer to an infix. Worst case is 5. */
    PESSIMISTIC_ENCODED_INFIX_POINTER_LENGTH = 4,
    /* An infix should save space overall. */
    MIN_INFIX_LENGTH = PESSIMISTIC_ENCODED_INFIX_POINTER_LENGTH + 1,
    /*
     * Minimum bytes length for a common infix between two longer strings
     * for it to be worth creating a new string.
     *
     * minBytesForInfix = Optimistic number of bytes
     * just for encoding the common-infix characters without other overhead.
     * Savings: Twice the difference to the pessimistic... length, one per original string.
     * Cost: The minimum bytes plus the length byte.
     *
     * int32_t minBytesForInfix = minBytes(infix, infixLength);
     * if (2 * (minBytesForInfix - PESSIMISTIC_ENCODED_INFIX_POINTER_LENGTH) >
     *     (1 + minBytesForInfix))  // 1 for the length byte
     *   ... worth it
     *
     * which is equivalent to
     *
     * int32_t minBytesForInfix = minBytes(infix, infixLength);
     * if (minBytesForInfix >= (2 *PESSIMISTIC_ENCODED_INFIX_POINTER_LENGTH + 2))
     *   ... worth it
     */
    MIN_COMMON_INFIX_LENGTH = 2 * PESSIMISTIC_ENCODED_INFIX_POINTER_LENGTH + 2,
    /* Limit recursion depth for decoding. */
    MAX_INFIX_DEPTH = 4
};

struct SResource *string_open(struct SRBRoot *bundle, char *tag, const UChar *value, int32_t len, const struct UString* comment, UErrorCode *status) {
    struct SResource *res = res_open(bundle, tag, comment, status);
    if (U_FAILURE(*status)) {
        return NULL;
    }
    res->fType = URES_STRING;

    if (!bundle->fIsDoneParsing) {
        /* TODO: fStringsCount is not incremented after parsing for development stats;
           for final implementation it must always be incremented so that the runtime
           code can size its hashtable accurately.
         */
        ++bundle->fStringsCount;
        bundle->fStringsSize += 4 + (len + 1) * 2 + calcPadding((len + 1) * 2);
    }

    /* check for duplicates */
    res->u.fString.fLength = len;
    res->u.fString.fChars  = (UChar *)value;
    res->u.fString.fRes = 0xffffffff;

    if (bundle->fStringSet == NULL) {
        UErrorCode localStatus = U_ZERO_ERROR;  /* if failure: just don't detect dups */
        bundle->fStringSet = uhash_open(string_hash, string_comp, string_comp, &localStatus);
    } else {
        res->u.fString.fSame = uhash_get(bundle->fStringSet, res);
    }
    if (res->u.fString.fSame == NULL) {
        struct SResource *current;

        /* this is a new string */
        res->u.fString.fChars = (UChar *) uprv_malloc(sizeof(UChar) * (len + 1));

        if (res->u.fString.fChars == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            uprv_free(res);
            return NULL;
        }

        uprv_memcpy(res->u.fString.fChars, value, sizeof(UChar) * len);
        res->u.fString.fChars[len] = 0;
        /* put it into the set for finding duplicates */
        uhash_put(bundle->fStringSet, res, res, status);

        /* put it into the list for finding infixes unless it's very short */
        if (minBytesAreGE(value, len, MIN_INFIX_LENGTH)) {
            for (current = bundle->fShortestString->u.fString.fLonger;
                 len > current->u.fString.fLength;
                 current = current->u.fString.fLonger) {}
            current->u.fString.fShorter->u.fString.fLonger = res;
            res->u.fString.fShorter = current->u.fString.fShorter;
            current->u.fString.fShorter = res;
            res->u.fString.fLonger = current;
        }

        if (!bundle->fIsDoneParsing) {
            addToStats(bundle, value, len);
        }
    } else {
        /* this is a duplicate of fSame */
        struct SResource *same = res->u.fString.fSame;
        int32_t size = 4 + (len + 1) * U_SIZEOF_UCHAR;  /* TODO: remove after debugging */
        bundle->fStringDedupReduction += size + calcPadding(size);
        res->u.fString.fChars = same->u.fString.fChars;
    }
    return res;
}

/* TODO: make alias_open and string_open use the same code */
struct SResource *alias_open(struct SRBRoot *bundle, char *tag, UChar *value, int32_t len, const struct UString* comment, UErrorCode *status) {
    struct SResource *res = res_open(bundle, tag, comment, status);
    if (U_FAILURE(*status)) {
        return NULL;
    }
    res->fType = URES_ALIAS;

    res->u.fString.fLength = len;
    res->u.fString.fChars  = (UChar *) uprv_malloc(sizeof(UChar) * (len + 1));
    if (res->u.fString.fChars == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        uprv_free(res);
        return NULL;
    }
    uprv_memcpy(res->u.fString.fChars, value, sizeof(UChar) * (len + 1));
    return res;
}


struct SResource* intvector_open(struct SRBRoot *bundle, char *tag, const struct UString* comment, UErrorCode *status) {
    struct SResource *res = res_open(bundle, tag, comment, status);
    if (U_FAILURE(*status)) {
        return NULL;
    }
    res->fType = URES_INT_VECTOR;

    res->u.fIntVector.fCount = 0;
    res->u.fIntVector.fArray = (uint32_t *) uprv_malloc(sizeof(uint32_t) * RESLIST_MAX_INT_VECTOR);
    if (res->u.fIntVector.fArray == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        uprv_free(res);
        return NULL;
    }
    return res;
}

struct SResource *int_open(struct SRBRoot *bundle, char *tag, int32_t value, const struct UString* comment, UErrorCode *status) {
    struct SResource *res = res_open(bundle, tag, comment, status);
    if (U_FAILURE(*status)) {
        return NULL;
    }
    res->fType = URES_INT;
    res->u.fIntValue.fValue = value;
    return res;
}

struct SResource *bin_open(struct SRBRoot *bundle, const char *tag, uint32_t length, uint8_t *data, const char* fileName, const struct UString* comment, UErrorCode *status) {
    struct SResource *res = res_open(bundle, tag, comment, status);
    if (U_FAILURE(*status)) {
        return NULL;
    }
    res->fType = URES_BINARY;

    res->u.fBinaryValue.fLength = length;
    res->u.fBinaryValue.fFileName = NULL;
    if(fileName!=NULL && uprv_strcmp(fileName, "") !=0){
        res->u.fBinaryValue.fFileName = (char*) uprv_malloc(sizeof(char) * (uprv_strlen(fileName)+1));
        uprv_strcpy(res->u.fBinaryValue.fFileName,fileName);
    }
    if (length > 0) {
        res->u.fBinaryValue.fData   = (uint8_t *) uprv_malloc(sizeof(uint8_t) * length);

        if (res->u.fBinaryValue.fData == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            uprv_free(res);
            return NULL;
        }

        uprv_memcpy(res->u.fBinaryValue.fData, data, length);
    }
    else {
        res->u.fBinaryValue.fData = NULL;
    }

    return res;
}

struct SRBRoot *bundle_open(const struct UString* comment, UErrorCode *status) {
    struct SRBRoot *bundle;

    if (U_FAILURE(*status)) {
        return NULL;
    }

    bundle = (struct SRBRoot *) uprv_malloc(sizeof(struct SRBRoot));
    if (bundle == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return 0;
    }
    uprv_memset(bundle, 0, sizeof(struct SRBRoot));

    bundle->fKeys = (char *) uprv_malloc(sizeof(char) * KEY_SPACE_SIZE);
    bundle->fRoot = table_open(bundle, NULL, comment, status);
    bundle->fShortestString = (struct SResource *)uprv_malloc(2 * sizeof(struct SResource));
    if (bundle->fKeys == NULL || bundle->fRoot == NULL ||
        bundle->fShortestString == NULL || U_FAILURE(*status)
    ) {
        if (U_SUCCESS(*status)) {
            *status = U_MEMORY_ALLOCATION_ERROR;
        }
        bundle_close(bundle, status);
        return NULL;
    }

    bundle->fLocale   = NULL;
    bundle->fKeysCapacity = KEY_SPACE_SIZE;
    /* formatVersion 1.1: start fKeyPoint after the root item and indexes[] */
    uprv_memset(bundle->fKeys, 0, URES_STRINGS_BOTTOM);
    bundle->fKeyPoint = URES_STRINGS_BOTTOM;

    /* doubly-linked list of unique strings with first/last sentinels */
    uprv_memset(bundle->fShortestString, 0, 2 * sizeof(struct SResource));
    bundle->fShortestString->u.fString.fLength = -1;
    bundle->fLongestString = bundle->fShortestString + 1;
    bundle->fLongestString->u.fString.fLength = 0x7fffffff;
    bundle->fShortestString->u.fString.fLonger = bundle->fLongestString;
    bundle->fLongestString->u.fString.fShorter = bundle->fShortestString;

    return bundle;
}

/* Closing Functions */
static void table_close(struct SResource *table) {
    struct SResource *current = NULL;
    struct SResource *prev    = NULL;

    current = table->u.fTable.fFirst;

    while (current != NULL) {
        prev    = current;
        current = current->fNext;

        res_close(prev);
    }

    table->u.fTable.fFirst = NULL;
}

static void array_close(struct SResource *array) {
    struct SResource *current = NULL;
    struct SResource *prev    = NULL;
    
    if(array==NULL){
        return;
    }
    current = array->u.fArray.fFirst;
    
    while (current != NULL) {
        prev    = current;
        current = current->fNext;

        res_close(prev);
    }
    array->u.fArray.fFirst = NULL;
}

static void string_close(struct SResource *string) {
    if (string->u.fString.fChars != NULL && string->u.fString.fSame == NULL) {
        uprv_free(string->u.fString.fChars);
        string->u.fString.fChars =NULL;
    }
}

static void alias_close(struct SResource *alias) {
    if (alias->u.fString.fChars != NULL) {
        uprv_free(alias->u.fString.fChars);
        alias->u.fString.fChars =NULL;
    }
}

static void intvector_close(struct SResource *intvector) {
    if (intvector->u.fIntVector.fArray != NULL) {
        uprv_free(intvector->u.fIntVector.fArray);
        intvector->u.fIntVector.fArray =NULL;
    }
}

static void int_close(struct SResource *intres) {
    /* Intentionally left blank */
}

static void bin_close(struct SResource *binres) {
    if (binres->u.fBinaryValue.fData != NULL) {
        uprv_free(binres->u.fBinaryValue.fData);
        binres->u.fBinaryValue.fData = NULL;
    }
}

void res_close(struct SResource *res) {
    if (res != NULL) {
        switch(res->fType) {
        case URES_STRING:
            string_close(res);
            break;
        case URES_ALIAS:
            alias_close(res);
            break;
        case URES_INT_VECTOR:
            intvector_close(res);
            break;
        case URES_BINARY:
            bin_close(res);
            break;
        case URES_INT:
            int_close(res);
            break;
        case URES_ARRAY:
            array_close(res);
            break;
        case URES_TABLE:
            table_close(res);
            break;
        default:
            /* Shouldn't happen */
            break;
        }

        ustr_deinit(&res->fComment);
        uprv_free(res);
    }
}

void bundle_close(struct SRBRoot *bundle, UErrorCode *status) {
    /*
     * Delete strings that were created as an optimization and are
     * not reachable by enumerating the bundle tree.
     * Do this before res_close(root) because that will delete
     * some of the items in this list.
     */
    struct SResource *current, *longer;
    for (current = bundle->fShortestString->u.fString.fLonger;
         current != bundle->fLongestString;
         current = longer
    ) {
        longer = current->u.fString.fLonger;
        if (current->fNext == &kOutsideBundleTree) {
            res_close(current);
        }
    }
    uprv_free(bundle->fShortestString);
    res_close(bundle->fRoot);
    uprv_free(bundle->fLocale);
    uprv_free(bundle->fKeys);
    uprv_free(bundle->fKeyMap);
    uhash_close(bundle->fStringSet);
    uprv_free(bundle->fStringBytes);
    uprv_free(bundle);
}

/* Adding Functions */
void table_add(struct SResource *table, struct SResource *res, int linenumber, UErrorCode *status) {
    struct SResource *current = NULL;
    struct SResource *prev    = NULL;
    struct SResTable *list;
    const char *resKeyString;
#if PACK_KEYS
    char resKeyBuffer[8];
#endif

    if (U_FAILURE(*status)) {
        return;
    }
    if (res == &kNoResource) {
        return;
    }

    /* remember this linenumber to report to the user if there is a duplicate key */
    res->line = linenumber;

    /* here we need to traverse the list */
    list = &(table->u.fTable);
    ++(list->fCount);

    /* is list still empty? */
    if (list->fFirst == NULL) {
        list->fFirst = res;
        res->fNext   = NULL;
        return;
    }

#if PACK_KEYS
    resKeyString = unpackKey(list->fRoot->fKeys, res->fKey, resKeyBuffer);
#else
    resKeyString = list->fRoot->fKeys + res->fKey;
#endif

    current = list->fFirst;

    while (current != NULL) {
#if PACK_KEYS
        char currentKeyBuffer[8];
        const char *currentKeyString = unpackKey(list->fRoot->fKeys, current->fKey, currentKeyBuffer);
#else
        const char *currentKeyString = list->fRoot->fKeys + current->fKey;
#endif
        int diff = uprv_strcmp(currentKeyString, resKeyString);
        if (diff < 0) {
            prev    = current;
            current = current->fNext;
        } else if (diff > 0) {
            /* we're either in front of list, or in middle */
            if (prev == NULL) {
                /* front of the list */
                list->fFirst = res;
            } else {
                /* middle of the list */
                prev->fNext = res;
            }

            res->fNext = current;
            return;
        } else {
            /* Key already exists! ERROR! */
            error(linenumber, "duplicate key '%s' in table, first appeared at line %d", currentKeyString, current->line);
            *status = U_UNSUPPORTED_ERROR;
            return;
        }
    }

    /* end of list */
    prev->fNext = res;
    res->fNext  = NULL;
}

void array_add(struct SResource *array, struct SResource *res, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return;
    }

    if (array->u.fArray.fFirst == NULL) {
        array->u.fArray.fFirst = res;
        array->u.fArray.fLast  = res;
    } else {
        array->u.fArray.fLast->fNext = res;
        array->u.fArray.fLast        = res;
    }

    (array->u.fArray.fCount)++;
}

void intvector_add(struct SResource *intvector, int32_t value, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return;
    }

    *(intvector->u.fIntVector.fArray + intvector->u.fIntVector.fCount) = value;
    intvector->u.fIntVector.fCount++;
}

/* Misc Functions */

void bundle_setlocale(struct SRBRoot *bundle, UChar *locale, UErrorCode *status) {

    if(U_FAILURE(*status)) {
        return;
    }

    if (bundle->fLocale!=NULL) {
        uprv_free(bundle->fLocale);
    }

    bundle->fLocale= (char*) uprv_malloc(sizeof(char) * (u_strlen(locale)+1));

    if(bundle->fLocale == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    /*u_strcpy(bundle->fLocale, locale);*/
    u_UCharsToChars(locale, bundle->fLocale, u_strlen(locale)+1);

}

#if PACK_KEYS

/* pack a-zA-Z0-9 and the two other most common key characters into 6 bits each */
static int32_t
packKeyChar(int32_t c) {
    int32_t i;
    for (i = 0; i < 64; ++i) {
        if (c == gPackedKeyCodeToChar[i]) {
            return i;
        }
    }
    return -1;  /* does not fit */
}

static int32_t
packKey(const char *key) {
    int32_t result = 0;
    int32_t i, c;
    for (i = 0;; ++i) {
        c = *key;
        if (c != 0) {
            ++key;
            c = packKeyChar(c);
            if (c < 0 || (c == 0 && (*key == 0 || i == 4))) {
                break;  /* does not fit */
            }
        }
        result = (result << 6) | c;
        if (i == 4) {  /* pack exactly five 6-bit codes */
            if (c == 0) {  /* reached the end of the string */
                return result;
            } else {  /* the string is too long */
                break;
            }
        }
    }
    return -1;  /* does not fit */
}

static const char gPackedKeyCodeToChar[64] = ":_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

const char *
unpackKey(const char *keys, int32_t key, char temp[8]) {
    if (key >= 0) {
        return keys + key;
    } else {
        char *s;
        temp[0] = gPackedKeyCodeToChar[(key >> 24) & 0x3f];
        temp[1] = gPackedKeyCodeToChar[(key >> 18) & 0x3f];
        temp[2] = gPackedKeyCodeToChar[(key >> 12) & 0x3f];
        temp[3] = gPackedKeyCodeToChar[(key >> 6) & 0x3f];
        temp[4] = gPackedKeyCodeToChar[key & 0x3f];
        /* find the end of the key and NUL-terminate it */
        s = temp + 5;
        while (*(s - 1) == ':' && temp < --s) {}
        *s = 0;
        return temp;
    }
}

#endif

static const char *
getKeyString(const struct SRBRoot *bundle, int32_t key) {
    if (key < 0) {
        return bundle->fPoolBundleKeys + (key & 0x7fffffff);
    } else {
        return bundle->fKeys + key;
    }
}

const char *
res_getKeyString(const struct SRBRoot *bundle, const struct SResource *res, char temp[8]) {
    if (res->fKey == -1) {
        return NULL;
    }
#if PACK_KEYS
    return unpackKey(bundle->fKeys, res->fKey, temp);
#else
    return getKeyString(bundle, res->fKey);
#endif
}

const char *
bundle_getKeyBytes(struct SRBRoot *bundle, int32_t *pLength) {
    *pLength = bundle->fKeyPoint - URES_STRINGS_BOTTOM;
    return bundle->fKeys + URES_STRINGS_BOTTOM;
}

int32_t
bundle_addKeyBytes(struct SRBRoot *bundle, const char *keyBytes, int32_t length, UErrorCode *status) {
    int32_t keypos;

    if (U_FAILURE(*status)) {
        return -1;
    }
    if (length < 0 || (keyBytes == NULL && length != 0)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return -1;
    }
    if (length == 0) {
        return bundle->fKeyPoint;
    }

    keypos = bundle->fKeyPoint;
    bundle->fKeyPoint += length;
    if (bundle->fKeyPoint >= bundle->fKeysCapacity) {
        /* overflow - resize the keys buffer */
        bundle->fKeysCapacity += KEY_SPACE_SIZE;
        bundle->fKeys = uprv_realloc(bundle->fKeys, bundle->fKeysCapacity);
        if(bundle->fKeys == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return -1;
        }
    }

    uprv_memcpy(bundle->fKeys + keypos, keyBytes, length);

    return keypos;
}

int32_t
bundle_addtag(struct SRBRoot *bundle, const char *tag, UErrorCode *status) {
    int32_t keypos;
#if PACK_KEYS
    int32_t packed;
#endif

    if (U_FAILURE(*status)) {
        return -1;
    }

    if (tag == NULL) {
        /* no error: the root table and array items have no keys */
        return -1;
    }

#if PACK_KEYS
    packed = packKey(tag);
    if (packed >= 0) {
        return packed | 0x80000000;
    }
#endif

    keypos = bundle_addKeyBytes(bundle, tag, (int32_t)(uprv_strlen(tag) + 1), status);
    if (U_SUCCESS(*status)) {
        ++bundle->fKeysCount;
    }
    return keypos;
}

static int32_t
compareInt32(int32_t lPos, int32_t rPos) {
    /*
     * Compare possibly-negative key offsets. Don't just return lPos - rPos
     * because that is prone to negative-integer underflows.
     */
    if (lPos < rPos) {
        return -1;
    } else if (lPos > rPos) {
        return 1;
    } else {
        return 0;
    }
}

static int32_t U_CALLCONV
compareKeySuffixes(const void *context, const void *l, const void *r) {
    const struct SRBRoot *bundle=(const struct SRBRoot *)context;
    int32_t lPos = ((const KeyMapEntry *)l)->oldpos;
    int32_t rPos = ((const KeyMapEntry *)r)->oldpos;
    const char *lStart = getKeyString(bundle, lPos);
    const char *lLimit = lStart;
    const char *rStart = getKeyString(bundle, rPos);
    const char *rLimit = rStart;
    int32_t diff;
    while (*lLimit != 0) { ++lLimit; }
    while (*rLimit != 0) { ++rLimit; }
    /* compare keys in reverse character order */
    while (lStart < lLimit && rStart < rLimit) {
        diff = (int32_t)(uint8_t)*--lLimit - (int32_t)(uint8_t)*--rLimit;
        if (diff != 0) {
            return diff;
        }
    }
    /* sort equal suffixes by descending length */
    diff = (int32_t)(rLimit - rStart) - (int32_t)(lLimit - lStart);
    if (diff != 0) {
        return diff;
    }
    /* Sort pool bundle keys first (negative oldpos), and otherwise keys in parsing order. */
    return compareInt32(lPos, rPos);
}

static int32_t U_CALLCONV
compareKeyNewpos(const void *context, const void *l, const void *r) {
    return compareInt32(((const KeyMapEntry *)l)->newpos, ((const KeyMapEntry *)r)->newpos);
#if 0
    return ((const KeyMapEntry *)l)->newpos - ((const KeyMapEntry *)r)->newpos;
#endif
}

static int32_t U_CALLCONV
compareKeyOldpos(const void *context, const void *l, const void *r) {
    return compareInt32(((const KeyMapEntry *)l)->oldpos, ((const KeyMapEntry *)r)->oldpos);
#if 0
    return ((const KeyMapEntry *)l)->oldpos - ((const KeyMapEntry *)r)->oldpos;
#endif
}

void
bundle_compactKeys(struct SRBRoot *bundle, UErrorCode *status) {
    KeyMapEntry *map;
    char *keys;
    int32_t i;
    int32_t keysCount = bundle->fPoolBundleKeysCount + bundle->fKeysCount;
    if (U_FAILURE(*status) || bundle->fKeysCount == 0 || bundle->fKeyMap != NULL) {
        return;
    }
    map = (KeyMapEntry *)uprv_malloc(keysCount * sizeof(KeyMapEntry));
    if (map == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    keys = (char *)bundle->fPoolBundleKeys + bundle->fPoolBundleKeysBottom;
    for (i = 0; i < bundle->fPoolBundleKeysCount; ++i) {
        map[i].oldpos =
            (int32_t)(keys - bundle->fPoolBundleKeys) | 0x80000000;  /* negative oldpos */
        map[i].newpos = 0;
        while (*keys != 0) { ++keys; }  /* skip the key */
        ++keys;  /* skip the NUL */
    }
    assert(keys == bundle->fPoolBundleKeys + bundle->fPoolBundleKeysTop);
    keys = bundle->fKeys + URES_STRINGS_BOTTOM;
    for (; i < keysCount; ++i) {
        map[i].oldpos = (int32_t)(keys - bundle->fKeys);
        map[i].newpos = 0;
        while (*keys != 0) { ++keys; }  /* skip the key */
        ++keys;  /* skip the NUL */
    }
    assert(keys == bundle->fKeys + bundle->fKeyPoint);
    /* Sort the keys so that each one is immediately followed by all of its suffixes. */
    uprv_sortArray(map, keysCount, (int32_t)sizeof(KeyMapEntry),
                   compareKeySuffixes, bundle, FALSE, status);
    /*
     * Make suffixes point into earlier, longer strings that contain them
     * and mark the old, now unused suffix bytes as deleted.
     */
    if (U_SUCCESS(*status)) {
        keys = bundle->fKeys;
        for (i = 0; i < keysCount;) {
            /*
             * This key is not a suffix of the previous one;
             * keep this one and delete the following ones that are
             * suffixes of this one.
             */
            const char *key;
            const char *keyLimit;
            int32_t j = i + 1;
            map[i].newpos = map[i].oldpos;
            if (j < keysCount && map[j].oldpos < 0) {
                /* Key string from the pool bundle, do not delete. */
                i = j;
                continue;
            }
            key = getKeyString(bundle, map[i].oldpos);
            for (keyLimit = key; *keyLimit != 0; ++keyLimit) {}
            for (; j < keysCount && map[j].oldpos >= 0; ++j) {
                const char *k;
                char *suffix;
                const char *suffixLimit;
                int32_t offset;
                suffix = keys + map[j].oldpos;
                for (suffixLimit = suffix; *suffixLimit != 0; ++suffixLimit) {}
                offset = (int32_t)(keyLimit - key) - (suffixLimit - suffix);
                if (offset < 0) {
                    break;  /* suffix cannot be longer than the original */
                }
                /* Is it a suffix of the earlier, longer key? */
                for (k = keyLimit; suffix < suffixLimit && *--k == *--suffixLimit;) {}
                if (suffix == suffixLimit && *k == *suffixLimit) {
                    map[j].newpos = map[i].oldpos + offset;  /* yes, point to the earlier key */
                    /* mark the suffix as deleted */
                    while (*suffix != 0) { *suffix++ = 1; }
                    *suffix = 1;
                } else {
                    break;  /* not a suffix, restart from here */
                }
            }
            i = j;
        }
        /*
         * Re-sort by newpos, then modify the key characters array in-place
         * to squeeze out unused bytes, and readjust the newpos offsets.
         */
        uprv_sortArray(map, keysCount, (int32_t)sizeof(KeyMapEntry),
                       compareKeyNewpos, NULL, FALSE, status);
        if (U_SUCCESS(*status)) {
            int32_t oldpos, newpos, limit;
            int32_t i;
            oldpos = newpos = URES_STRINGS_BOTTOM;
            limit = bundle->fKeyPoint;
            /* skip key offsets that point into the pool bundle rather than this new bundle */
            for (i = 0; i < keysCount && map[i].newpos < 0; ++i) {}
            /* TODO: could skip this loop if i == keysCount */
            while (oldpos < limit) {
                if (keys[oldpos] == 1) {
                    ++oldpos;  /* skip unused bytes */
                } else {
                    /* adjust the new offsets for keys starting here */
                    while (i < keysCount && map[i].newpos == oldpos) {
                        map[i++].newpos = newpos;
                    }
                    /* move the key characters to their new position */
                    keys[newpos++] = keys[oldpos++];
                }
            }
            assert(i == keysCount);
            bundle->fKeyPoint = newpos;
            /* Re-sort once more, by old offsets for binary searching. */
            uprv_sortArray(map, keysCount, (int32_t)sizeof(KeyMapEntry),
                           compareKeyOldpos, NULL, FALSE, status);
            if (U_SUCCESS(*status)) {
                bundle->fKeysReduction = limit - newpos;
                bundle->fKeyMap = map;
                map = NULL;
            }
        }
    }
    uprv_free(map);
}

static int32_t
countInfixes(struct SResource *current) {
    int32_t numInfixes;
    for (numInfixes = 0;
         numInfixes < MAX_INFIXES && current->u.fString.fInfixes[numInfixes] != NULL;
         ++numInfixes) {}
    return numInfixes;
}

static UBool
hasInfixes(struct SResource *current) {
    return current->u.fString.fInfixes[0] != NULL;
}

static UBool
hasMaxInfixes(struct SResource *current) {
    return current->u.fString.fInfixes[MAX_INFIXES - 1] != NULL;
}

/*
 * Find a string that is an infix.
 * There might be other infix strings of the same length;
 * which one of such a set is returned is arbitrary.
 */
static struct SResource *
findInfix(struct SResource *text, int32_t start, int32_t limit, int32_t *index) {
    struct SResource *current;
    const UChar *textChars = text->u.fString.fChars + start;
    int32_t textLength = limit - start;
    if (textLength < 1 || !minBytesAreGE(textChars, textLength, MIN_INFIX_LENGTH)) {
        return NULL;  /* there is no string this short in the list */
    }

    for (current = text->u.fString.fShorter;
         current->u.fString.fLength >= 1;
         current = current->u.fString.fShorter
    ) {
        if (current->u.fString.fLength <= textLength) {
            const UChar *findPos =
                u_strFindFirst(textChars, textLength,
                               current->u.fString.fChars, current->u.fString.fLength);
            if (findPos != NULL) {
                *index = start + (int32_t)(findPos - textChars);
                return current;
            }
        }
    }
    return NULL;
}

/* current must not yet have MAX_INFIXES infixes */
static void
insertInfix(struct SResource *current, int32_t index, struct SResource *infix) {
    int32_t i, j;
    int8_t depth = infix->u.fString.fInfixDepth;
    assert(current != infix);
    if (depth >= current->u.fString.fInfixDepth) {
        current->u.fString.fInfixDepth = depth + 1;
    }
    infix->u.fString.fIsInfix = TRUE;
    /* keep infixes in ascending order of indexes */
    for (i = MAX_INFIXES - 1; i > 0; i = j) {
        j = i - 1;
        if (current->u.fString.fInfixes[j] != NULL) {
            if (index < current->u.fString.fInfixIndexes[j]) {
                current->u.fString.fInfixes[i] = current->u.fString.fInfixes[j];
                current->u.fString.fInfixIndexes[i] = current->u.fString.fInfixIndexes[j];
            } else {
                break;
            }
        }
    }
    current->u.fString.fInfixes[i] = infix;
    current->u.fString.fInfixIndexes[i] = index;
}

/* current must not yet have MAX_INFIXES infixes */
static void
findInfixesForString(struct SRBRoot *bundle, struct SResource *current) {
    int32_t length = current->u.fString.fLength;
    while (!hasMaxInfixes(current)) {
        int32_t start = 0, segment = 0;
        for (;;) {
            struct SResource *infix = NULL;
            int32_t limit = length;
            if (segment < MAX_INFIXES && (infix = current->u.fString.fInfixes[segment]) != NULL) {
                limit = current->u.fString.fInfixIndexes[segment];
            }
            if (start < limit) {
                int32_t index;
                struct SResource *newInfix = findInfix(current, start, limit, &index);
                if (newInfix != NULL && newInfix->u.fString.fInfixDepth < MAX_INFIX_DEPTH) {
                    insertInfix(current, index, newInfix);
                    /* TODO: printf("found infix[%4ld] in string[%6ld]\n", (long)newInfix->u.fString.fLength, (long)length); */
                    break;  /* try the remaining segments again */
                }
            }
            if (infix == NULL) {
                /* done: no new infixes found in any segment */
                return;
            }
            start = limit + infix->u.fString.fLength;
            ++segment;
        }
    }
}

static void
findInfixes(struct SRBRoot *bundle, UErrorCode *status) {
    struct SResource *current;

    if (U_FAILURE(*status)) {
        return;
    }
    /* Go from shortest to longest string for counting and capping the recursion depth. */
    for (current = bundle->fShortestString->u.fString.fLonger;
         current != bundle->fLongestString;
         current = current->u.fString.fLonger
    ) {
        if (!hasMaxInfixes(current)) {
            findInfixesForString(bundle, current);
        }
    }
}

/* get the length of the text before the first infix */
static int32_t
getFirstSegmentLength(struct SResource *text) {
    if (text->u.fString.fInfixes[0] != NULL) {
        return text->u.fString.fInfixIndexes[0];
    } else {
        return text->u.fString.fLength;
    }
}

/*
 * Returns another string with which text shares a common prefix.
 * text must not yet have MAX_INFIXES infixes.
 */
static struct SResource *
findPrefix(struct SResource *text, int32_t *commonLength) {
    struct SResource *current;
    const UChar *textChars = text->u.fString.fChars;
    int32_t textLength = getFirstSegmentLength(text);
    struct SResource *other;
    int32_t commonMinBytes;

    if (textLength < 1 || !minBytesAreGE(textChars, textLength, MIN_INFIX_LENGTH)) {
        return NULL;  /* there is no string this short in the list */
    }

    other = NULL;
    commonMinBytes = MIN_INFIX_LENGTH - 1;
    for (current = text->u.fString.fShorter;
         current->u.fString.fLength >= 1;
         current = current->u.fString.fShorter
    ) {
        if (!hasMaxInfixes(current)) {
            const UChar *currentChars = current->u.fString.fChars;
            int32_t currentLength = getFirstSegmentLength(current);
            const UChar *currentLimit = currentChars + currentLength;

            if (currentLength > 0 && minBytesAreGE(currentChars, currentLength, MIN_INFIX_LENGTH)) {
                /* does text share a prefix with current? */
                const UChar *pc = currentChars;
                const UChar *pt = textChars;
                if (*pc == *pt) {
                    int32_t prefixLength;
                    /*
                     * At the beginning of each loop iteration: *pc == *pt.
                     * After the loop, pc and pt are on the first difference.
                     */
                    while (++pc < currentLimit && *pc == *++pt) {}
                    /* adjust the end of the prefix to a code point boundary */
                    if (U16_IS_LEAD(*(pc - 1))) {
                        --pc;
                    }
                    prefixLength = (int32_t)(pc - currentChars);
                    /* is this the longest common prefix so far? */
                    if (minBytesAreGE(currentChars, prefixLength, commonMinBytes + 1)) {
                        commonMinBytes = minBytes(currentChars, prefixLength);
                        other = current;
                        *commonLength = prefixLength;
                    }
                }
            }
        }
    }
    return other;
}

/* get the start of the text after the last infix */
static int32_t
getLastSegmentStart(struct SResource *text) {
    if (hasInfixes(text)) {
        int32_t numInfixes = countInfixes(text);
        return text->u.fString.fInfixIndexes[numInfixes - 1] +
               text->u.fString.fInfixes[numInfixes - 1]->u.fString.fLength;
    } else {
        return 0;
    }
}

/*
 * Returns another string with which text shares a common suffix.
 * text must not yet have MAX_INFIXES infixes.
 */
static struct SResource *
findSuffix(struct SResource *text, int32_t *textStart, int32_t *otherStart, int32_t *commonLength) {
    struct SResource *current;
    int32_t start = getLastSegmentStart(text);
    const UChar *textChars = text->u.fString.fChars + start;
    int32_t textLength = text->u.fString.fLength - start;
    struct SResource *other;
    int32_t commonMinBytes;

    if (textLength < 1 || !minBytesAreGE(textChars, textLength, MIN_INFIX_LENGTH)) {
        return NULL;  /* there is no string this short in the list */
    }

    other = NULL;
    commonMinBytes = MIN_INFIX_LENGTH - 1;
    for (current = text->u.fString.fShorter;
         current->u.fString.fLength >= 1;
         current = current->u.fString.fShorter
    ) {
        if (!hasMaxInfixes(current)) {
            int32_t currentStart = getLastSegmentStart(current);
            const UChar *currentChars = current->u.fString.fChars + currentStart;
            int32_t currentLength = current->u.fString.fLength - currentStart;
            const UChar *currentLimit = currentChars + currentLength;

            if (currentLength > 0 && minBytesAreGE(currentChars, currentLength, MIN_INFIX_LENGTH)) {
                /* does text share a suffix with current? */
                const UChar *pc = currentLimit - 1;
                const UChar *pt = textChars + textLength - 1;
                if (*pc == *pt) {
                    int32_t suffixLength;
                    /*
                     * At the beginning of each loop iteration: *pc == *pt.
                     * After the loop, pc and pt are on the first (from the start)
                     * equal character in the suffix.
                     */
                    for (; pc > currentChars && *(pc - 1) == *(pt - 1); --pc, --pt) {}
                    /* adjust the start of the suffix to a code point boundary */
                    if (U16_IS_TRAIL(*pc)) {
                        ++pc;
                    }
                    suffixLength = (int32_t)(currentLimit - pc);
                    /* is this the longest common suffix so far? */
                    if (minBytesAreGE(pc, suffixLength, commonMinBytes + 1)) {
                        commonMinBytes = minBytes(pc, suffixLength);
                        other = current;
                        *textStart = start + (int32_t)(pt - textChars);
                        *otherStart = currentStart + (int32_t)(pc - currentChars);
                        *commonLength = suffixLength;
                    }
                }
            }
        }
    }
    return other;
}

static struct SResource *
findString(struct SRBRoot *bundle, const UChar *s, int32_t length) {
    if (bundle->fStringSet != NULL) {
        struct SResource temp = { URES_STRING };
        temp.u.fString.fChars = (UChar *)s;
        temp.u.fString.fLength = length;
        return uhash_get(bundle->fStringSet, &temp);
    }
    return NULL;
}

static void
insertCommonInfix(struct SRBRoot *bundle,
                  const UChar *commonChars, int32_t commonLength,
                  struct SResource *current, int32_t currentStart,
                  struct SResource *other, int32_t otherStart,
                  UErrorCode *status) {
    /* does this common infix already exist as a string? */
    const char *name = currentStart == 0 ? "prefix" : "suffix";
    struct SResource *infix;
    if (commonLength == other->u.fString.fLength) {
        infix = other;  /* the whole other string was matched */
    } else {
        infix = findString(bundle, commonChars, commonLength);
    }
    if (infix != NULL) {
        /* TODO: printf("found common infix[%3ld] (%s) that already exists as a string\n", commonLength, name); */
    } else if (minBytesAreGE(commonChars, commonLength, MIN_COMMON_INFIX_LENGTH)) {
        /* it is long enough to be worth creating a new string */
        /* TODO: printf("found common infix[%3ld] (%s), creating a new string\n", commonLength, name); */
        infix = string_open(bundle, NULL, commonChars, commonLength, NULL, status);
        if (U_FAILURE(*status)) {
            return;
        }
        infix->fNext = (struct SResource *)&kOutsideBundleTree;
    } else {
        return;
    }
    insertInfix(current, currentStart, infix);
    if (infix != other) {
        insertInfix(other, otherStart, infix);
    }
}

static void
findPrefixesAndSuffixes(struct SRBRoot *bundle, UErrorCode *status) {
    struct SResource *current;
    UBool addedStrings = FALSE;

    if (U_FAILURE(*status)) {
        return;
    }
    /* Go from longest to shortest string for longest prefixes and suffixes. */
    for (current = bundle->fLongestString->u.fString.fShorter;
         current != bundle->fShortestString && U_SUCCESS(*status);
         current = current->u.fString.fShorter
    ) {
        if (!hasMaxInfixes(current)) {
            int32_t commonLength;
            struct SResource *other = findPrefix(current, &commonLength);
            if (other != NULL) {
                insertCommonInfix(
                    bundle,
                    current->u.fString.fChars, commonLength,
                    current, 0, other, 0, status);
            }
        }
        if (!hasMaxInfixes(current) && U_SUCCESS(*status)) {
            int32_t currentStart, otherStart, commonLength;
            struct SResource *other = findSuffix(current, &currentStart, &otherStart, &commonLength);
            if (other != NULL) {
                insertCommonInfix(
                    bundle,
                    current->u.fString.fChars + currentStart, commonLength,
                    current, currentStart, other, otherStart, status);
            }
        }
    }
}

static void
bundle_compactStrings(struct SRBRoot *bundle, UErrorCode *status) {
    findPrefixesAndSuffixes(bundle, status);
    findInfixes(bundle, status);
}
