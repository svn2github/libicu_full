/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2011, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************
 *
 * File MSGFMT.CPP
 *
 * Modification History:
 *
 *   Date        Name        Description
 *   02/19/97    aliu        Converted from java.
 *   03/20/97    helena      Finished first cut of implementation.
 *   04/10/97    aliu        Made to work on AIX.  Added stoi to replace wtoi.
 *   06/11/97    helena      Fixed addPattern to take the pattern correctly.
 *   06/17/97    helena      Fixed the getPattern to return the correct pattern.
 *   07/09/97    helena      Made ParsePosition into a class.
 *   02/22/99    stephen     Removed character literals for EBCDIC safety
 *   11/01/09    kirtig      Added SelectFormat
 ********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/appendable.h"
#include "unicode/choicfmt.h"
#include "unicode/datefmt.h"
#include "unicode/decimfmt.h"
#include "unicode/localpointer.h"
#include "unicode/msgfmt.h"
#include "unicode/plurfmt.h"
#include "unicode/rbnf.h"
#include "unicode/selfmt.h"
#include "unicode/smpdtfmt.h"
#include "unicode/uchar.h"
#include "unicode/ucnv_err.h"
#include "unicode/umsg.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "messageimpl.h"
#include "msgfmt_impl.h"
#include "uassert.h"
#include "ustrfmt.h"
#include "util.h"
#include "uvector.h"

// *****************************************************************************
// class MessageFormat
// *****************************************************************************

#define SINGLE_QUOTE      ((UChar)0x0027)
#define COMMA             ((UChar)0x002C)
#define LEFT_CURLY_BRACE  ((UChar)0x007B)
#define RIGHT_CURLY_BRACE ((UChar)0x007D)

//---------------------------------------
// static data

static const UChar ID_NUMBER[]    = {
    0x6E, 0x75, 0x6D, 0x62, 0x65, 0x72, 0  /* "number" */
};
static const UChar ID_DATE[]      = {
    0x64, 0x61, 0x74, 0x65, 0              /* "date" */
};
static const UChar ID_TIME[]      = {
    0x74, 0x69, 0x6D, 0x65, 0              /* "time" */
};
static const UChar ID_SPELLOUT[]  = {
    0x73, 0x70, 0x65, 0x6c, 0x6c, 0x6f, 0x75, 0x74, 0 /* "spellout" */
};
static const UChar ID_ORDINAL[]   = {
    0x6f, 0x72, 0x64, 0x69, 0x6e, 0x61, 0x6c, 0 /* "ordinal" */
};
static const UChar ID_DURATION[]  = {
    0x64, 0x75, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0 /* "duration" */
};

// MessageFormat Type List  Number, Date, Time or Choice
static const UChar * const TYPE_IDS[] = {
    ID_NUMBER,
    ID_DATE,
    ID_TIME,
    ID_SPELLOUT,
    ID_ORDINAL,
    ID_DURATION,
    NULL,
};

static const UChar ID_EMPTY[]     = {
    0 /* empty string, used for default so that null can mark end of list */
};
static const UChar ID_CURRENCY[]  = {
    0x63, 0x75, 0x72, 0x72, 0x65, 0x6E, 0x63, 0x79, 0  /* "currency" */
};
static const UChar ID_PERCENT[]   = {
    0x70, 0x65, 0x72, 0x63, 0x65, 0x6E, 0x74, 0        /* "percent" */
};
static const UChar ID_INTEGER[]   = {
    0x69, 0x6E, 0x74, 0x65, 0x67, 0x65, 0x72, 0        /* "integer" */
};

// NumberFormat modifier list, default, currency, percent or integer
static const UChar * const NUMBER_STYLE_IDS[] = {
    ID_EMPTY,
    ID_CURRENCY,
    ID_PERCENT,
    ID_INTEGER,
    NULL,
};

static const UChar ID_SHORT[]     = {
    0x73, 0x68, 0x6F, 0x72, 0x74, 0        /* "short" */
};
static const UChar ID_MEDIUM[]    = {
    0x6D, 0x65, 0x64, 0x69, 0x75, 0x6D, 0  /* "medium" */
};
static const UChar ID_LONG[]      = {
    0x6C, 0x6F, 0x6E, 0x67, 0              /* "long" */
};
static const UChar ID_FULL[]      = {
    0x66, 0x75, 0x6C, 0x6C, 0              /* "full" */
};

// DateFormat modifier list, default, short, medium, long or full
static const UChar * const DATE_STYLE_IDS[] = {
    ID_EMPTY,
    ID_SHORT,
    ID_MEDIUM,
    ID_LONG,
    ID_FULL,
    NULL,
};

static const UChar NULL_STRING[]      = {
    0x6E, 0x75, 0x6C, 0x6C, 0              /* "null" */
};

static const U_NAMESPACE_QUALIFIER DateFormat::EStyle DATE_STYLES[] = {
    U_NAMESPACE_QUALIFIER DateFormat::kDefault,
    U_NAMESPACE_QUALIFIER DateFormat::kShort,
    U_NAMESPACE_QUALIFIER DateFormat::kMedium,
    U_NAMESPACE_QUALIFIER DateFormat::kLong,
    U_NAMESPACE_QUALIFIER DateFormat::kFull,
};

static const int32_t DEFAULT_INITIAL_CAPACITY = 10;

U_CDECL_BEGIN
static UBool U_CALLCONV equalFormatsForHash(const UHashTok key1,
                                            const UHashTok key2) {
    return U_NAMESPACE_QUALIFIER MessageFormat::equalFormats(key1.pointer, key2.pointer);
}

U_CDECL_END

U_NAMESPACE_BEGIN

// -------------------------------------
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(MessageFormat)
UOBJECT_DEFINE_NO_RTTI_IMPLEMENTATION(MessageFormat::DummyFormat)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FormatNameEnumeration)

//--------------------------------------------------------------------

/**
 * Convert an integer value to a string and append the result to
 * the given UnicodeString.
 */
static UnicodeString& itos(int32_t i, UnicodeString& appendTo) {
    UChar temp[16];
    uprv_itou(temp,16,i,10,0); // 10 == radix
    appendTo.append(temp);
    return appendTo;
}


// AppendableWrapper: encapsulates the result of formatting, keeping track
// of the string and its length.
class AppendableWrapper : public UMemory {
  public:
    AppendableWrapper(Appendable& appendable) : app(appendable), len(0) {
    }
    void append(const UnicodeString& s) {
        app.appendString(s.getBuffer(), s.length());
        len += s.length();
    }
    void append(const UChar* s, const int32_t s_len) {
        app.appendString(s, s_len);
        len += s_len;
    }
    void append(const UnicodeString& s, int32_t start, int32_t length) {
        append(s.tempSubString(start, length));
    }
    void formatAndAppend(const Format* formatter, const Formattable& arg, UErrorCode& ec) {
        UnicodeString s;
        formatter->format(arg, s, ec);
        if (U_SUCCESS(ec)) {
            append(s);
        }
    }
    int32_t length() {
        return len;
    }
  private:
    Appendable& app;
    int32_t len;
};

/*
 * A structure representing one subformat of this MessageFormat.
 * Each subformat has a Format object, an offset into the plain
 * pattern text fPattern, and an argument number.  The argument
 * number corresponds to the array of arguments to be formatted.
 * @internal
 */
class MessageFormat::Subformat : public UMemory {
public:
    /**
     * @internal
     */
    Format* format; // formatter
    /**
     * @internal
     */
    int32_t offset; // offset into fPattern
    /**
     * @internal
     */
    // TODO (claireho) or save the number to argName and use itos to convert to number.=> we need this number
    int32_t argNum;    // 0-based argument number
    /**
     * @internal
     */
    UnicodeString* argName; // argument name or number

    /**
     * Clone that.format and assign it to this.format
     * Do NOT delete this.format
     * @internal
     */
    Subformat& operator=(const Subformat& that) {
        if (this != &that) {
            format = that.format ? that.format->clone() : NULL;
            offset = that.offset;
            argNum = that.argNum;
            argName = (that.argNum==-1) ? new UnicodeString(*that.argName): NULL;
        }
        return *this;
    }

    /**
     * @internal
     */
    UBool operator==(const Subformat& that) const {
        // Do cheap comparisons first
        return offset == that.offset &&
               argNum == that.argNum &&
               ((argName == that.argName) ||
                (*argName == *that.argName)) &&
               ((format == that.format) || // handles NULL
                (*format == *that.format));
    }

