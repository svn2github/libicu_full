/*
******************************************************************************
*
*   Copyright (C) 2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  unisetspan.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2007mar01
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "unicode/uniset.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "uvector.h"
#include "unisetspan.h"

U_NAMESPACE_BEGIN

/*
 * List of next indexes from where to try matching a code point or a string.
 * Indexes are set as deltas from a reference point.
 *
 * Assumption: The maximum delta is limited, and the indexes that are stored
 * at any one time are relatively dense, that is, there are normally no gaps of
 * hundreds or thousands of index values.
 *
 * The implementation uses a circular buffer of bit flags,
 * each indicating whether the corresponding index is in the list.
 * This avoids inserting into a sorted list of actual indexes and
 * physically moving part of the list.
 *
 * Note: If maxLength were guaranteed to be no more than 32 or 64,
 * the list could be stored as bit flags in a single integer.
 * Rather than handling a circular buffer with a start list index,
 * popMinimum() and popMaximum() would shift the integer such that
 * the lowest/highest bit corresponded to the reference text index.
 * UnicodeSet does not have a limit on the lengths of strings.
 */
class IndexList : public UMemory {
public:
    IndexList(int32_t maxLength) : length(0), reference(0), start(0) {
        if(maxLength<=sizeof(staticList)) {
            list=staticList;
            capacity=(int32_t)sizeof(staticList);
        } else {
            capacity=maxLength;
            list=(UBool *)uprv_malloc(capacity);
        }
        uprv_memset(list, 0, capacity);
    }

    ~IndexList() {
        if(list!=staticList) {
            uprv_free(list);
        }
    }

    void clear() {
        uprv_memset(list, 0, capacity);
        reference=start=length=0;
    }

    UBool isEmpty() const {
        return (UBool)(length==0);
    }

    // Set the starting reference point on an empty list,
    // for adding increments 1..maxLength from there.
    // Normally the last span limit.
    void setStart(int32_t index) {
        reference=index;
    }

    // Increment the starting reference point, for adding increments 1..maxLength
    // from there. Equivalent to addIncrement(inc) and popMinimum().
    // If the list is not empty, then there must not be any stored indexes
    // with a lower increment.
    // inc=[1..maxLength]
    void incrementStart(int32_t inc) {
        reference+=inc;
        int32_t i=start+inc;
        if(i>=capacity) {
            i-=capacity;
        }
        if(list[i]) {
            list[i]=FALSE;
            --length;
        }
        start=i;
    }

    // inc=[1..maxLength]
    void addIncrement(int32_t inc) {
        int32_t i=start+inc;
        if(i>=capacity) {
            i-=capacity;
        }
        if(!list[i]) {
            list[i]=TRUE;
            ++length;
        }
    }

    // inc=[1..maxLength]
    UBool containsIncrement(int32_t inc) const {
        int32_t i=start+inc;
        if(i>=capacity) {
            i-=capacity;
        }
        return list[i];
    }

    // Get the lowest stored index from a non-empty list which has been
    // filled with setStart() and addIncrement().
    int32_t popMinimum() {
        // Look for the next index in list[start+1..capacity-1].
        int32_t i=start;
        while(++i<capacity) {
            if(list[i]) {
                list[i]=FALSE;
                --length;
                reference+=(i-start);
                start=i;
                return reference;
            }
        }
        // i==capacity

        // Wrap around and look for the next index in list[0..].
        // Since the list is not empty, there will be one.
        reference+=(capacity-start);
        i=0;
        while(!list[i]) {
            ++i;
        }
        list[i]=FALSE;
        --length;
        start=i;
        return reference+=i;
    }

    // Set the limit reference point on an empty list,
    // for adding decrements 1..maxLength from there.
    // Normally the last spanBack limit.
    void setLimit(int32_t index) {
        reference=index;
    }

