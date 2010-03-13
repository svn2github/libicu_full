/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  uts46.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010mar09
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
                 int32_t labelStart, int32_t labelLength,
                 const Normalizer2 &norm2, UBool toASCII,
                 uint32_t &errors, UErrorCode &errorCode) const;

    UBool
    isLabelOkBiDi(const UChar *label, int32_t labelLength) const;

    UBool
    isLabelOkContextJ(const UChar *label, int32_t labelLength) const;

    UnicodeSet *transSet;  // NULL, or ASCII without LDH+dot
    UnicodeSet *nontransSet;  // transSet & notDeviationSet
    const Normalizer2 &norm2;  // uts46.nrm
    FilteredNormalizer2 transNorm2;  // maps deviation characters, with transSet
    FilteredNormalizer2 nontransNorm2;  // passes deviation characters through
    const Normalizer2 &toASCIINorm2;
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
        : transSet(opt&UIDNA_USE_STD3_RULES ? NULL : new UnicodeSet(0, 0x10ffff)),
          nontransSet(new UnicodeSet(0, 0x10ffff)),
          norm2(*Normalizer2::getInstance(NULL, "uts46", UNORM2_COMPOSE, errorCode)),
          transNorm2(norm2, *transSet),
          nontransNorm2(norm2, *nontransSet),
          toASCIINorm2(
              opt&UIDNA_NONTRANSITIONAL_TO_ASCII ? nontransNorm2 :
              opt&UIDNA_USE_STD3_RULES ? norm2 : transNorm2),
          options(opt) {
    if(U_SUCCESS(errorCode)) {
        if(((opt&UIDNA_USE_STD3_RULES)==0 && transSet==NULL) || nontransSet==NULL) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
        } else {
            if((opt&UIDNA_USE_STD3_RULES)==0) {
                // If we do not enforce STD3 rules, then we pass through
                // all ASCII except LDH and the dot.
                // The uts46.nrm table maps all such characters to U+FFFD,
                // so we use a FilteredNormalizer and normalize all characters
                // except the ASCII non-LDH+dot set.
                transSet->remove(0, 0x2c).remove(0x2f).remove(0x3a, 0x40).
                          remove(0x5b, 0x60).remove(0x7b, 0x7f).freeze();
                nontransSet->retainAll(*transSet);
            }
            // In nontransitional processing, we pass through deviation characters,
            // rather than mapping them.
            nontransSet->remove(0xdf).remove(0x3c2).remove(0x200c, 0x200d).freeze();
        }
    }
}

UTS46::~UTS46() {
    delete transSet;
    delete nontransSet;
}

#if 0
// TODO: May need such argument checking in ASCII fastpath, when we add that optimization.
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
    process(label, toASCIINorm2, TRUE, TRUE, dest, errors, errorCode);
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
    process(name, toASCIINorm2, FALSE, TRUE, dest, errors, errorCode);
    if(dest.length()>=254 && (dest.length()>254 || dest[253]!=0x2e)) {
        errors|=UIDNA_ERROR_DOMAIN_NAME_TOO_LONG;
    }
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
    errors=0;
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
                delta=processLabel(dest, labelStart, labelLimit-labelStart,
                                   norm2, toASCII, errors, errorCode);
                if(U_FAILURE(errorCode)) {
                    return dest;
                }
                destArray=dest.getBuffer();
                destLength+=delta;
                labelStart=labelLimit+=delta+1;
            }
        }
        // Permit an empty label at the end (0<labelStart==labelLimit is ok)
        // but not an empty label elsewhere nor a completely empty domain name.
        // processLabel() sets UIDNA_ERROR_EMPTY_LABEL when labelLength==0.
        if(0==labelStart || labelStart<labelLimit) {
            processLabel(dest, labelStart, labelLimit-labelStart,
                         norm2, toASCII, errors, errorCode);
        }
    }
    return dest;
}