    /**
     * @internal
     */
    UBool operator!=(const Subformat& that) const {
        return !operator==(that);
    }
};

// -------------------------------------
// Creates a MessageFormat instance based on the pattern.

MessageFormat::MessageFormat(const UnicodeString& pattern,
                             UErrorCode& success)
: fLocale(Locale::getDefault()),  // Uses the default locale
  msgPattern(success),
  formatAliases(NULL),
  formatAliasesCapacity(0),
  idStart(UCHAR_ID_START),
  idContinue(UCHAR_ID_CONTINUE),
  subformats(NULL),
  subformatCount(0),
  subformatCapacity(0),
  argTypes(NULL),
  argTypeCount(0),
  argTypeCapacity(0),
  isArgNumeric(TRUE),
  defaultNumberFormat(NULL),
  defaultDateFormat(NULL),
  cachedFormatters(NULL),
  customFormatArgStarts(NULL),
  pluralProvider(&fLocale)
{
    if (!allocateSubformats(DEFAULT_INITIAL_CAPACITY) ||
        !allocateArgTypes(DEFAULT_INITIAL_CAPACITY)) {
        success = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    applyPattern(pattern, success);
    setLocaleIDs(fLocale.getName(), fLocale.getName());
}

MessageFormat::MessageFormat(const UnicodeString& pattern,
                             const Locale& newLocale,
                             UErrorCode& success)
: fLocale(newLocale),
  msgPattern(success),
  formatAliases(NULL),
  formatAliasesCapacity(0),
  idStart(UCHAR_ID_START),
  idContinue(UCHAR_ID_CONTINUE),
  subformats(NULL),
  subformatCount(0),
  subformatCapacity(0),
  argTypes(NULL),
  argTypeCount(0),
  argTypeCapacity(0),
  isArgNumeric(TRUE),
  defaultNumberFormat(NULL),
  defaultDateFormat(NULL),
  cachedFormatters(NULL),
  customFormatArgStarts(NULL),
  pluralProvider(&fLocale)
{
    if (!allocateSubformats(DEFAULT_INITIAL_CAPACITY) ||
        !allocateArgTypes(DEFAULT_INITIAL_CAPACITY)) {
        success = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    applyPattern(pattern, success);
    setLocaleIDs(fLocale.getName(), fLocale.getName());
}

MessageFormat::MessageFormat(const UnicodeString& pattern,
                             const Locale& newLocale,
                             UParseError& parseError,
                             UErrorCode& success)
: fLocale(newLocale),
  msgPattern(success),
  formatAliases(NULL),
  formatAliasesCapacity(0),
  idStart(UCHAR_ID_START),
  idContinue(UCHAR_ID_CONTINUE),
  subformats(NULL),
  subformatCount(0),
  subformatCapacity(0),
  argTypes(NULL),
  argTypeCount(0),
  argTypeCapacity(0),
  isArgNumeric(TRUE),
  defaultNumberFormat(NULL),
  defaultDateFormat(NULL),
  cachedFormatters(NULL),
  customFormatArgStarts(NULL),
  pluralProvider(&fLocale)
{
    if (!allocateSubformats(DEFAULT_INITIAL_CAPACITY) ||
        !allocateArgTypes(DEFAULT_INITIAL_CAPACITY)) {
        success = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    applyPattern(pattern, parseError, success);
    setLocaleIDs(fLocale.getName(), fLocale.getName());
}

MessageFormat::MessageFormat(const MessageFormat& that)
:
  Format(that),
  msgPattern(that.msgPattern),
  formatAliases(NULL),
  formatAliasesCapacity(0),
  idStart(UCHAR_ID_START),
  idContinue(UCHAR_ID_CONTINUE),
  subformats(NULL),
  subformatCount(0),
  subformatCapacity(0),
  argTypes(NULL),
  argTypeCount(0),
  argTypeCapacity(0),
  isArgNumeric(TRUE),
  defaultNumberFormat(NULL),
  defaultDateFormat(NULL),
  cachedFormatters(NULL),
  customFormatArgStarts(NULL),
  pluralProvider(&fLocale)
{
    *this = that;
    UErrorCode ec = U_ZERO_ERROR;

    // This will take care of creating the hash tables (since they are NULL).
    CopyHashTables(that, ec);
    if (U_FAILURE(ec)) {
        resetPattern();
    }
}

MessageFormat::~MessageFormat()
{
    uhash_close(cachedFormatters);
    uhash_close(customFormatArgStarts);

    uprv_free(argTypes);
    argTypes = NULL;
    argTypeCount = argTypeCapacity = 0;

    uprv_free(formatAliases);
    delete defaultNumberFormat;
    delete defaultDateFormat;
}

//--------------------------------------------------------------------
// Variable-size array management

/**
 * Allocate subformats[] to at least the given capacity and return
 * TRUE if successful.  If not, leave subformats[] unchanged.
 *
 * If subformats is NULL, allocate it.  If it is not NULL, enlarge it
 * if necessary to be at least as large as specified.
 */
UBool MessageFormat::allocateSubformats(int32_t capacity) {
    if (subformats == NULL) {
        subformats = (Subformat*) uprv_malloc(sizeof(*subformats) * capacity);
        subformatCapacity = capacity;
        subformatCount = 0;
        if (subformats == NULL) {
            subformatCapacity = 0;
            return FALSE;
        }
    } else if (subformatCapacity < capacity) {
        if (capacity < 2*subformatCapacity) {
            capacity = 2*subformatCapacity;
        }
        Subformat* a = (Subformat*)
            uprv_realloc(subformats, sizeof(*subformats) * capacity);
        if (a == NULL) {
            return FALSE; // request failed
        }
        subformats = a;
        subformatCapacity = capacity;
    }
    return TRUE;
}

/**
 * Allocate argTypes[] to at least the given capacity and return
 * TRUE if successful.  If not, leave argTypes[] unchanged.
 *
 * If argTypes is NULL, allocate it.  If it is not NULL, enlarge it
 * if necessary to be at least as large as specified.
 */
UBool MessageFormat::allocateArgTypes(int32_t capacity) {
    if (argTypes == NULL) {
        argTypes = (Formattable::Type*) uprv_malloc(sizeof(*argTypes) * capacity);
        argTypeCount = 0;
        argTypeCapacity = capacity;
        if (argTypes == NULL) {
            argTypeCapacity = 0;
            return FALSE;
        }
        for (int32_t i=0; i<capacity; ++i) {
            argTypes[i] = Formattable::kString;
        }
    } else if (argTypeCapacity < capacity) {
        if (capacity < 2*argTypeCapacity) {
            capacity = 2*argTypeCapacity;
        }
        Formattable::Type* a = (Formattable::Type*)
            uprv_realloc(argTypes, sizeof(*argTypes) * capacity);
        if (a == NULL) {
            return FALSE; // request failed
        }
        for (int32_t i=argTypeCapacity; i<capacity; ++i) {
            a[i] = Formattable::kString;
        }
        argTypes = a;
        argTypeCapacity = capacity;
    }
    return TRUE;
}

// -------------------------------------
// assignment operator

const MessageFormat&
MessageFormat::operator=(const MessageFormat& that)
{
    // Reallocate the arrays BEFORE changing this object
    if (this != &that &&
        allocateSubformats(that.subformatCount) &&
        allocateArgTypes(that.argTypeCount)) {

        // Calls the super class for assignment first.
        Format::operator=(that);

        fPattern = that.fPattern;
        setLocale(that.fLocale);
        isArgNumeric = that.isArgNumeric;
        int32_t j;
        for (j=0; j<subformatCount; ++j) {
            delete subformats[j].format;
        }
        subformatCount = 0;

        for (j=0; j<that.subformatCount; ++j) {
            // Subformat::operator= does NOT delete this.format
            subformats[j] = that.subformats[j];
        }
        subformatCount = that.subformatCount;

        for (j=0; j<that.argTypeCount; ++j) {
            argTypes[j] = that.argTypes[j];
        }
        argTypeCount = that.argTypeCount;

        UErrorCode ec;
        CopyHashTables(that, ec);
        if (U_FAILURE(ec)) {
            resetPattern();
        }
    }
    return *this;
}

UBool
MessageFormat::operator==(const Format& rhs) const
{
    if (this == &rhs) return TRUE;

    MessageFormat& that = (MessageFormat&)rhs;

    // Check class ID before checking MessageFormat members
    if (!Format::operator==(rhs) ||
        fPattern != that.fPattern ||
        fLocale != that.fLocale ||
        isArgNumeric != that.isArgNumeric) {
        return FALSE;
    }

    int32_t j;
    for (j=0; j<subformatCount; ++j) {
        if (subformats[j] != that.subformats[j]) {
            return FALSE;
        }
    }

    // Compare hashtables.
    if ((customFormatArgStarts == NULL && that.customFormatArgStarts != NULL) ||
        (customFormatArgStarts != NULL && that.customFormatArgStarts == NULL) ) {
        return FALSE;
    }
    if (customFormatArgStarts == NULL && that.customFormatArgStarts == NULL) {
        return TRUE;
    }

    UErrorCode ec = U_ZERO_ERROR;
    const int32_t count = uhash_count(customFormatArgStarts);
    const int32_t rhs_count = uhash_count(that.customFormatArgStarts);
    if (count != rhs_count) {
        return FALSE;
    }
    int idx = 0, rhs_idx = 0, pos = -1, rhs_pos = -1;
    for (; idx < count && rhs_idx < rhs_count && U_SUCCESS(ec); ++idx, ++rhs_idx) {
        const UHashElement* cur = uhash_nextElement(customFormatArgStarts, &pos);
        const UHashElement* rhs_cur = uhash_nextElement(that.customFormatArgStarts, &rhs_pos);
        if (cur->key.integer != rhs_cur->key.integer) {
            return FALSE;
        }
        const Format* format = (Format*)uhash_iget(cachedFormatters, cur->key.integer);
        const Format* rhs_format = (Format*)uhash_iget(that.cachedFormatters, rhs_cur->key.integer);
        if (*format != *rhs_format) {
            return FALSE;
        }
    }
    return TRUE;
}

// -------------------------------------
// Creates a copy of this MessageFormat, the caller owns the copy.

Format*
MessageFormat::clone() const
{
    return new MessageFormat(*this);
}

// -------------------------------------
// Sets the locale of this MessageFormat object to theLocale.

void
MessageFormat::setLocale(const Locale& theLocale)
{
    if (fLocale != theLocale) {
        delete defaultNumberFormat;
        defaultNumberFormat = NULL;
        delete defaultDateFormat;
        defaultDateFormat = NULL;
    }
    fLocale = theLocale;
    setLocaleIDs(fLocale.getName(), fLocale.getName());
    pluralProvider.reset();
}

// -------------------------------------
// Gets the locale of this MessageFormat object.

const Locale&
MessageFormat::getLocale() const
{
    return fLocale;
}




void
MessageFormat::applyPattern(const UnicodeString& newPattern,
                            UErrorCode& status)
{
    UParseError parseError;
    applyPattern(newPattern,parseError,status);
}


// -------------------------------------
// Applies the new pattern and returns an error if the pattern
// is not correct.
void
MessageFormat::applyPattern(const UnicodeString& pattern,
                            UParseError& parseError,
                            UErrorCode& ec)
{
    if(U_FAILURE(ec)) {
        return;
    }
    msgPattern.parse(pattern, &parseError, ec);
    cacheExplicitFormats(ec);

    if (U_FAILURE(ec)) {
        resetPattern();
    }
}

void MessageFormat::resetPattern() {
    msgPattern.clear();
    uhash_close(cachedFormatters);
    cachedFormatters = NULL;
    uhash_close(customFormatArgStarts);
    customFormatArgStarts = NULL;
}

void
MessageFormat::applyPattern(const UnicodeString& pattern,
                            UMessagePatternApostropheMode aposMode,
                            UParseError* parseError,
                            UErrorCode& status) {
    if (aposMode != msgPattern.getApostropheMode()) {
        msgPattern.clearPatternAndSetApostropheMode(aposMode);
    }
    applyPattern(pattern, *parseError, status);
}

// -------------------------------------
// Converts this MessageFormat instance to a pattern.

UnicodeString&
MessageFormat::toPattern(UnicodeString& appendTo) const {
    if (customFormatArgStarts || 0 == msgPattern.countParts()) {
        appendTo.setToBogus();
        return appendTo;
    }
    appendTo.append(msgPattern.getPatternString());
    return appendTo;
}

int32_t MessageFormat::nextTopLevelArgStart(int32_t partIndex) const {
    if (partIndex != 0) {
        partIndex = msgPattern.getLimitPartIndex(partIndex);
    }
    for (;;) {
        UMessagePatternPartType type = msgPattern.getPartType(++partIndex);
        if (type == UMSGPAT_PART_TYPE_ARG_START) {
            return partIndex;
        }
        if (type == UMSGPAT_PART_TYPE_MSG_LIMIT) {
            return -1;
        }
    }
}

void MessageFormat::setArgStartFormat(int32_t argStart,
                                      Format* formatter,
                                      UErrorCode& status) {
    if (U_FAILURE(status)) {
        delete formatter;
    }
    if (cachedFormatters == NULL) {
        cachedFormatters=uhash_open(uhash_hashLong, uhash_compareLong,
                                    equalFormatsForHash, &status);
        if (U_FAILURE(status)) {
            delete formatter;
            return;
        }
        uhash_setValueDeleter(cachedFormatters, uhash_deleteUObject);
    }
    if (formatter == NULL) {
        formatter = new DummyFormat();
    }
    uhash_iput(cachedFormatters, argStart, formatter, &status);
    if (U_FAILURE(status)) {
        delete formatter;
    }
}


bool MessageFormat::argNameMatches(int32_t partIndex, const UnicodeString& argName, int32_t argNumber) {
    const MessagePattern::Part& part = msgPattern.getPart(partIndex);
    return part.getType() == UMSGPAT_PART_TYPE_ARG_NAME ?
        msgPattern.partSubstringMatches(part, argName) :
        part.getValue() == argNumber;  // ARG_NUMBER
}

// Sets a custom formatter for a MessagePattern ARG_START part index.
// "Custom" formatters are provided by the user via setFormat() or similar APIs.
void MessageFormat::setCustomArgStartFormat(int32_t argStart,
                                            Format* formatter,
                                            UErrorCode& status) {
    setArgStartFormat(argStart, formatter, status);
    if (customFormatArgStarts == NULL) {
        customFormatArgStarts=uhash_open(uhash_hashLong, uhash_compareLong,
                                         NULL, &status);
    }
    uhash_iputi(customFormatArgStarts, argStart, 1, &status);
}

Format* MessageFormat::getCachedFormatter(int32_t argumentNumber) const {
    if (!cachedFormatters) {
        return NULL;
    }
    void* ptr = uhash_iget(cachedFormatters, argumentNumber);
    if (ptr && dynamic_cast<DummyFormat*>((Format*)ptr) == NULL) {
        return (Format*) ptr;
    } else {
        return NULL;
    }
}

// -------------------------------------
// Adopts the new formats array and updates the array count.
// This MessageFormat instance owns the new formats.
void
MessageFormat::adoptFormats(Format** newFormats,
                            int32_t count) {
    if (newFormats == NULL || count < 0) {
        return;
    }
    // Throw away any cached formatters.
    if (cachedFormatters) {
        uhash_removeAll(cachedFormatters);
    }
    if (customFormatArgStarts) {
        uhash_removeAll(customFormatArgStarts);
    }

    int32_t formatNumber = 0;
    UErrorCode status = U_ZERO_ERROR;
    for (int32_t partIndex = 0;
        formatNumber < count && U_SUCCESS(status) &&
            (partIndex = nextTopLevelArgStart(partIndex)) >= 0;) {
        setCustomArgStartFormat(partIndex, newFormats[formatNumber], status);
        ++formatNumber;
    }
    // Delete those that didn't get used (if any).
    for (; formatNumber < count; ++formatNumber) {
        delete newFormats[formatNumber];
    }

}

// -------------------------------------
// Sets the new formats array and updates the array count.
// This MessageFormat instance maks a copy of the new formats.

void
MessageFormat::setFormats(const Format** newFormats,
                          int32_t count) {
    if (newFormats == NULL || count < 0) {
        return;
    }
    // Throw away any cached formatters.
    if (cachedFormatters) {
        uhash_removeAll(cachedFormatters);
    }
    if (customFormatArgStarts) {
        uhash_removeAll(customFormatArgStarts);
    }

    UErrorCode status = U_ZERO_ERROR;
    int32_t formatNumber = 0;
    for (int32_t partIndex = 0;
        formatNumber < count && U_SUCCESS(status) && (partIndex = nextTopLevelArgStart(partIndex)) >= 0;) {
      Format* new_format = NULL;
      if (newFormats[formatNumber]) {
          new_format = newFormats[formatNumber]->clone();
          if (!new_format) {
              status = U_MEMORY_ALLOCATION_ERROR;
          }
      }
      setCustomArgStartFormat(partIndex, new_format, status);
      ++formatNumber;
    }
    if (U_FAILURE(status)) {
        resetPattern();
    }
}

// -------------------------------------
// Adopt a single format by format number.
// Do nothing if the format number is not less than the array count.

void
MessageFormat::adoptFormat(int32_t n, Format *newFormat) {
    if (n < 0 || msgPattern.hasNamedArguments()) {
        delete newFormat;
    } else {
        int32_t formatNumber = 0;
        UErrorCode status = U_ZERO_ERROR;
        for (int32_t partIndex = 0;
             U_SUCCESS(status) && (partIndex = nextTopLevelArgStart(partIndex)) >= 0;) {
            if (n == formatNumber) {
                setCustomArgStartFormat(partIndex, newFormat, status);
                return;
            }
            ++formatNumber;
        }
        delete newFormat;
    }
}

// -------------------------------------
// Adopt a single format by format name.
// Do nothing if there is no match of formatName.
void
MessageFormat::adoptFormat(const UnicodeString& formatName,
                           Format* formatToAdopt,
                           UErrorCode& status) {
    if (U_FAILURE(status)) {
        delete formatToAdopt;
        return;
    }
    int32_t argNumber = MessagePattern::validateArgumentName(formatName);
    if (argNumber < UMSGPAT_ARG_NAME_NOT_NUMBER) {
        delete formatToAdopt;
        return;
    }
    bool alreadyPut = false;
    for (int32_t partIndex = 0; (partIndex = nextTopLevelArgStart(partIndex)) >= 0;) {
        if (argNameMatches(partIndex + 1, formatName, argNumber)) {
            Format* f;
            if (alreadyPut) {
                f = formatToAdopt->clone();
                if (f == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    return;
                }
            } else {
                f = formatToAdopt;
            }
            setCustomArgStartFormat(partIndex, f, status);
            alreadyPut = true;
        }
    }
    if (!alreadyPut) {
        delete formatToAdopt;
    }
}

// -------------------------------------
// Set a single format.
// Do nothing if the variable is not less than the array count.
void
MessageFormat::setFormat(int32_t n, const Format& newFormat) {

    if (n > 0 && !msgPattern.hasNamedArguments()) {
        int32_t formatNumber = 0;
        for (int32_t partIndex = 0;
             (partIndex = nextTopLevelArgStart(partIndex)) >= 0;) {
            UErrorCode status = U_ZERO_ERROR;
            if (n == formatNumber) {
                Format* new_format = newFormat.clone();
                if (new_format) {
                    setCustomArgStartFormat(partIndex, new_format, status);
                }
                return;
            }
            ++formatNumber;
        }
    }
}

// -------------------------------------
// Get a single format by format name.
// Do nothing if the variable is not less than the array count.
Format *
MessageFormat::getFormat(const UnicodeString& formatName, UErrorCode& status) {
    if (U_FAILURE(status) || cachedFormatters == NULL) return NULL;

    int32_t argNumber = MessagePattern::validateArgumentName(formatName);
    if (argNumber < UMSGPAT_ARG_NAME_NOT_NUMBER) {
        return NULL;
    }
    for (int32_t partIndex = 0; (partIndex = nextTopLevelArgStart(partIndex)) >= 0;) {
        if (argNameMatches(partIndex + 1, formatName, argNumber)) {
            return getCachedFormatter(partIndex);
        }
    }
    return NULL;
}

// -------------------------------------
// Set a single format by format name
// Do nothing if the variable is not less than the array count.
void
MessageFormat::setFormat(const UnicodeString& formatName,
                         const Format& newFormat,
                         UErrorCode& status) {
    if (U_FAILURE(status)) return;

    int32_t argNumber = MessagePattern::validateArgumentName(formatName);
    if (argNumber < UMSGPAT_ARG_NAME_NOT_NUMBER) {
        return;
    }
    for (int32_t partIndex = 0; (partIndex = nextTopLevelArgStart(partIndex)) >= 0;) {
        if (argNameMatches(partIndex + 1, formatName, argNumber)) {
            if (&newFormat == NULL) {
                setCustomArgStartFormat(partIndex, NULL, status);
            } else {
                Format* new_format = newFormat.clone();
                if (new_format == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    return;
                }
                setCustomArgStartFormat(partIndex, new_format, status);
            }
        }
    }
}

// -------------------------------------
// Gets the format array.
const Format**
MessageFormat::getFormats(int32_t& cnt) const
{
    // This old API returns an array (which we hold) of Format*
    // pointers.  The array is valid up to the next call to any
    // method on this object.  We construct and resize an array
    // on demand that contains aliases to the subformats[i].format
    // pointers.
    MessageFormat* t = const_cast<MessageFormat*> (this);
    cnt = 0;
    if (formatAliases == NULL) {
        t->formatAliasesCapacity = (argTypeCount<10) ? 10 : argTypeCount;
        Format** a = (Format**)
            uprv_malloc(sizeof(Format*) * formatAliasesCapacity);
        if (a == NULL) {
            return NULL;
        }
        t->formatAliases = a;
    } else if (argTypeCount > formatAliasesCapacity) {
        Format** a = (Format**)
            uprv_realloc(formatAliases, sizeof(Format*) * argTypeCount);
        if (a == NULL) {
            return NULL;
        }
        t->formatAliases = a;
        t->formatAliasesCapacity = argTypeCount;
    }

    for (int32_t partIndex = 0; (partIndex = nextTopLevelArgStart(partIndex)) >= 0;) {
        t->formatAliases[cnt++] = getCachedFormatter(partIndex);
    }

    return (const Format**)formatAliases;
}


UnicodeString MessageFormat::getArgName(int32_t partIndex) {
    const MessagePattern::Part& part = msgPattern.getPart(partIndex);
    if (part.getType() == UMSGPAT_PART_TYPE_ARG_NAME) {
        return msgPattern.getSubstring(part);
    } else {
        UnicodeString temp;
        return itos(part.getValue(), temp);
    }
}

StringEnumeration*
MessageFormat::getFormatNames(UErrorCode& status) {
    if (U_FAILURE(status))  return NULL;

    UVector *fFormatNames = new UVector(status);
    if (U_FAILURE(status)) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }

    for (int32_t partIndex = 0; (partIndex = nextTopLevelArgStart(partIndex)) >= 0;) {
        fFormatNames->addElement(new UnicodeString(getArgName(partIndex)), status);
    }

    StringEnumeration* nameEnumerator = new FormatNameEnumeration(fFormatNames, status);
    return nameEnumerator;
}

// -------------------------------------
// Formats the source Formattable array and copy into the result buffer.
// Ignore the FieldPosition result for error checking.

UnicodeString&
MessageFormat::format(const Formattable* source,
                      int32_t cnt,
                      UnicodeString& appendTo,
                      FieldPosition& ignore,
                      UErrorCode& success) const
{
    if (U_FAILURE(success))
        return appendTo;

    return format(source, cnt, appendTo, ignore, 0, success);
}

// -------------------------------------
// Internally creates a MessageFormat instance based on the
// pattern and formats the arguments Formattable array and
// copy into the appendTo buffer.

UnicodeString&
MessageFormat::format(  const UnicodeString& pattern,
                        const Formattable* arguments,
                        int32_t cnt,
                        UnicodeString& appendTo,
                        UErrorCode& success)
{
    MessageFormat temp(pattern, success);
    FieldPosition ignore(0);
    temp.format(arguments, cnt, appendTo, ignore, success);
    return appendTo;
}

// -------------------------------------
// Formats the source Formattable object and copy into the
// appendTo buffer.  The Formattable object must be an array
// of Formattable instances, returns error otherwise.

UnicodeString&
MessageFormat::format(const Formattable& source,
                      UnicodeString& appendTo,
                      FieldPosition& ignore,
                      UErrorCode& success) const
{
    int32_t cnt;

    if (U_FAILURE(success))
        return appendTo;
    if (source.getType() != Formattable::kArray) {
        success = U_ILLEGAL_ARGUMENT_ERROR;
        return appendTo;
    }
    const Formattable* tmpPtr = source.getArray(cnt);

    return format(tmpPtr, cnt, appendTo, ignore, 0, success);
}

UnicodeString&
MessageFormat::format(const UnicodeString* argumentNames,
                      const Formattable* arguments,
                      int32_t count,
                      UnicodeString& appendTo,
                      UErrorCode& success) const {
    FieldPosition ignore(0);
    return format(arguments, argumentNames, count, appendTo, ignore, 0, success);
}

UnicodeString&
MessageFormat::format(const Formattable* arguments,
                      int32_t cnt,
                      UnicodeString& appendTo,
                      FieldPosition& pos,
                      int32_t recursionProtection,
                      UErrorCode& success) const
{
    return format(arguments, NULL, cnt, appendTo, pos, recursionProtection, success);
}

// -------------------------------------
// Formats the arguments Formattable array and copy into the appendTo buffer.
// Ignore the FieldPosition result for error checking.

UnicodeString&
MessageFormat::format(const Formattable* arguments,
                      const UnicodeString *argumentNames,
                      int32_t cnt,
                      UnicodeString& appendTo,
                      FieldPosition& pos,
                      int32_t /*recursionProtection*/,
                      UErrorCode& success) const {
    UnicodeStringAppendable usapp(appendTo);
    AppendableWrapper app(usapp);
    format(0, 0.0, arguments, argumentNames, cnt, app, &pos, success);
    return appendTo;
}

// Does linear search to find the match for an ArgName.
const Formattable* MessageFormat::getArgFromListByName(const Formattable* arguments,
                                                       const UnicodeString *argumentNames,
                                                       int32_t cnt, UnicodeString& name) const {
    for (int32_t i = 0; i < cnt; ++i) {
        if (0 == argumentNames[i].compare(name)) {
            return arguments + i;
        }
    }
    return NULL;
}


// if argumentNames is NULL, this means arguments is a numeric array.
// arguments can not be NULL.
void MessageFormat::format(int32_t msgStart, double pluralNumber,
                           const Formattable* arguments,
                           const UnicodeString *argumentNames,
                           int32_t cnt,
                           AppendableWrapper& appendTo,
                           FieldPosition* ignore,
                           UErrorCode& success) const {
    if (U_FAILURE(success)) {
        return;
    }

    const UnicodeString& msgString = msgPattern.getPatternString();
    int32_t prevIndex = msgPattern.getPart(msgStart).getLimit();
    for (int32_t i = msgStart + 1; U_SUCCESS(success) ; ++i) {
        const MessagePattern::Part* part = &msgPattern.getPart(i);
        const UMessagePatternPartType type = part->getType();
        int32_t index = part->getIndex();
        appendTo.append(msgString, prevIndex, index - prevIndex);
        if (type == UMSGPAT_PART_TYPE_MSG_LIMIT) {
            return;
        }
        prevIndex = part->getLimit();
        if (type == UMSGPAT_PART_TYPE_REPLACE_NUMBER) {
            const NumberFormat* nf = getDefaultNumberFormat(success);
            appendTo.formatAndAppend(nf, Formattable(pluralNumber), success);
            continue;
        }
        if (type != UMSGPAT_PART_TYPE_ARG_START) {
            continue;
        }
        int32_t argLimit = msgPattern.getLimitPartIndex(i);
        UMessagePatternArgType argType = part->getArgType();
        part = &msgPattern.getPart(++i);
        const Formattable* arg;
        UnicodeString noArg;
        if (argumentNames == NULL) {
            int32_t argNumber = part->getValue();  // ARG_NUMBER
            if (0 <= argNumber && argNumber < cnt) {
                arg = arguments + argNumber;
            } else {
                arg = NULL;
                noArg.append(LEFT_CURLY_BRACE);
                itos(argNumber, noArg);
                noArg.append(RIGHT_CURLY_BRACE);
            }
        } else {
            UnicodeString key;
            if (part->getType() == UMSGPAT_PART_TYPE_ARG_NAME) {
                key = msgPattern.getSubstring(*part);
            } else /* UMSGPAT_PART_TYPE_ARG_NUMBER */ {
                itos(part->getValue(), key);
            }
            arg = getArgFromListByName(arguments, argumentNames, cnt, key);
            if (arg == NULL) {
                noArg.append(LEFT_CURLY_BRACE);
                noArg.append(key);
                noArg.append(RIGHT_CURLY_BRACE);
            }
        }
        ++i;
        int32_t prevDestLength = appendTo.length();
        const Format* formatter = NULL;
        if (!noArg.isEmpty()) {
            appendTo.append(noArg);
        } else if (arg == NULL) {
            appendTo.append(NULL_STRING, 4);
        } else if ((formatter = getCachedFormatter(i -2))) {
            // Handles all ArgType.SIMPLE, and formatters from setFormat() and its siblings.
            if (dynamic_cast<const ChoiceFormat*>(formatter) ||
                dynamic_cast<const PluralFormat*>(formatter) ||
                dynamic_cast<const SelectFormat*>(formatter)) {
                // We only handle nested formats here if they were provided via
                // setFormat() or its siblings. Otherwise they are not cached and instead
                // handled below according to argType.
                UnicodeString subMsgString;
                formatter->format(const_cast<Formattable*>(arg), subMsgString,success);
                if (subMsgString.indexOf(LEFT_CURLY_BRACE) >= 0 ||
                  (subMsgString.indexOf(SINGLE_QUOTE) >= 0 && !MessageImpl::jdkAposMode(msgPattern))) {
                    MessageFormat subMsgFormat(subMsgString, fLocale, success);
                    subMsgFormat.format(0, 0, arguments, argumentNames, cnt, appendTo, ignore, success);
                } else {
                    appendTo.append(subMsgString);
                }
            } else {
                appendTo.formatAndAppend(formatter, *arg, success);
            }
        } else if (argType == UMSGPAT_ARG_TYPE_NONE || (cachedFormatters && uhash_iget(cachedFormatters, i - 2))) {
            // We arrive here if getCachedFormatter returned NULL, but there was actually an element in the hash table.
            // This can only happen if the hash table contained a DummyFormat, so the if statement above is a check
            // for the hash table containind DummyFormat.
            if (arg->isNumeric()) {
                const NumberFormat* nf = getDefaultNumberFormat(success);
                appendTo.formatAndAppend(nf, *arg, success);
            } else if (arg->getType() == Formattable::kDate) {
                const DateFormat* df = getDefaultDateFormat(success);
                appendTo.formatAndAppend(df, Formattable(arg->getDate()), success);
            } else {
                appendTo.append(const_cast<Formattable*>(arg)->getString());
            }
        } else if (argType == UMSGPAT_ARG_TYPE_CHOICE) {
            if (!arg->isNumeric()) {
                success = U_ILLEGAL_ARGUMENT_ERROR;
                return;
            }
            const double number = arg->getDouble();
            int32_t subMsgStart = ChoiceFormat::findSubMessage(msgPattern, i, number);
            formatComplexSubMessage(subMsgStart, 0, arguments, argumentNames,
                                    cnt, appendTo, success);
        } else if (argType == UMSGPAT_ARG_TYPE_PLURAL) {
            if (!arg->isNumeric()) {
                success = U_ILLEGAL_ARGUMENT_ERROR;
                return;
            }
            double number = arg->getDouble();
            int32_t subMsgStart = PluralFormat::findSubMessage(msgPattern, i, pluralProvider, number,
                                                               success);
            double offset = msgPattern.getPluralOffset(subMsgStart);
            formatComplexSubMessage(subMsgStart, number-offset, arguments, argumentNames,
                                    cnt, appendTo, success);
        } else if (argType == UMSGPAT_ARG_TYPE_SELECT) {
            int32_t subMsgStart = SelectFormat::findSubMessage(msgPattern, i, arg->getString(), success);
            formatComplexSubMessage(subMsgStart, 0, arguments, argumentNames,
                                    cnt, appendTo, success);
        } else {
            // This should never happen.
            success = U_INTERNAL_PROGRAM_ERROR;
            return;
        }
        ignore = updateMetaData(appendTo, prevDestLength, ignore, arg);
        prevIndex = msgPattern.getPart(argLimit).getLimit();
        i = argLimit;
    }
}


void MessageFormat::formatComplexSubMessage(int32_t msgStart,
                                            double pluralNumber,
                                            const Formattable* arguments,
                                            const UnicodeString *argumentNames,
                                            int32_t cnt,
                                            AppendableWrapper& appendTo,
                                            UErrorCode& success) const {
    if (U_FAILURE(success)) {
        return;
    }

    if (!MessageImpl::jdkAposMode(msgPattern)) {
        format(msgStart, pluralNumber, arguments, argumentNames, cnt, appendTo, NULL, success);
        return;
    }

    // JDK compatibility mode: (see JDK MessageFormat.format() API docs)
    // - remove SKIP_SYNTAX; that is, remove half of the apostrophes
    // - if the result string contains an open curly brace '{' then
    //   instantiate a temporary MessageFormat object and format again;
    //   otherwise just append the result string
    const UnicodeString& msgString = msgPattern.getPatternString();
    UnicodeString sb;
    int32_t prevIndex = msgPattern.getPart(msgStart).getLimit();
    for (int32_t i = msgStart;;) {
        const MessagePattern::Part& part = msgPattern.getPart(++i);
        const UMessagePatternPartType type = part.getType();
        int32_t index = part.getIndex();
        if (type == UMSGPAT_PART_TYPE_MSG_LIMIT) {
            sb.append(msgString, prevIndex, index - prevIndex);
            break;
        } else if (type == UMSGPAT_PART_TYPE_REPLACE_NUMBER || type == UMSGPAT_PART_TYPE_SKIP_SYNTAX) {
            sb.append(msgString, prevIndex, index - prevIndex);
            if (type == UMSGPAT_PART_TYPE_REPLACE_NUMBER) {
                const NumberFormat* nf = getDefaultNumberFormat(success);
                sb.append(nf->format(pluralNumber, sb, success));
            }
            prevIndex = part.getLimit();
        } else if (type == UMSGPAT_PART_TYPE_ARG_START) {
            sb.append(msgString, prevIndex, index - prevIndex);
            prevIndex = index;
            i = msgPattern.getLimitPartIndex(i);
            index = msgPattern.getPart(i).getLimit();
            MessageImpl::appendReducedApostrophes(msgString, prevIndex, index, sb);
            prevIndex = index;
        }
    }
    if (sb.indexOf(LEFT_CURLY_BRACE) >= 0) {
        MessageFormat subMsgFormat(UnicodeString(), fLocale, success);
        subMsgFormat.applyPattern(sb, UMSGPAT_APOS_DOUBLE_REQUIRED, NULL, success);
        subMsgFormat.format(0, 0, arguments, argumentNames, cnt, appendTo, NULL, success);
    } else {
        appendTo.append(sb);
    }
}


UnicodeString MessageFormat::getLiteralStringUntilNextArgument(int32_t from) const {
    const UnicodeString& msgString=msgPattern.getPatternString();
    int32_t prevIndex=msgPattern.getPart(from).getLimit();
    UnicodeString b;
    for (int32_t i = from + 1; ; ++i) {
        const MessagePattern::Part& part = msgPattern.getPart(i);
        const UMessagePatternPartType type=part.getType();
        int32_t index=part.getIndex();
        b.append(msgString, prevIndex, index - prevIndex);
        if(type==UMSGPAT_PART_TYPE_ARG_START || type==UMSGPAT_PART_TYPE_MSG_LIMIT) {
            return b;
        }
        // Unexpected Part "part" in parsed message.
        U_ASSERT(type==UMSGPAT_PART_TYPE_SKIP_SYNTAX || type==UMSGPAT_PART_TYPE_INSERT_CHAR);
        prevIndex=part.getLimit();
    }
}


FieldPosition* MessageFormat::updateMetaData(AppendableWrapper& /*dest*/, int32_t /*prevLength*/,
                             FieldPosition* /*fp*/, const Formattable* /*argId*/) const {
    // Unlike in Java, there are no field attributes defined for MessageFormat. Do nothing.
    return NULL;
    /*
      if (fp != NULL && Field.ARGUMENT.equals(fp.getFieldAttribute())) {
          fp->setBeginIndex(prevLength);
          fp->setEndIndex(dest.get_length());
          return NULL;
      }
      return fp;
    */
}

void MessageFormat::CopyHashTables(const MessageFormat& that, UErrorCode& ec) {
    // Deep copy the Formats.
    int32_t pos = -1;
    int32_t idx = 0;
    if (U_FAILURE(ec)) {
        return;
    }
    if (cachedFormatters != NULL) {
        uhash_removeAll(cachedFormatters);
    }
    if (customFormatArgStarts != NULL) {
        uhash_removeAll(customFormatArgStarts);
    }
    if (that.cachedFormatters) {
        if (cachedFormatters == NULL) {
            cachedFormatters=uhash_open(uhash_hashLong, uhash_compareLong,
                                        equalFormatsForHash, &ec);
            if (U_FAILURE(ec)) {
                return;
            }
            uhash_setValueDeleter(cachedFormatters, uhash_deleteUObject);
        }

        const int32_t count = uhash_count(that.cachedFormatters);
        for (; idx < count && U_SUCCESS(ec); ++idx) {
            const UHashElement* cur = uhash_nextElement(that.cachedFormatters, &pos);
            Format* new_format = ((Format*)(cur->value.pointer))->clone();
            if (new_format) {
                uhash_iput(cachedFormatters, cur->key.integer, new_format, &ec);
            } else {
                ec = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
        }
    }
    if (that.customFormatArgStarts) {
        if (customFormatArgStarts == NULL) {
            customFormatArgStarts=uhash_open(uhash_hashLong, uhash_compareLong,
                                              NULL, &ec);
        }
        const int32_t count = uhash_count(that.customFormatArgStarts);
        for (idx = 0, pos = -1; idx < count && U_SUCCESS(ec); ++idx) {
            const UHashElement* cur = uhash_nextElement(that.customFormatArgStarts, &pos);
            uhash_iputi(customFormatArgStarts, cur->key.integer, cur->value.integer, &ec);
        }
    }
}


Formattable*
MessageFormat::parse(int32_t msgStart,
                     const UnicodeString& source,
                     ParsePosition& pos,
                     int32_t& count,
                     UErrorCode& ec) const {
    count = 0;
    if (U_FAILURE(ec)) {
        pos.setErrorIndex(pos.getIndex());
        return NULL;
    }
    // parse() does not work with named arguments.
    if (msgPattern.hasNamedArguments()) {
        ec = U_ARGUMENT_TYPE_MISMATCH;
        pos.setErrorIndex(pos.getIndex());
        return NULL;
    }
    LocalArray<Formattable> resultArray(new Formattable[argTypeCount ? argTypeCount : 1]);
    const UnicodeString& msgString=msgPattern.getPatternString();
    int32_t prevIndex=msgPattern.getPart(msgStart).getLimit();
    int32_t sourceOffset = pos.getIndex();
    ParsePosition tempStatus(0);

    int32_t argIndex = 0;
    for(int32_t i=msgStart+1; ; ++i) {
        const MessagePattern::Part* part=&msgPattern.getPart(i);
        const UMessagePatternPartType type=part->getType();
        int32_t index=part->getIndex();
        // Make sure the literal string matches.
        int32_t len = index - prevIndex;
        if (len == 0 || (0 == msgString.compare(prevIndex, len, source, sourceOffset, len))) {
            sourceOffset += len;
            prevIndex += len;
        } else {
            pos.setErrorIndex(sourceOffset);
            return NULL; // leave index as is to signal error
        }
        if(type==UMSGPAT_PART_TYPE_MSG_LIMIT) {
            // Things went well! Done.
            pos.setIndex(sourceOffset);
            count = argIndex;
            return resultArray.orphan();
        }
        if(type==UMSGPAT_PART_TYPE_SKIP_SYNTAX || type==UMSGPAT_PART_TYPE_INSERT_CHAR) {
            prevIndex=part->getLimit();
            continue;
        }
        // We do not support parsing Plural formats. (No REPLACE_NUMBER here.)
        // Unexpected Part "part" in parsed message.
        U_ASSERT(type==UMSGPAT_PART_TYPE_ARG_START);
        int32_t argLimit=msgPattern.getLimitPartIndex(i);

        UMessagePatternArgType argType=part->getArgType();
        part=&msgPattern.getPart(++i);
        int32_t argNumber = part->getValue();  // ARG_NUMBER
        UnicodeString key;
        ++i;
        const Format* formatter = NULL;
        Formattable& argResult = resultArray[argIndex];

        if(cachedFormatters!=NULL && (formatter = getCachedFormatter(i - 2))!=NULL) {
            // Just parse using the formatter.
            tempStatus.setIndex(sourceOffset);
            formatter->parseObject(source, argResult, tempStatus);
            if (tempStatus.getIndex() == sourceOffset) {
                pos.setErrorIndex(sourceOffset);
                return NULL; // leave index as is to signal error
            }
            sourceOffset = tempStatus.getIndex();
        } else if(
            argType==UMSGPAT_ARG_TYPE_NONE || (cachedFormatters && uhash_iget(cachedFormatters, i -2))) {
              // We arrive here if getCachedFormatter returned NULL, but there was actually an element in the hash table.
              // This can only happen if the hash table contained a DummyFormat, so the if statement above is a check
              // for the hash table containind DummyFormat.

              // Match as a string.
              // if at end, use longest possible match
              // otherwise uses first match to intervening string
              // does NOT recursively try all possibilities
              const UnicodeString& stringAfterArgument = getLiteralStringUntilNextArgument(argLimit);
              int32_t next;
              if (!stringAfterArgument.isEmpty()) {
                  next = source.indexOf(stringAfterArgument, sourceOffset);
              } else {
                  next = source.length();
              }
              if (next < 0) {
                  pos.setErrorIndex(sourceOffset);
                  return NULL; // leave index as is to signal error
              } else {
                  UnicodeString strValue(source.tempSubString(sourceOffset, next - sourceOffset));
                  UnicodeString compValue;
                  compValue.append(LEFT_CURLY_BRACE);
                  itos(argNumber, compValue);
                  compValue.append(RIGHT_CURLY_BRACE);
                  if (0 != strValue.compare(compValue)) {
                      argResult.setString(strValue);
                  }
                  sourceOffset = next;
              }
          } else if(argType==UMSGPAT_ARG_TYPE_CHOICE) {
              tempStatus.setIndex(sourceOffset);
              double choiceResult = ChoiceFormat::parseArgument(msgPattern, i, source, tempStatus);
              if (tempStatus.getIndex() == sourceOffset) {
                  pos.setErrorIndex(sourceOffset);
                  return NULL; // leave index as is to signal error
              }
              argResult.setDouble(choiceResult);
              sourceOffset = tempStatus.getIndex();
          } else if(argType==UMSGPAT_ARG_TYPE_PLURAL || argType==UMSGPAT_ARG_TYPE_SELECT) {
              // No can do!
              ec = U_UNSUPPORTED_ERROR;
              return NULL;
          } else {
              // This should never happen.
              ec = U_INTERNAL_PROGRAM_ERROR;
              return NULL;
          }
          ++argIndex;
          prevIndex=msgPattern.getPart(argLimit).getLimit();
          i=argLimit;
    }
}
// -------------------------------------
// Parses the source pattern and returns the Formattable objects array,
// the array count and the ending parse position.  The caller of this method
// owns the array.

Formattable*
MessageFormat::parse(const UnicodeString& source,
                     ParsePosition& pos,
                     int32_t& count) const {
    UErrorCode ec;
    return parse(0, source, pos, count, ec);
}

// -------------------------------------
// Parses the source string and returns the array of
// Formattable objects and the array count.  The caller
// owns the returned array.

Formattable*
MessageFormat::parse(const UnicodeString& source,
                     int32_t& cnt,
                     UErrorCode& success) const
{
    if (!isArgNumeric ) {
        success = U_ARGUMENT_TYPE_MISMATCH;
        return NULL;
    }
    ParsePosition status(0);
    // Calls the actual implementation method and starts
    // from zero offset of the source text.
    Formattable* result = parse(source, status, cnt);
    if (status.getIndex() == 0) {
        success = U_MESSAGE_PARSE_ERROR;
        delete[] result;
        return NULL;
    }
    return result;
}

// -------------------------------------
// Parses the source text and copy into the result buffer.

void
MessageFormat::parseObject( const UnicodeString& source,
                            Formattable& result,
                            ParsePosition& status) const
{
    int32_t cnt = 0;
    Formattable* tmpResult = parse(source, status, cnt);
    if (tmpResult != NULL)
        result.adoptArray(tmpResult, cnt);
}

UnicodeString
MessageFormat::autoQuoteApostrophe(const UnicodeString& pattern, UErrorCode& status) {
    UnicodeString result;
    if (U_SUCCESS(status)) {
        int32_t plen = pattern.length();
        const UChar* pat = pattern.getBuffer();
        int32_t blen = plen * 2 + 1; // space for null termination, convenience
        UChar* buf = result.getBuffer(blen);
        if (buf == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        } else {
            int32_t len = umsg_autoQuoteApostrophe(pat, plen, buf, blen, &status);
            result.releaseBuffer(U_SUCCESS(status) ? len : 0);
        }
    }
    if (U_FAILURE(status)) {
        result.setToBogus();
    }
    return result;
}

// -------------------------------------

static Format* makeRBNF(URBNFRuleSetTag tag, const Locale& locale, const UnicodeString& defaultRuleSet, UErrorCode& ec) {
    RuleBasedNumberFormat* fmt = new RuleBasedNumberFormat(tag, locale, ec);
    if (fmt == NULL) {
        ec = U_MEMORY_ALLOCATION_ERROR;
    } else if (U_SUCCESS(ec) && defaultRuleSet.length() > 0) {
        UErrorCode localStatus = U_ZERO_ERROR; // ignore unrecognized default rule set
        fmt->setDefaultRuleSet(defaultRuleSet, localStatus);
    }
    return fmt;
}

void MessageFormat::cacheExplicitFormats(UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }

    if (cachedFormatters != NULL) {
        uhash_removeAll(cachedFormatters);
    }
    if (customFormatArgStarts != NULL) {
        uhash_removeAll(customFormatArgStarts);
    }
    argTypeCount = 0;
    int32_t limit = msgPattern.countParts() - 1;
    for (int32_t i = 1; i < limit; ++i) {
        const MessagePattern::Part* part = &msgPattern.getPart(i);
        if (part->getType() != UMSGPAT_PART_TYPE_ARG_START) {
            continue;
        }
        int32_t argNumber = -1;
        if (msgPattern.getPart(i + 1).getType() == UMSGPAT_PART_TYPE_ARG_NUMBER) {
            argNumber = msgPattern.getPart(i + 1).getValue();
            if (argNumber + 1 > argTypeCount) {
                argTypeCount = argNumber + 1;
            }
        }
        Formattable::Type formattableType;

        UMessagePatternArgType argType = part->getArgType();
        if (argType == UMSGPAT_ARG_TYPE_SIMPLE){
            int32_t index = i;
            i += 2;
            UnicodeString explicitType = msgPattern.getSubstring(msgPattern.getPart(i++));
            UnicodeString style;
            if ((part = &msgPattern.getPart(i))->getType() == UMSGPAT_PART_TYPE_ARG_STYLE) {
                style = msgPattern.getSubstring(*part);
                ++i;
            }
            UParseError parseError;
            Format* formatter = createAppropriateFormat(explicitType, style, formattableType, parseError, status);
            setArgStartFormat(index, formatter, status);
        } else {
            if (argType == UMSGPAT_ARG_TYPE_CHOICE || argType == UMSGPAT_ARG_TYPE_PLURAL) {
                formattableType = Formattable::kDouble;
            } else if (argType == UMSGPAT_ARG_TYPE_SELECT) {
                formattableType = Formattable::kString;
            }
        }
        if (argNumber != -1) {
            argTypes[argNumber] = formattableType;
        }
    }
}


Format* MessageFormat::createAppropriateFormat(UnicodeString& type, UnicodeString& style,
                                               Formattable::Type& formattableType, UParseError& parseError,
                                               UErrorCode& ec) {
    if (U_FAILURE(ec)) {
        return NULL;
    }
    Format* fmt = NULL;
    int32_t typeID, styleID;
    DateFormat::EStyle date_style;

    switch (typeID = findKeyword(type, TYPE_IDS)) {
        case 0: // number
            formattableType = Formattable::kDouble;
            switch (findKeyword(style, NUMBER_STYLE_IDS)) {
            case 0: // default
                fmt = NumberFormat::createInstance(fLocale, ec);
                break;
            case 1: // currency
                fmt = NumberFormat::createCurrencyInstance(fLocale, ec);
                break;
            case 2: // percent
                fmt = NumberFormat::createPercentInstance(fLocale, ec);
                break;
            case 3: // integer
                formattableType = Formattable::kLong;
                fmt = createIntegerFormat(fLocale, ec);
                break;
            default: // pattern
                fmt = NumberFormat::createInstance(fLocale, ec);
                if (fmt) {
                    DecimalFormat* decfmt = dynamic_cast<DecimalFormat*>(fmt);
                    if (decfmt != NULL) {
                        decfmt->applyPattern(style,parseError,ec);
                    }
                }
                break;
            }
            break;

        case 1: // date
        case 2: // time
            formattableType = Formattable::kDate;
            styleID = findKeyword(style, DATE_STYLE_IDS);
            date_style = (styleID >= 0) ? DATE_STYLES[styleID] : DateFormat::kDefault;

            if (typeID == 2) {
                fmt = DateFormat::createDateInstance(date_style, fLocale);
            } else {
                fmt = DateFormat::createTimeInstance(date_style, fLocale);
            }

            if (styleID < 0 && fmt != NULL) {
                SimpleDateFormat* sdtfmt = dynamic_cast<SimpleDateFormat*>(fmt);
                if (sdtfmt != NULL) {
                    sdtfmt->applyPattern(style);
                }
            }
            break;

        case 3: // spellout
            formattableType = Formattable::kDouble;
            fmt = makeRBNF(URBNF_SPELLOUT, fLocale, style, ec);
            break;
        case 4: // ordinal
            formattableType = Formattable::kDouble;
            fmt = makeRBNF(URBNF_ORDINAL, fLocale, style, ec);
            break;
        case 5: // duration
            formattableType = Formattable::kDouble;
            fmt = makeRBNF(URBNF_DURATION, fLocale, style, ec);
            break;
        default:
            formattableType = Formattable::kString;
            ec = U_ILLEGAL_ARGUMENT_ERROR;
            break;
    }

    return fmt;
}


// -------------------------------------
// Finds the string, s, in the string array, list.
int32_t MessageFormat::findKeyword(const UnicodeString& s,
                                   const UChar * const *list)
{
    if (s.length() == 0)
        return 0; // default

    UnicodeString buffer = s;
    // Trims the space characters and turns all characters
    // in s to lower case.
    buffer.trim().toLower("");
    for (int32_t i = 0; list[i]; ++i) {
        if (!buffer.compare(list[i], u_strlen(list[i]))) {
            return i;
        }
    }
    return -1;
}

// -------------------------------------
// Checks the range of the source text to quote the special
// characters, { and ' and copy to target buffer.

void
MessageFormat::copyAndFixQuotes(const UnicodeString& source,
                                int32_t start,
                                int32_t end,
                                UnicodeString& appendTo)
{
    UBool gotLB = FALSE;

    for (int32_t i = start; i < end; ++i) {
        UChar ch = source[i];
        if (ch == LEFT_CURLY_BRACE) {
            appendTo += SINGLE_QUOTE;
            appendTo += LEFT_CURLY_BRACE;
            appendTo += SINGLE_QUOTE;
            gotLB = TRUE;
        }
        else if (ch == RIGHT_CURLY_BRACE) {
            if(gotLB) {
                appendTo += RIGHT_CURLY_BRACE;
                gotLB = FALSE;
            }
            else {
                // orig code.
                appendTo += SINGLE_QUOTE;
                appendTo += RIGHT_CURLY_BRACE;
                appendTo += SINGLE_QUOTE;
            }
        }
        else if (ch == SINGLE_QUOTE) {
            appendTo += SINGLE_QUOTE;
            appendTo += SINGLE_QUOTE;
        }
        else {
            appendTo += ch;
        }
    }
}

/**
 * Convenience method that ought to be in NumberFormat
 */
NumberFormat*
MessageFormat::createIntegerFormat(const Locale& locale, UErrorCode& status) const {
    NumberFormat *temp = NumberFormat::createInstance(locale, status);
    DecimalFormat *temp2;
    if (temp != NULL && (temp2 = dynamic_cast<DecimalFormat*>(temp)) != NULL) {
        temp2->setMaximumFractionDigits(0);
        temp2->setDecimalSeparatorAlwaysShown(FALSE);
        temp2->setParseIntegerOnly(TRUE);
    }

    return temp;
}

/**
 * Return the default number format.  Used to format a numeric
 * argument when subformats[i].format is NULL.  Returns NULL
 * on failure.
 *
 * Semantically const but may modify *this.
 */
const NumberFormat* MessageFormat::getDefaultNumberFormat(UErrorCode& ec) const {
    if (defaultNumberFormat == NULL) {
        MessageFormat* t = (MessageFormat*) this;
        t->defaultNumberFormat = NumberFormat::createInstance(fLocale, ec);
        if (U_FAILURE(ec)) {
            delete t->defaultNumberFormat;
            t->defaultNumberFormat = NULL;
        } else if (t->defaultNumberFormat == NULL) {
            ec = U_MEMORY_ALLOCATION_ERROR;
        }
    }
    return defaultNumberFormat;
}

/**
 * Return the default date format.  Used to format a date
 * argument when subformats[i].format is NULL.  Returns NULL
 * on failure.
 *
 * Semantically const but may modify *this.
 */
const DateFormat* MessageFormat::getDefaultDateFormat(UErrorCode& ec) const {
    if (defaultDateFormat == NULL) {
        MessageFormat* t = (MessageFormat*) this;
        t->defaultDateFormat = DateFormat::createDateTimeInstance(DateFormat::kShort, DateFormat::kShort, fLocale);
        if (t->defaultDateFormat == NULL) {
            ec = U_MEMORY_ALLOCATION_ERROR;
        }
    }
    return defaultDateFormat;
}

UBool
MessageFormat::usesNamedArguments() const {
    return !isArgNumeric;
}

UBool
MessageFormat::isLegalArgName(const UnicodeString& argName) const {
    if(!u_hasBinaryProperty(argName.charAt(0), idStart)) {
        return FALSE;
    }
    for (int32_t i=1; i<argName.length(); ++i) {
        if(!u_hasBinaryProperty(argName.charAt(i), idContinue)) {
            return FALSE;
        }
    }
    return TRUE;
}

int32_t
MessageFormat::getArgTypeCount() const {
    return argTypeCount;
}

UBool MessageFormat::equalFormats(const void* left, const void* right) {
    return *(const Format*)left==*(const Format*)right;
}


UBool MessageFormat::DummyFormat::operator==(const Format&) const {
    return true;
}

Format* MessageFormat::DummyFormat::clone() const {
    return new DummyFormat();
}

UnicodeString& MessageFormat::DummyFormat::format(const Formattable&,
                          UnicodeString& appendTo,
                          FieldPosition&,
                          UErrorCode& status) const {
    if (U_SUCCESS(status)) {
        status = U_UNSUPPORTED_ERROR;
    }
    return appendTo;
}

void MessageFormat::DummyFormat::parseObject(const UnicodeString&,
                                                     Formattable&,
                                                     ParsePosition& ) const {
}


FormatNameEnumeration::FormatNameEnumeration(UVector *fNameList, UErrorCode& /*status*/) {
    pos=0;
    fFormatNames = fNameList;
}

const UnicodeString*
FormatNameEnumeration::snext(UErrorCode& status) {
    if (U_SUCCESS(status) && pos < fFormatNames->size()) {
        return (const UnicodeString*)fFormatNames->elementAt(pos++);
    }
    return NULL;
}

void
FormatNameEnumeration::reset(UErrorCode& /*status*/) {
    pos=0;
}

int32_t
FormatNameEnumeration::count(UErrorCode& /*status*/) const {
    return (fFormatNames==NULL) ? 0 : fFormatNames->size();
}

FormatNameEnumeration::~FormatNameEnumeration() {
    UnicodeString *s;
    for (int32_t i=0; i<fFormatNames->size(); ++i) {
        if ((s=(UnicodeString *)fFormatNames->elementAt(i))!=NULL) {
            delete s;
        }
    }
    delete fFormatNames;
}


UnicodeString MessageFormat::PluralSelectorProvider::select(double number, UErrorCode& ec) const {
    if (U_FAILURE(ec)) {
        return UNICODE_STRING_SIMPLE("other");
    }
    MessageFormat::PluralSelectorProvider* t = const_cast<MessageFormat::PluralSelectorProvider*>(this);
    if(rules == NULL) {
        t->rules = PluralRules::forLocale(*locale, ec);
        if (U_FAILURE(ec)) {
            return UNICODE_STRING_SIMPLE("other");
        }
    }
    return rules->select(number);
}

void MessageFormat::PluralSelectorProvider::reset() {
    rules = NULL;
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
