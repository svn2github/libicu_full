/*
 * @(#)ClassDefinitionTables.h	1.5 00/03/15
 *
 * (C) Copyright IBM Corp. 1998, 1999, 2000, 2001 - All Rights Reserved
 *
 */

#ifndef __CLASSDEFINITIONTABLES_H
#define __CLASSDEFINITIONTABLES_H

#include "LETypes.h"
#include "OpenTypeTables.h"

U_NAMESPACE_BEGIN

struct ClassDefinitionTable
{
    le_uint16 classFormat;

    le_int32  getGlyphClass(LEGlyphID glyphID) const;
    le_bool   hasGlyphClass(le_int32 glyphClass) const;
};

struct ClassDefFormat1Table : ClassDefinitionTable
{
    LEGlyphID  startGlyph;
    le_uint16  glyphCount;
    le_uint16  classValueArray[ANY_NUMBER];

    le_int32 getGlyphClass(LEGlyphID glyphID) const;
    le_bool  hasGlyphClass(le_int32 glyphClass) const;
};

struct ClassRangeRecord
{
    LEGlyphID start;
    LEGlyphID end;
    le_uint16 classValue;
};

struct ClassDefFormat2Table : ClassDefinitionTable
{
    le_uint16        classRangeCount;
    GlyphRangeRecord classRangeRecordArray[ANY_NUMBER];

    le_int32 getGlyphClass(LEGlyphID glyphID) const;
    le_bool hasGlyphClass(le_int32 glyphClass) const;
};

U_NAMESPACE_END
#endif