    // Decrement the limit reference point, for adding decrements 1..maxLength
    // from there. Equivalent to addDecrement(dec) and popMaximum().
    // If the list is not empty, then there must not be any stored indexes
    // with a lower decrement.
    // dec=[1..maxLength]
    void decrementLimit(int32_t dec) {
        reference-=dec;
        int32_t i=start-dec;
        if(i<0) {
            i+=capacity;
        }
        if(list[i]) {
            list[i]=FALSE;
            --length;
        }
        start=i;
    }

    // dec=[1..maxLength]
    void addDecrement(int32_t dec) {
        int32_t i=start-dec;
        if(i<0) {
            i+=capacity;
        }
        if(!list[i]) {
            list[i]=TRUE;
            ++length;
        }
    }

    // dec=[1..maxLength]
    UBool containsDecrement(int32_t dec) const {
        int32_t i=start-dec;
        if(i<0) {
            i+=capacity;
        }
        return list[i];
    }

    // Get the highest stored index from a non-empty list which has been
    // filled with setLimit() and addDecrement().
    int32_t popMaximum() {
        // Look for the next index in list[0..start-1].
        int32_t i=start;
        while(--i>=0) {
            if(list[i]) {
                list[i]=FALSE;
                --length;
                reference-=(start-i);
                start=i;
                return reference;
            }
        }
        // i==-1

        // Wrap around and look for the next index in list[..capacity-1].
        // Since the list is not empty, there will be one.
        reference-=start+1;
        i=capacity-1;
        while(!list[i]) {
            --i;
        }
        list[i]=FALSE;
        --length;
        start=i;
        return reference+=i;
    }

private:
    UBool *list;
    int32_t capacity;
    int32_t length;
    int32_t reference;
    int32_t start;

    UBool staticList[16];
};

// Get the number of UTF-8 bytes for a UTF-16 (sub)string.
static int32_t
getUTF8Length(const UChar *s, int32_t length) {
    UErrorCode errorCode=U_ZERO_ERROR;
    int32_t length8=0;
    u_strToUTF8(NULL, 0, &length8, s, length, &errorCode);
    if(U_SUCCESS(errorCode) || errorCode==U_BUFFER_OVERFLOW_ERROR) {
        return length8;
    } else {
        // The string contains an unpaired surrogate.
        // Ignore this string.
        return 0;
    }
}

// Append the UTF-8 version of the string to t and return the appended UTF-8 length.
static int32_t
appendUTF8(const UChar *s, int32_t length, uint8_t *t, int32_t capacity) {
    UErrorCode errorCode=U_ZERO_ERROR;
    int32_t length8=0;
    u_strToUTF8((char *)t, capacity, &length8, s, length, &errorCode);
    if(U_SUCCESS(errorCode)) {
        return length8;
    } else {
        // The string contains an unpaired surrogate.
        // Ignore this string.
        return 0;
    }
}

static inline uint8_t
makeSpanLengthByte(int32_t spanLength) {
    // 0xfe==UnicodeSetStringSpan::LONG_SPAN
    return spanLength<0xfe ? (uint8_t)spanLength : (uint8_t)0xfe;
}