int32_t
UTS46::processLabel(UnicodeString &dest,
                    int32_t labelStart, int32_t labelLength,
                    const Normalizer2 &norm2, UBool toASCII,
                    uint32_t &errors, UErrorCode &errorCode) const {
    const UChar *label=dest.getBuffer()+labelStart;
    int32_t delta=0;
    // TODO: Copy UTS #46 snippets into code comments for processing and validation.
    // TODO: Fastpath for ASCII label
    // TODO: Remember whether the input was Punycode, and don't double-replace for toASCII,
    //       unless there were errors
    if(labelLength>=4 && label[0]==0x78 && label[1]==0x6e && label[2]==0x2d && label[3]==0x2d) {
        // Label starts with "xn--", try to un-Punycode it.
        UnicodeString unicode;
        UChar *unicodeBuffer=unicode.getBuffer(-1);  // capacity==-1: most labels should fit
        if(unicodeBuffer==NULL) {
            // Should never occur if we used capacity==-1 which uses the internal buffer.
            errorCode=U_MEMORY_ALLOCATION_ERROR;
            return 0;
        }
        UErrorCode punycodeErrorCode=U_ZERO_ERROR;
        int32_t unicodeLength=u_strFromPunycode(label+4, labelLength-4,
                                                unicodeBuffer, unicode.getCapacity(),
                                                NULL, &punycodeErrorCode);
        if(punycodeErrorCode==U_BUFFER_OVERFLOW_ERROR) {
            unicode.releaseBuffer(0);
            unicodeBuffer=unicode.getBuffer(unicodeLength);
            if(unicodeBuffer==NULL) {
                errorCode=U_MEMORY_ALLOCATION_ERROR;
                return 0;
            }
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
            dest.insert(labelStart+labelLength+1, (UChar)0xfffd);
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
        if(unicode!=mapped) {
            errors|=UIDNA_ERROR_INVALID_ACE_LABEL;
        }
        // Replace the Punycode label with its Unicode form.
        dest.replace(labelStart, labelLength, mapped);
        delta=mapped.length()-labelLength;
        label=dest.getBuffer()+labelStart;
        labelLength=mapped.length();
    }
    // Validity check
    if(labelLength==0) {
        if(toASCII) {
            errors|=UIDNA_ERROR_EMPTY_LABEL;
            // TODO: insert U+FFFD? (I think not)
        }
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
    // The optional STD3 restriction to LDH characters was handled by the
    // construction and selection of the normalizer.
    // All we need to look for now is U+FFFD which indicates disallowed characters
    // in a non-Punycode label or U+FFFD itself in a Punycode label.
    // We also check for dots which can come from a Punycode label
    // or from the input to a single-label function.
    // Ok to cast away const because we own the UnicodeString.
    UChar *s=(UChar *)label;
    const UChar *limit=label+labelLength;
    UChar oredChars=0;
    do {
        UChar c=*s;
        oredChars|=c;
        if(c==0xfffd) {
            errors|=UIDNA_ERROR_DISALLOWED;
        } else if(c==0x2e) {
            errors|=UIDNA_ERROR_LABEL_HAS_DOT;
            *s=0xfffd;
        }
    } while(++s<limit);
    if(UIDNA_CHECK_BIDI && oredChars>=0x590 && !isLabelOkBiDi(label, labelLength)) {
        errors|=UIDNA_ERROR_BIDI;
    }
    if( UIDNA_CHECK_CONTEXTJ && (oredChars&0x200c)==0x200c &&
        !isLabelOkContextJ(label, labelLength)
    ) {
        errors|=UIDNA_ERROR_CONTEXTJ;
    }
    if(toASCII) {
        if(oredChars>=0x80) {
            UnicodeString punycode=UNICODE_STRING("xn--", 4);
            UChar *buffer=punycode.getBuffer(63);  // 63==maximum DNS label length
            if(buffer==NULL) {
                errorCode=U_MEMORY_ALLOCATION_ERROR;
                return delta;
            }
            int32_t punycodeLength=u_strToPunycode(label, labelLength,
                                                   buffer+4, punycode.getCapacity()-4,
                                                   NULL, &errorCode);
            if(errorCode==U_BUFFER_OVERFLOW_ERROR) {
                errorCode=U_ZERO_ERROR;
                punycode.releaseBuffer(4);
                errors|=UIDNA_ERROR_LABEL_TOO_LONG;
                return delta;
            }
            punycode.releaseBuffer(4+punycodeLength);
            if(U_FAILURE(errorCode)) {
                return delta;
            }
            // Replace the Unicode label with its Punycode form.
            dest.replace(labelStart, labelLength, punycode);
            int32_t newDelta=punycode.length()-labelLength;
            delta+=newDelta;
            label=dest.getBuffer()+labelStart;
            labelLength=punycode.length();
        }
        if(labelLength>63) {
            errors|=UIDNA_ERROR_LABEL_TOO_LONG;
        }
    }
    return delta;
}

#define L_MASK U_MASK(U_LEFT_TO_RIGHT)
#define R_AL_MASK U_MASK(U_RIGHT_TO_LEFT)|U_MASK(U_RIGHT_TO_LEFT_ARABIC)
#define L_R_AL_MASK L_MASK|R_AL_MASK

#define EN_AN_MASK U_MASK(U_EUROPEAN_NUMBER)|U_MASK(U_ARABIC_NUMBER)
#define R_AL_EN_AN_MASK R_AL_MASK|EN_AN_MASK
#define L_EN_MASK L_MASK|U_MASK(U_EUROPEAN_NUMBER)

#define ES_CS_ET_ON_BN_NSM_MASK \
    U_MASK(U_EUROPEAN_NUMBER_SEPARATOR)| \
    U_MASK(U_COMMON_NUMBER_SEPARATOR)| \
    U_MASK(U_EUROPEAN_NUMBER_TERMINATOR)| \
    U_MASK(U_OTHER_NEUTRAL)| \
    U_MASK(U_BOUNDARY_NEUTRAL)| \
    U_MASK(U_DIR_NON_SPACING_MARK)
#define L_EN_ES_CS_ET_ON_BN_NSM_MASK L_EN_MASK|ES_CS_ET_ON_BN_NSM_MASK
#define R_AL_AN_EN_ES_CS_ET_ON_BN_NSM_MASK R_AL_MASK|EN_AN_MASK|ES_CS_ET_ON_BN_NSM_MASK

UBool
UTS46::isLabelOkBiDi(const UChar *label, int32_t labelLength) const {
    // IDNA2008 BiDi rule
    // Get the directionality of the first character.
    UChar32 c;
    int32_t i=0;
    U16_NEXT_UNSAFE(label, i, c);
    uint32_t firstMask=U_MASK(u_charDirection(c));
    // 1. The first character must be a character with BIDI property L, R
    // or AL.  If it has the R or AL property, it is an RTL label; if it
    // has the L property, it is an LTR label.
    if((firstMask&~L_R_AL_MASK)!=0) {
        return FALSE;
    }
    // Get the directionality of the last non-NSM character.
    uint32_t lastMask;
    for(;;) {
        if(i>=labelLength) {
            lastMask=firstMask;
            break;
        }
        U16_PREV_UNSAFE(label, labelLength, c);
        UCharDirection dir=u_charDirection(c);
        if(dir!=U_DIR_NON_SPACING_MARK) {
            lastMask=U_MASK(dir);
            break;
        }
    }
    // 3. In an RTL label, the end of the label must be a character with
    // BIDI property R, AL, EN or AN, followed by zero or more
    // characters with BIDI property NSM.
    // 6. In an LTR label, the end of the label must be a character with
    // BIDI property L or EN, followed by zero or more characters with
    // BIDI property NSM.
    if( (firstMask&L_MASK)!=0 ?
            (lastMask&~L_EN_MASK)!=0 :
            (lastMask&~R_AL_EN_AN_MASK)!=0
    ) {
        return FALSE;
    }
    // Get the directionalities of the intervening characters.
    uint32_t mask=0;
    while(i<labelLength) {
        U16_NEXT_UNSAFE(label, i, c);
        mask|=U_MASK(u_charDirection(c));
    }
    if(firstMask&L_MASK) {
        // 5. In an LTR label, only characters with the BIDI properties L, EN,
        // ES, CS.  ET, ON, BN and NSM are allowed.
        if((mask&~L_EN_ES_CS_ET_ON_BN_NSM_MASK)!=0) {
            return FALSE;
        }
    } else {
        // 2. In an RTL label, only characters with the BIDI properties R, AL,
        // AN, EN, ES, CS, ET, ON, BN and NSM are allowed.
        if((mask&~R_AL_AN_EN_ES_CS_ET_ON_BN_NSM_MASK)!=0) {
            return FALSE;
        }
        // 4. In an RTL label, if an EN is present, no AN may be present, and
        // vice versa.
        if((mask&EN_AN_MASK)==EN_AN_MASK) {
            return FALSE;
        }
    }
    return TRUE;
}

UBool
UTS46::isLabelOkContextJ(const UChar *label, int32_t labelLength) const {
    // [IDNA2008-Tables]
    // 200C..200D  ; CONTEXTJ    # ZERO WIDTH NON-JOINER..ZERO WIDTH JOINER
    for(int32_t i=0; i<labelLength; ++i) {
        if(label[i]==0x200c) {
            // Appendix A.1. ZERO WIDTH NON-JOINER
            // Rule Set:
            //  False;
            //  If Canonical_Combining_Class(Before(cp)) .eq.  Virama Then True;
            //  If RegExpMatch((Joining_Type:{L,D})(Joining_Type:T)*\u200C
            //     (Joining_Type:T)*(Joining_Type:{R,D})) Then True;
            if(i==0) {
                return FALSE;
            }
            UChar32 c;
            int32_t j=i;
            U16_PREV_UNSAFE(label, j, c);
            if(u_getCombiningClass(c)==9) {
                continue;
            }
            // check precontext (Joining_Type:{L,D})(Joining_Type:T)*
            for(;;) {
                UJoiningType type=(UJoiningType)u_getIntPropertyValue(c, UCHAR_JOINING_TYPE);
                if(type==U_JT_TRANSPARENT) {
                    if(j==0) {
                        return FALSE;
                    }
                    U16_PREV_UNSAFE(label, j, c);
                } else if(type==U_JT_LEFT_JOINING || type==U_JT_DUAL_JOINING) {
                    break;  // precontext fulfilled
                } else {
                    return FALSE;
                }
            }
            // check postcontext (Joining_Type:T)*(Joining_Type:{R,D})
            for(j=i+1;;) {
                if(j==labelLength) {
                    return FALSE;
                }
                U16_NEXT_UNSAFE(label, i, c);
                UJoiningType type=(UJoiningType)u_getIntPropertyValue(c, UCHAR_JOINING_TYPE);
                if(type==U_JT_TRANSPARENT) {
                    // just skip this character
                } else if(type==U_JT_RIGHT_JOINING || type==U_JT_DUAL_JOINING) {
                    break;  // postcontext fulfilled
                } else {
                    return FALSE;
                }
            }
        } else if(label[i]==0x200d) {
            // Appendix A.2. ZERO WIDTH JOINER (U+200D)
            // Rule Set:
            //  False;
            //  If Canonical_Combining_Class(Before(cp)) .eq.  Virama Then True;
            if(i==0) {
                return FALSE;
            }
            UChar32 c;
            int32_t j=i;
            U16_PREV_UNSAFE(label, j, c);
            if(u_getCombiningClass(c)!=9) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

U_NAMESPACE_END

#endif  // UCONFIG_NO_IDNA
