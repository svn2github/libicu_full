/*
*******************************************************************************
*
*   Copyright (C) 2000-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  genmbcs.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2000jul10
*   created by: Markus W. Scherer
*/

#ifndef __GENMBCS_H__
#define __GENMBCS_H__

#include "makeconv.h"

enum {
    /* TODO: why not use ucnvmbcs.h constants? */
    MBCS_STAGE_2_SHIFT=4,
    MBCS_STAGE_2_BLOCK_SIZE=0x40,       /* =64=1<<6 for 6 bits in stage 2 */
    MBCS_STAGE_2_BLOCK_SIZE_SHIFT=6,    /* log2(MBCS_STAGE_2_BLOCK_SIZE) */
    MBCS_STAGE_2_BLOCK_MASK=0x3f,       /* for after shifting by MBCS_STAGE_2_SHIFT */
    MBCS_STAGE_1_SHIFT=10,
    MBCS_STAGE_1_BMP_SIZE=0x40, /* 0x10000>>MBCS_STAGE_1_SHIFT, or 16 for one entry per 1k code points on the BMP */
    MBCS_STAGE_1_SIZE=0x440,    /* 0x110000>>MBCS_STAGE_1_SHIFT, or 17*64 for one entry per 1k code points */
    MBCS_STAGE_2_SIZE=0xfbc0,   /* 0x10000-MBCS_STAGE_1_SIZE: stages 1 & 2 share a 16-bit-indexed array */
    MBCS_MAX_STAGE_2_TOP=MBCS_STAGE_2_SIZE,
    MBCS_STAGE_2_MAX_BLOCKS=MBCS_STAGE_2_SIZE>>MBCS_STAGE_2_BLOCK_SIZE_SHIFT,

    MBCS_STAGE_2_ALL_UNASSIGNED_INDEX=0, /* stage 1 entry for the all-unassigned stage 2 block */
    MBCS_STAGE_2_FIRST_ASSIGNED=MBCS_STAGE_2_BLOCK_SIZE, /* start of the first stage 2 block after the all-unassigned one */

    MBCS_STAGE_3_BLOCK_SIZE=16,         /* =16=1<<4 for 4 bits in stage 3 */
    MBCS_STAGE_3_BLOCK_MASK=0xf,
    MBCS_STAGE_3_FIRST_ASSIGNED=MBCS_STAGE_3_BLOCK_SIZE, /* start of the first stage 3 block after the all-unassigned one */

    MBCS_STAGE_3_GRANULARITY=16,        /* =1<<4: MBCS stage 2 indexes are shifted left 4 */
    MBCS_STAGE_3_SBCS_SIZE=0x10000,     /* max 64k mappings for SBCS */
    MBCS_STAGE_3_MBCS_SIZE=0x10000*MBCS_STAGE_3_GRANULARITY, /* max mappings for MBCS */

    /*
     * Note: SBCS_UTF8_MAX must be at least 0x7ff with the current format version.
     * Theoretically possible values are 0x7ff..0xffff, in steps of 0x100.
     * Unlike for MBCS, this constant only affects the stage 3 block allocation size;
     * there is no additional stage 1/2 table stored in the .cnv file.
     * The max value must be at least 0x7ff to cover 2-byte UTF-8.
     * Higher values up to 0x1fff are harmless and potentially useful because
     * that covers small-script blocks which usually have either dense mappings
     * or no mappings at all.
     * Starting at U+2000, there are mostly symbols and format characters
     * with a low density of SBCS mappings, which would result in more wasted
     * stage 3 entries with the larger block size.
     */
    SBCS_UTF8_MAX=0x1fff,                /* maximum code point with UTF-8-friendly SBCS data structures */

    /*
     * Note: MBCS_UTF8_MAX cannot be _decreased_ without a major format version change!
     * Theoretically possible values are 0xd7ff..0xfeff, in steps of 0x100.
     *
     * (The theoretical maximum is 0xfeff not 0xffff due to a limitation in MBCSAddFromUnicode().
     * With a limit of 0xffff we would have to detect a possible (but _extremely_ rare)
     * overflow there, turn off utf8Friendly, and start over with MBCSAddTable().)
     *
     * 0xd7ff is chosen for the majority of common characters including Unihan and Hangul.
     * At U+d800 there are mostly surrogates, private use codes, compatibility characters, etc.
     * Larger values cause slightly larger MBCS .cnv files.
     */
    MBCS_UTF8_MAX=0xd7ff,               /* maximum code point with UTF-8-friendly MBCS data structures */
    MBCS_UTF8_LIMIT=MBCS_UTF8_MAX+1,    /* =0xd800 */