// Construct for all variants of span(), or only for any one variant.
// Initialize as little as possible, for single use.
UnicodeSetStringSpan::UnicodeSetStringSpan(const UnicodeSet &set,
                                           const UVector &setStrings,
                                           uint32_t which)
        : spanSet(0, 0x10ffff), pSpanNotSet(NULL), strings(setStrings),
          utf8Lengths(NULL), spanLengths(NULL), utf8(NULL),
          maxLength16(0), maxLength8(0),
          all((UBool)(which==ALL)) {
    spanSet.retainAll(set);
    if(all) {
        spanSet.freeze();
    }
    if(which&NOT_CONTAINED) {
        // Default to the same sets.
        // addToSpanNotSet() will create a separate set if necessary.
        pSpanNotSet=&spanSet;
    }

    // Determine if the strings even need to be taken into account at all for span() etc.
    // Also count the lengths of the UTF-8 versions of the strings for memory allocation.
    int32_t stringsLength=strings.size();

    int32_t utf8Length=0;  // Length of all UTF-8 versions of relevant strings.

    int32_t i, spanLength;
    for(i=0; i<stringsLength; ++i) {
        const UnicodeString &string=*(const UnicodeString *)strings.elementAt(i);
        const UChar *s16=string.getBuffer();
        int32_t length16=string.length();
        spanLength=spanSet.span(s16, length16, USET_SPAN_WHILE_CONTAINED);
        if(spanLength<length16) {  // Relevant string.
            if((which&UTF16) && length16>maxLength16) {
                maxLength16=length16;
            }
            if(which&UTF8) {
                int32_t length8=getUTF8Length(s16, length16);
                utf8Length+=length8;
                if(length8>maxLength8) {
                    maxLength8=length8;
                }
            }
        }
    }
    if((maxLength16|maxLength8)==0) {
        return;
    }

    uint8_t *spanBackLengths;
    uint8_t *spanUTF8Lengths;
    uint8_t *spanBackUTF8Lengths;

    // Allocate a block of meta data.
    int32_t allocSize;
    if(all) {
        // UTF-8 lengths, 4 sets of span lengths, UTF-8 strings.
        allocSize=stringsLength*(4+1+1+1+1)+utf8Length;
    } else {
        allocSize=stringsLength;  // One set of span lengths.
        if(which&UTF8) {
            // UTF-8 lengths and UTF-8 strings.
            allocSize+=stringsLength*4+utf8Length;
        }
    }
    if(allocSize<=sizeof(staticLengths)) {
        utf8Lengths=staticLengths;
    } else {
        utf8Lengths=(int32_t *)uprv_malloc(allocSize);
        if(utf8Lengths==NULL) {
            maxLength16=maxLength8=0;  // Prevent usage by making needsStringSpanUTF16/8() return FALSE.
            return;  // Out of memory.
        }
    }

    if(all) {
        // Store span lengths for all span() variants.
        spanLengths=(uint8_t *)(utf8Lengths+stringsLength);
        spanBackLengths=spanLengths+stringsLength;
        spanUTF8Lengths=spanBackLengths+stringsLength;
        spanBackUTF8Lengths=spanUTF8Lengths+stringsLength;
        utf8=spanBackUTF8Lengths+stringsLength;
    } else {
        // Store span lengths for only one span() variant.
        if(which&UTF8) {
            spanLengths=(uint8_t *)(utf8Lengths+stringsLength);
            utf8=spanLengths+stringsLength;
        } else {
            spanLengths=(uint8_t *)utf8Lengths;
        }
        spanBackLengths=spanUTF8Lengths=spanBackUTF8Lengths=spanLengths;
    }

    // Set the meta data and pSpanNotSet and write the UTF-8 strings.
    int32_t utf8Count=0;  // Count UTF-8 bytes written so far.

    for(i=0; i<stringsLength; ++i) {
        const UnicodeString &string=*(const UnicodeString *)strings.elementAt(i);
        const UChar *s16=string.getBuffer();
        int32_t length16=string.length();
        spanLength=spanSet.span(s16, length16, USET_SPAN_WHILE_CONTAINED);
        if(spanLength<length16) {  // Relevant string.
            if(which&UTF16) {
                if(which&CONTAINED) {
                    if(which&FWD) {
                        spanLengths[i]=makeSpanLengthByte(spanLength);
                    }
                    if(which&BACK) {
                        spanLength=length16-spanSet.spanBack(s16, length16, USET_SPAN_WHILE_CONTAINED);
                        spanBackLengths[i]=makeSpanLengthByte(spanLength);
                    }
                } else /* not CONTAINED, not all, but NOT_CONTAINED */ {
                    spanLengths[i]=spanBackLengths[i]=0;  // Only store a relevant/irrelevant flag.
                }
            }
            if(which&UTF8) {
                uint8_t *s8=utf8+utf8Count;
                int32_t length8=appendUTF8(s16, length16, s8, utf8Length-utf8Count);
                utf8Count+=utf8Lengths[i]=length8;
                if(length8==0) {  // Irrelevant for UTF-8 because not representable in UTF-8.
                    spanUTF8Lengths[i]=spanBackUTF8Lengths[i]=(uint8_t)ALL_CP_CONTAINED;
                } else {  // Relevant for UTF-8.
                    if(which&CONTAINED) {
                        if(which&FWD) {
                            spanLength=spanSet.spanUTF8((const char *)s8, length8, USET_SPAN_WHILE_CONTAINED);
                            spanUTF8Lengths[i]=makeSpanLengthByte(spanLength);
                        }
                        if(which&BACK) {
                            spanLength=length8-spanSet.spanBackUTF8((const char *)s8, length8, USET_SPAN_WHILE_CONTAINED);
                            spanBackUTF8Lengths[i]=makeSpanLengthByte(spanLength);
                        }
                    } else /* not CONTAINED, not all, but NOT_CONTAINED */ {
                        spanUTF8Lengths[i]=spanBackUTF8Lengths[i]=0;  // Only store a relevant/irrelevant flag.
                    }
                }
            }
            if(which&NOT_CONTAINED) {
                // Add string start and end code points to the spanNotSet so that
                // a span(while not contained) stops before any string.
                UChar32 c;
                if(which&FWD) {
                    int32_t len=0;
                    U16_NEXT(s16, len, length16, c);
                    addToSpanNotSet(c);
                }
                if(which&BACK) {
                    int32_t len=length16;
                    U16_PREV(s16, 0, len, c);
                    addToSpanNotSet(c);
                }
            }
        } else {  // Irrelevant string.
            if(all) {
                utf8Lengths[i]=0;
                spanLengths[i]=spanBackLengths[i]=
                    spanUTF8Lengths[i]=spanBackUTF8Lengths[i]=
                        (uint8_t)ALL_CP_CONTAINED;
            } else {
                if(which&UTF8) {
                    utf8Lengths[i]=0;
                }
                // All spanXYZLengths pointers contain the same address.
                spanLengths[i]=(uint8_t)ALL_CP_CONTAINED;
            }
        }
    }

    // Finish.
    if(all) {
        pSpanNotSet->freeze();
    }
}

