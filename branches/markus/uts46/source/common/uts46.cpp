/*
*******************************************************************************
*
*   Copyright (C) 2009-2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  normalizer2.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov22
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_IDNA

#include "unicode/idna.h"
#include "unicode/normalizer2.h"
#include "unicode/uniset.h"
#include "punycode.h"

U_NAMESPACE_BEGIN

// UTS46 class declaration ------------------------------------------------- ***

class UTS46 : public IDNA {
public:
    UTS46(uint32_t options, UErrorCode &errorCode);
    virtual ~UTS46();

    virtual UnicodeString &
    labelToASCII(const UnicodeString &label, UnicodeString &dest,
                 uint32_t &errors, UErrorCode &errorCode) const;

    virtual UnicodeString &
    labelToUnicode(const UnicodeString &label, UnicodeString &dest,
                   uint32_t &errors, UErrorCode &errorCode) const;

    virtual UnicodeString &
    nameToASCII(const UnicodeString &name, UnicodeString &dest,
                uint32_t &errors, UErrorCode &errorCode) const;

    virtual UnicodeString &
    nameToUnicode(const UnicodeString &name, UnicodeString &dest,
                  uint32_t &errors, UErrorCode &errorCode) const;

    static UClassID U_EXPORT2 getStaticClassID();
    virtual UClassID getDynamicClassID() const;

private:
    UnicodeString &
    process(const UnicodeString &src,
            const Normalizer2 &norm2, UBool isLabel, UBool toASCII,
            UnicodeString &dest,
            uint32_t &errors, UErrorCode &errorCode) const;

    // returns delta for how much the label length changes
    int32_t
    processLabel(UnicodeString &dest,
                 int32_t labelStart, int32_t labelLimit,
                 const Normalizer2 &norm2, UBool toASCII,
                 uint32_t &errors, UErrorCode &errorCode) const;

    UnicodeSet *notDeviationSet;
    const Normalizer2 &transNorm2;  // maps deviation characters
    FilteredNormalizer2 nontransNorm2;  // passes deviation characters through
    uint32_t options;
};

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(UTS46)

IDNA *
IDNA::createUTS46Instance(uint32_t options, UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode)) {
        return new UTS46(options, errorCode);
    } else {
        return NULL;
    }
}

UOBJECT_DEFINE_ABSTRACT_RTTI_IMPLEMENTATION(IDNA)

// UTS46 implementation ---------------------------------------------------- ***

UTS46::UTS46(uint32_t opt, UErrorCode &errorCode)
        : notDeviationSet(new UnicodeSet(0, 0x10ffff)),
          transNorm2(*Normalizer2::getInstance(NULL, "uts46", UNORM2_COMPOSE, errorCode)),
          nontransNorm2(transNorm2, *notDeviationSet),
          options(opt) {
    if(U_SUCCESS(errorCode)) {
        if(notDeviationSet==NULL) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
        } else {
            notDeviationSet->remove(0xdf).remove(0x3c2).remove(0x200c, 0x200d).freeze();
        }
    }
}

UTS46::~UTS46() {
    delete notDeviationSet;
}

#if 0
// TODO: May need such argument checking in ASCII fastpath.
static const UChar *checkArgs(const UnicodeString &src, UnicodeString &dest,
                              uint32_t &errors, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        dest.setToBogus();
        return NULL;
    }
    const UChar *srcArray=src.getBuffer();
    if(&dest==&src || srcArray==NULL) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        dest.setToBogus();
        return NULL;
    }
    // Arguments are fine, reset output values.
    dest.remove();
    errors=0;
    return srcArray;
}
#endif

UnicodeString &
UTS46::labelToASCII(const UnicodeString &label, UnicodeString &dest,
                    uint32_t &errors, UErrorCode &errorCode) const {
    process(label, nontransNorm2, TRUE, TRUE, dest, errors, errorCode);
    if(errors!=0) {
        dest.setToBogus();
    }
    return dest;
}

UnicodeString &
UTS46::labelToUnicode(const UnicodeString &label, UnicodeString &dest,
                      uint32_t &errors, UErrorCode &errorCode) const {
    return process(label, nontransNorm2, TRUE, FALSE, dest, errors, errorCode);
}

UnicodeString &
UTS46::nameToASCII(const UnicodeString &name, UnicodeString &dest,
                   uint32_t &errors, UErrorCode &errorCode) const {
    process(name, nontransNorm2, FALSE, TRUE, dest, errors, errorCode);
    if(errors!=0) {
        dest.setToBogus();
    }
    return dest;
}

UnicodeString &
UTS46::nameToUnicode(const UnicodeString &name, UnicodeString &dest,
                     uint32_t &errors, UErrorCode &errorCode) const {
    return process(name, nontransNorm2, FALSE, FALSE, dest, errors, errorCode);
}

UnicodeString &
UTS46::process(const UnicodeString &src,
               const Normalizer2 &norm2, UBool isLabel, UBool toASCII,
               UnicodeString &dest,
               uint32_t &errors, UErrorCode &errorCode) const {
    norm2.normalize(src, dest, errorCode);
    if(U_FAILURE(errorCode)) {
        return dest;
    }
    if(isLabel) {
        processLabel(dest, 0, dest.length(), norm2, toASCII, errors, errorCode);
    } else {
        const UChar *destArray=dest.getBuffer();
        int32_t destLength=dest.length();
        int32_t labelStart=0, labelLimit=0, delta;
        while(labelLimit<destLength) {
            if(destArray[labelLimit]==0x2e) {
                delta=processLabel(dest, labelStart, labelLimit, norm2, toASCII, errors, errorCode);
                if(U_FAILURE(errorCode)) {
                    return dest;
                }
                destArray=dest.getBuffer();
                destLength+=delta;
                labelStart=labelLimit+=delta+1;
            }
        }
        // permit an empty label at the end
        if(0<labelStart && labelStart<labelLimit) {
            processLabel(dest, labelStart, labelLimit, norm2, toASCII, errors, errorCode);
        }
    }
    return dest;
}

int32_t
UTS46::processLabel(UnicodeString &dest,
                    int32_t labelStart, int32_t labelLimit,
                    const Normalizer2 &norm2, UBool toASCII,
                    uint32_t &errors, UErrorCode &errorCode) const {
    const UChar *label=dest.getBuffer()+labelStart;
    int32_t labelLength=labelLimit-labelStart;
    int32_t delta=0;
    // TODO: Fastpath for ASCII label
    // TODO: Remember whether the input was Punycode, and don't double-replace for toASCII,
    //       unless there were errors
    if(labelLength>=4 && label[0]==0x78 && label[1]==0x6e && label[2]==0x2d && label[3]==0x2d) {
        // Label starts with "xn--", try to un-Punycode it.
        UnicodeString unicode;
        UChar *unicodeBuffer=unicode.getBuffer(-1);
        // TODO: buffer==NULL?
        UErrorCode punycodeErrorCode=U_ZERO_ERROR;
        int32_t unicodeLength=u_strFromPunycode(label+4, labelLength-4,
                                                unicodeBuffer, unicode.getCapacity(),
                                                NULL, &punycodeErrorCode);
        if(punycodeErrorCode==U_BUFFER_OVERFLOW_ERROR) {
            unicode.releaseBuffer(0);
            unicodeBuffer=unicode.getBuffer(unicodeLength);
            // TODO: buffer==NULL?
            punycodeErrorCode=U_ZERO_ERROR;
            unicodeLength=u_strFromPunycode(label+4, labelLength-4,
                                            unicodeBuffer, unicode.getCapacity(),
                                            NULL, &punycodeErrorCode);
        }
        unicode.releaseBuffer(unicodeLength);
        if(U_FAILURE(punycodeErrorCode)) {
            // Failure, prepend and append U+FFFD.
            errors|=UIDNA_ERROR_PUNYCODE;
            dest.insert(labelStart, (UChar)0xfffd);
            dest.insert(labelLimit+1, (UChar)0xfffd);
            return 2;
        }
        // Check for NFC, and for characters that are not
        // valid or deviation characters according to the normalizer.
        // If there is something wrong, then the string will change.
        UnicodeString mapped;
        norm2.normalize(unicode, mapped, errorCode);
        if(U_FAILURE(errorCode)) {
            return 0;
        }
        const UnicodeString *final;
        if(unicode==mapped) {
            final=&unicode;
        } else {
            errors|=UIDNA_ERROR_INVALID_ACE_LABEL;
            final=&mapped;
        }
        // Check for dots.
        // Ok to cast away const because we own the UnicodeString.
        UChar *s=(UChar *)final->getBuffer();
        const UChar *limit=s+final->length();
        do {
            if(*s==0x2e) {
                errors|=UIDNA_ERROR_ACE_LABEL_HAS_DOT;
                *s=0xfffd;
            }
        } while(++s<limit);
        // Replace the Punycode label with its Unicode form.
        dest.replace(labelStart, labelLength, *final);
        delta=final->length()-labelLength;
        label=dest.getBuffer()+labelStart;
        labelLength=final->length();
        labelLimit+=delta;
        // TODO: is labelLimit still needed? need to track it?
    }
    // Validity check
    if(labelLength==0) {
        errors|=UIDNA_ERROR_EMPTY_LABEL;
        // TODO: insert U+FFFD? (I think not)
        return delta;
    }
    // labelLength>0
    if(labelLength>=4 && label[2]==0x2d && label[3]==0x2d) {
        // label starts with "??--"
        errors|=UIDNA_ERROR_HYPHEN_3_4;
        // TODO: replace each hyphen with U+FFFD? (I think not)
    }
    if(label[0]==0x2d) {
        // label starts with "-"
        errors|=UIDNA_ERROR_LEADING_HYPHEN;
        // TODO: replace hyphen with U+FFFD? (I think not)
    }
    if(label[labelLength-1]==0x2d) {
        // label ends with "-"
        errors|=UIDNA_ERROR_TRAILING_HYPHEN;
        // TODO: replace hyphen with U+FFFD? (I think not)
    }
    UChar32 c;
    int32_t cpLength=0;
    // "Unsafe" is ok because unpaired surrogates were mapped to U+FFFD.
    U16_NEXT_UNSAFE(label, cpLength, c);
    if((U_GET_GC_MASK(c)&U_GC_M_MASK)!=0) {
        errors|=UIDNA_ERROR_LEADING_COMBINING_MARK;
    }
    // If the label was not a Punycode label, then it was the result of
    // mapping, normalization and label segmentation.
    // If the label was in Punycode, then we mapped it again above
    // and checked its validity.
    // All we need to look for now is U+FFFD which indicates disallowed characters
    // in a non-Punycode label or U+FFFD itself in a Punycode label.
    const UChar *s=label;
    const UChar *limit=label+labelLength;
    UChar oredChars=0;
    do {
        UChar c=*s;
        oredChars|=c;
        if(c==0xfffd) {
            errors|=UIDNA_ERROR_DISALLOWED;
        }
    } while(++s<limit);
    // TODO: Optional BiDi check
    // TODO: Optional CONTEXTJ check
    if(toASCII) {
        if(oredChars>=0x80) {
            UnicodeString punycode=UNICODE_STRING("xn--", 4);
            UChar *buffer=punycode.getBuffer(63);  // TODO: magic number
            // TODO: buffer==NULL?
            int32_t punycodeLength=u_strToPunycode(label, labelLength,
                                                   buffer+4, punycode.getCapacity()-4,
                                                   NULL, &errorCode);
            if(errorCode==U_BUFFER_OVERFLOW_ERROR) {
                punycode.releaseBuffer(4);
                buffer=punycode.getBuffer(4+punycodeLength);
                // TODO: buffer==NULL?
                errorCode=U_ZERO_ERROR;
                punycodeLength=u_strToPunycode(label, labelLength,
                                               buffer+4, punycode.getCapacity()-4,
                                               NULL, &errorCode);
            }
            punycode.releaseBuffer(4+punycodeLength);
            if(U_FAILURE(errorCode)) {
                return 0;
            }
            // Replace the Unicode label with its Punycode form.
            dest.replace(labelStart, labelLength, punycode);
            int32_t newDelta=punycode.length()-labelLength;
            delta+=newDelta;
            label=dest.getBuffer()+labelStart;
            labelLength=punycode.length();
            labelLimit+=newDelta;
        }
        // TODO: Optional STD3 & STD13 checks
    }
    return delta;
}

U_NAMESPACE_END

#endif  // UCONFIG_NO_IDNA
