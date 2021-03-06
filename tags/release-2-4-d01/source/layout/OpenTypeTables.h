/*
 * @(#)OpenTypeTables.h	1.7 00/03/15
 *
 * (C) Copyright IBM Corp. 1998, 1999, 2000 - All Rights Reserved
 *
 */

#ifndef __OPENTYPETABLES_H
#define __OPENTYPETABLES_H

#include "LETypes.h"

U_NAMESPACE_BEGIN

#define ANY_NUMBER 1

typedef le_uint16 Offset;
typedef le_uint8  ATag[4];
typedef le_uint32 fixed32;

#define SWAPT(atag) ((LETag) ((atag[0] << 24) + (atag[1] << 16) + (atag[2] << 8) + atag[3]))

struct TagAndOffsetRecord
{
    ATag    tag;
    Offset  offset;
};

struct GlyphRangeRecord
{
    LEGlyphID firstGlyph;
    LEGlyphID lastGlyph;
    le_int16  rangeValue;
};

U_NAMESPACE_END
#endif