UnicodeSetStringSpan::~UnicodeSetStringSpan() {
    if(pSpanNotSet!=NULL && pSpanNotSet!=&spanSet) {
        delete pSpanNotSet;
    }
    if(utf8Lengths!=NULL && utf8Lengths!=staticLengths) {
        uprv_free(utf8Lengths);
    }
}

void UnicodeSetStringSpan::addToSpanNotSet(UChar32 c) {
    if(pSpanNotSet==NULL || pSpanNotSet==&spanSet) {
        UnicodeSet *newSet=(UnicodeSet *)spanSet.cloneAsThawed();
        if(newSet==NULL) {
            return;  // Out of memory.
        } else {
            pSpanNotSet=newSet;
        }
    }
    pSpanNotSet->add(c);
}

// Compare strings without any argument checks. Requires length>0.
static inline UBool
matches16(const UChar *s, const UChar *t, int32_t length) {
    do {
        if(*s++!=*t++) {
            return FALSE;
        }
    } while(--length>0);
    return TRUE;
}

static inline UBool
matches8(const uint8_t *s, const uint8_t *t, int32_t length) {
    do {
        if(*s++!=*t++) {
            return FALSE;
        }
    } while(--length>0);
    return TRUE;
}

// Does the set contain the next code point?
// If so, return its length; otherwise return its negative length.
static inline int32_t
spanOne(const UnicodeSet &set, const UChar *s, int32_t length) {
    UChar c=*s, c2;
    if(c>=0xd800 && c<=0xdbff && length>=2 && U16_IS_TRAIL(c2=s[1])) {
        return set.contains(U16_GET_SUPPLEMENTARY(c, c2)) ? 2 : -2;
    }
    return set.contains(c) ? 1 : -1;
}

static inline int32_t
spanOneBack(const UnicodeSet &set, const UChar *s, int32_t length) {
    UChar c=s[length-1], c2;
    if(c>=0xdc00 && c<=0xdfff && length>=2 && U16_IS_TRAIL(c2=s[length-2])) {
        return set.contains(U16_GET_SUPPLEMENTARY(c2, c)) ? 2 : -2;
    }
    return set.contains(c) ? 1 : -1;
}