    MBCS_UTF8_STAGE_SHIFT=6,
#if 0
    MBCS_UTF8_STAGE_1_SHIFT=12,
    MBCS_UTF8_STAGE_2_SHIFT=6,
#endif
    MBCS_UTF8_STAGE_3_BLOCK_SIZE=0x40,  /* =64=1<<6 for 6 bits from last trail byte */
    MBCS_UTF8_STAGE_3_BLOCK_MASK=0x3f,

    /* size of the single-stage table for up to U+d7ff (used instead of stage1/2) */
    MBCS_UTF8_STAGE_SIZE=MBCS_UTF8_LIMIT>>MBCS_UTF8_STAGE_SHIFT, /* =0x360 */

#if 0
    /* TODO: remove the following MBCS_UTF8_STAGE_XYZ constants */
    MBCS_UTF8_STAGE_1_SIZE=16,          /* =1<<4 for 4 bits from UTF-8 lead byte */
    MBCS_UTF8_STAGE_SIZE=0xff0,         /* =255<<4: size will be rounded up, >>4, and stored in one byte */
    MBCS_UTF8_STAGE_2_SIZE=0xfe0,       /* =MBCS_UTF8_STAGE_SIZE-MBCS_UTF8_STAGE_1_SIZE, both stages stored together */
#endif

    MBCS_FROM_U_EXT_FLAG=0x10,          /* UCMapping.f bit for base table mappings that fit into the base toU table */
    MBCS_FROM_U_EXT_MASK=0x0f,          /* but need to go into the extension fromU table */

#if 0
    MBCS_UTF8_TRAIL_BLOCK_SIZE=0x40,    /* =64=1<<6 code points for final UTF-8 trail byte */
    MBCS_UTF8_DOUBLE_TRAIL_BLOCK_SIZE=0x1000, /* =4096=1<<(6+6) code points for final two UTF-8 trail bytes */
    MBCS_UTF8_TWO_BYTE_BLOCK_SIZE=0x800, /* =2048=1<<(5+6) code points for two-byte UTF-8 (lead & trail bytes) */

    /* =256 size of a UTF-8-friendly stage 2 block given regular stage 3 blocks */
    MBCS_UTF8_STAGE_2_BLOCK_SIZE=MBCS_UTF8_DOUBLE_TRAIL_BLOCK_SIZE/MBCS_STAGE_3_BLOCK_SIZE,

    /* =128 size of a two-byte UTF-8-friendly stage 2 block given regular stage 3 blocks */
    MBCS_UTF8_TWO_BYTE_STAGE_2_BLOCK_SIZE=MBCS_UTF8_TWO_BYTE_BLOCK_SIZE/MBCS_STAGE_3_BLOCK_SIZE,
#endif

    /* =4 number of regular stage 3 blocks for final UTF-8 trail byte */
    MBCS_UTF8_STAGE_3_BLOCKS=MBCS_UTF8_STAGE_3_BLOCK_SIZE/MBCS_STAGE_3_BLOCK_SIZE,

#if 0
    /* =4 number of regular stage 2 blocks per final two UTF-8 trail bytes */
    MBCS_UTF8_STAGE_2_BLOCKS=MBCS_UTF8_STAGE_2_BLOCK_SIZE/MBCS_STAGE_2_BLOCK_SIZE,

    /* =2 number of regular stage 2 blocks per two-byte UTF-8 (lead & trail bytes) */
    MBCS_UTF8_TWO_BYTE_STAGE_2_BLOCKS=MBCS_UTF8_TWO_BYTE_STAGE_2_BLOCK_SIZE/MBCS_STAGE_2_BLOCK_SIZE,
#endif

    MBCS_MAX_FALLBACK_COUNT=8192
};

U_CFUNC NewConverter *
MBCSOpen(UCMFile *ucm);

/* Test if a 1:1 mapping fits into the MBCS base table's fromUnicode structure. */
U_CFUNC UBool
MBCSOkForBaseFromUnicode(UBool utf8Friendly,
                         const uint8_t *bytes, int32_t length,
                         UChar32 c, int8_t flag);

U_CFUNC NewConverter *
CnvExtOpen(UCMFile *ucm);

#endif
