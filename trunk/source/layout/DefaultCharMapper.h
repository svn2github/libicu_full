/*
 * %W% %W%
 *
 * (C) Copyright IBM Corp. 1998, 1999, 2000, 2001 - All Rights Reserved
 *
 */

#ifndef __DEFAULTCHARMAPPER_H
#define __DEFAULTCHARMAPPER_H

#include "LETypes.h"
#include "LEFontInstance.h"

/**
 * This class is an instance of LECharMapper which
 * implements control character filtering and bidi
 * mirroring.
 *
 * @see LECharMapper
 */
class DefaultCharMapper : public LECharMapper
{
private:
    le_bool fFilterControls;
    le_bool fMirror;

    static LEUnicode32 controlChars[];

    static const le_int32 controlCharsCount;

    static LEUnicode32 mirroredChars[];

    static const le_int32 mirroredCharsCount;

public:
    DefaultCharMapper(le_bool filterControls, le_bool mirror)
        : fFilterControls(filterControls), fMirror(mirror)
    {
        // nothing
    };

    ~DefaultCharMapper()
    {
        // nada
    };

    LEUnicode32 mapChar(LEUnicode32 ch) const;
};

#endif