static inline int32_t
spanOneUTF8(const UnicodeSet &set, const uint8_t *s, int32_t length) {
    UChar32 c=*s;
    if((int8_t)c>=0) {
        return set.contains(c) ? 1 : -1;
    }
    // Take advantage of non-ASCII fastpaths in U8_NEXT().
    int32_t i=0;
    U8_NEXT(s, i, length, c);
    return set.contains(c) ? i : -i;
}

static inline int32_t
spanOneBackUTF8(const UnicodeSet &set, const uint8_t *s, int32_t length) {
    UChar32 c=s[length-1];
    if((int8_t)c>=0) {
        return set.contains(c) ? 1 : -1;
    }
    int32_t i=length-1;
    c=utf8_prevCharSafeBody(s, 0, &i, c, -1);
    length-=i;
    return set.contains(c) ? length : -length;
}

/*
 * TODO: span algorithm
 */

int32_t UnicodeSetStringSpan::span(const UChar *s, int32_t length, USetSpanCondition spanCondition) const {
    if(spanCondition==USET_SPAN_WHILE_NOT_CONTAINED) {
        return spanNot(s, length);
    }
    int32_t spanLength=spanSet.span(s, length, USET_SPAN_WHILE_CONTAINED);
    if(spanLength==length) {
        return length;
    }

    // Consider strings; they may overlap with the span.
    IndexList indexes(maxLength16);
    indexes.setStart(spanLength);
    int32_t pos=spanLength, rest=length-pos;
    int32_t i, stringsLength=strings.size();
    for(;;) {
        for(i=0; i<stringsLength; ++i) {
            int32_t overlap=spanLengths[i];
            if(overlap==ALL_CP_CONTAINED) {
                continue;  // Irrelevant string.
            }
            const UnicodeString &string=*(const UnicodeString *)strings.elementAt(i);
            const UChar *s16=string.getBuffer();
            int32_t length16=string.length();

            // Try to match this string at pos-overlap..pos.
            if(overlap==LONG_SPAN) {
                overlap=length16;  // Length of the string minus the last code point.
                U16_BACK_1(s16, 0, overlap);
            }
            if(overlap>spanLength) {
                overlap=spanLength;
            }
            int32_t inc=length16-overlap;  // Keep overlap+inc==length16.
            for(;;) {
                if(inc>rest) {
                    break;
                }
                // Match at code point boundaries if the increment is not listed already.
                if( !(overlap>0 && U16_IS_TRAIL(s[pos-overlap]) &&
                      overlap<spanLength && U16_IS_LEAD(s[pos-overlap-1])) &&
                    !indexes.containsIncrement(inc) &&
                    matches16(s+pos-overlap, s16, length16)
                ) {
                    if(inc==rest) {
                        return length;  // Reached the end of the string.
                    }
                    indexes.addIncrement(inc);
                }
                if(overlap==0) {
                    break;
                }
                --overlap;
                ++inc;
            }
        }
        // Finished trying to match all strings at pos.

        if(spanLength!=0 || pos==0) {
            // The position is after an unlimited code point span (spanLength!=0),
            // not after a string match.
            // The only position where spanLength==0 after a span is pos==0.
            // Otherwise, an unlimited code point span is only tried again after no
            // strings match, and if such a non-initial span fails we stop.
            if(indexes.isEmpty()) {
                return pos;  // No strings matched after a span.
            }
            // Match strings from after the next string match.
        } else {
            // The position is after a string match (or a single code point).
            if(indexes.isEmpty()) {
                // No more strings matched after a previous string match.
                // Try another code point span from after the last string match.
                spanLength=spanSet.span(s+pos, rest, USET_SPAN_WHILE_CONTAINED);
                pos+=spanLength;
                if( pos==length ||      // Reached the end of the string, or
                    spanLength==0       // neither strings nor span progressed.
                ) {
                    return pos;
                }
                indexes.setStart(pos);
                continue;  // spanLength>0: Match strings from after a span.
            } else {
                // Try to match only one code point from after a string match if some
                // string matched beyond it, so that we try all possible positions
                // and don't overshoot.
                spanLength=spanOne(spanSet, s+pos, rest);
                if(spanLength>0) {
                    if(spanLength==rest) {
                        return length;  // Reached the end of the string.
                    }
                    // Match strings after this code point.
                    // There cannot be any increments below it because UnicodeSet strings
                    // contain multiple code points.
                    pos+=spanLength;
                    indexes.incrementStart(spanLength);
                    spanLength=0;
                    continue;  // Match strings from after a single code point.
                }
                // Match strings from after the next string match.
            }
        }
        pos=indexes.popMinimum();
        rest=length-pos;
        spanLength=0;  // Match strings from after a string match.
    }
    return 0;  // TODO
}

int32_t UnicodeSetStringSpan::spanBack(const UChar *s, int32_t length, USetSpanCondition spanCondition) const {
    if(spanCondition==USET_SPAN_WHILE_NOT_CONTAINED) {
        return spanNotBack(s, length);
    }
    return 0;  // TODO
}

int32_t UnicodeSetStringSpan::spanUTF8(const uint8_t *s, int32_t length, USetSpanCondition spanCondition) const {
    if(spanCondition==USET_SPAN_WHILE_NOT_CONTAINED) {
        return spanNotUTF8(s, length);
    }
    return 0;  // TODO
}

int32_t UnicodeSetStringSpan::spanBackUTF8(const uint8_t *s, int32_t length, USetSpanCondition spanCondition) const {
    if(spanCondition==USET_SPAN_WHILE_NOT_CONTAINED) {
        return spanNotBackUTF8(s, length);
    }
    return 0;  // TODO
}

/*
 * TODO: spanNot algorithm
 */

/*
 * Note: In spanNot() and spanNotUTF8(), string matching could use a binary search
 * because all string matches are done from the same start index.
 * For UTF-8, this would require a comparison function that returns UTF-16 order.
 * This should not be necessary for normal UnicodeSets because
 * most sets have no strings, and most sets with strings have
 * very few very short strings.
 */

int32_t UnicodeSetStringSpan::spanNot(const UChar *s, int32_t length) const {
    int32_t pos=0, rest=length;
    int32_t i, stringsLength=strings.size();
    do {
        // Span until we find a code point from the set,
        // or a code point that starts or ends some string.
        i=pSpanNotSet->span(s+pos, rest, USET_SPAN_WHILE_NOT_CONTAINED);
        if(i==rest) {
            return length;  // Reached the end of the string.
        }
        pos+=i;
        rest-=i;

        // Try to match the strings at pos.
        for(i=0; i<stringsLength; ++i) {
            if(spanLengths[i]==ALL_CP_CONTAINED) {
                continue;  // Irrelevant string.
            }
            const UnicodeString &string=*(const UnicodeString *)strings.elementAt(i);
            const UChar *s16=string.getBuffer();
            int32_t length16=string.length();
            if(length16<=rest && matches16(s+pos, s16, length16)) {
                return pos;  // There is a set element at pos.
            }
        }

        // Check whether the current code point is in the original set,
        // without the string starts and ends.
        i=spanOne(spanSet, s+pos, rest);
        if(i>0) {
            return pos;  // There is a set element at pos.
        } else /* i<0 */ {
            // The span(while not contained) ended on a string start/end which is
            // not in the original set. Skip this code point and continue.
            pos-=i;
            rest+=i;
        }
    } while(rest!=0);
    return length;  // Reached the end of the string.
}

int32_t UnicodeSetStringSpan::spanNotBack(const UChar *s, int32_t length) const {
    int32_t pos=length;
    int32_t i, stringsLength=strings.size();
    do {
        // Span until we find a code point from the set,
        // or a code point that starts or ends some string.
        pos=pSpanNotSet->spanBack(s, pos, USET_SPAN_WHILE_NOT_CONTAINED);
        if(pos==0) {
            return 0;  // Reached the start of the string.
        }

        // Try to match the strings at pos.
        for(i=0; i<stringsLength; ++i) {
            // Use spanLengths rather than a spanBackLengths pointer because
            // it is easier and we only need to know whether the string is irrelevant
            // which is the same in either array.
            if(spanLengths[i]==ALL_CP_CONTAINED) {
                continue;  // Irrelevant string.
            }
            const UnicodeString &string=*(const UnicodeString *)strings.elementAt(i);
            const UChar *s16=string.getBuffer();
            int32_t length16=string.length();
            if(length16<=pos && matches16(s+pos-length16, s16, length16)) {
                return pos;  // There is a set element at pos.
            }
        }

        // Check whether the current code point is in the original set,
        // without the string starts and ends.
        i=spanOneBack(spanSet, s, pos);
        if(i>0) {
            return pos;  // There is a set element at pos.
        } else /* i<0 */ {
            // The span(while not contained) ended on a string start/end which is
            // not in the original set. Skip this code point and continue.
            pos+=i;
        }
    } while(pos!=0);
    return length;  // Reached the start of the string.
}

int32_t UnicodeSetStringSpan::spanNotUTF8(const uint8_t *s, int32_t length) const {
    int32_t pos=0, rest=length;
    int32_t i, stringsLength=strings.size();
    do {
        // Span until we find a code point from the set,
        // or a code point that starts or ends some string.
        i=pSpanNotSet->spanUTF8((const char *)s+pos, rest, USET_SPAN_WHILE_NOT_CONTAINED);
        if(i==rest) {
            return length;  // Reached the end of the string.
        }
        pos+=i;
        rest-=i;

        // Try to match the strings at pos.
        const uint8_t *s8=utf8;
        int32_t length8;
        for(i=0; i<stringsLength; ++i) {
            if(spanLengths[i]==ALL_CP_CONTAINED) {
                continue;  // Irrelevant string.
            }
            length8=utf8Lengths[i];
            if(length8<=rest && matches8(s+pos, s8, length8)) {
                return pos;  // There is a set element at pos.
            }
            s8+=length8;
        }

        // Check whether the current code point is in the original set,
        // without the string starts and ends.
        i=spanOneUTF8(spanSet, s+pos, rest);
        if(i>0) {
            return pos;  // There is a set element at pos.
        } else /* i<0 */ {
            // The span(while not contained) ended on a string start/end which is
            // not in the original set. Skip this code point and continue.
            pos-=i;
            rest+=i;
        }
    } while(rest!=0);
    return length;  // Reached the end of the string.
}

int32_t UnicodeSetStringSpan::spanNotBackUTF8(const uint8_t *s, int32_t length) const {
    int32_t pos=length;
    int32_t i, stringsLength=strings.size();
    do {
        // Span until we find a code point from the set,
        // or a code point that starts or ends some string.
        pos=pSpanNotSet->spanBackUTF8((const char *)s, pos, USET_SPAN_WHILE_NOT_CONTAINED);
        if(pos==0) {
            return 0;  // Reached the start of the string.
        }

        // Try to match the strings at pos.
        const uint8_t *s8=utf8;
        int32_t length8;
        for(i=0; i<stringsLength; ++i) {
            // Use spanLengths rather than a spanBackLengths pointer because
            // it is easier and we only need to know whether the string is irrelevant
            // which is the same in either array.
            if(spanLengths[i]==ALL_CP_CONTAINED) {
                continue;  // Irrelevant string.
            }
            length8=utf8Lengths[i];
            if(length8<=pos && matches8(s+pos-length8, s8, length8)) {
                return pos;  // There is a set element at pos.
            }
            s8+=length8;
        }

        // Check whether the current code point is in the original set,
        // without the string starts and ends.
        i=spanOneBackUTF8(spanSet, s, pos);
        if(i>0) {
            return pos;  // There is a set element at pos.
        } else /* i<0 */ {
            // The span(while not contained) ended on a string start/end which is
            // not in the original set. Skip this code point and continue.
            pos+=i;
        }
    } while(pos!=0);
    return length;  // Reached the start of the string.
}

U_NAMESPACE_END
