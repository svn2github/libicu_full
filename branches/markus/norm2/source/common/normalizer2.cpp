/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
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

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/localpointer.h"
#include "unicode/normalizer2.h"
#include "cstring.h"
#include "mutex.h"
#include "normalizer2impl.h"
#include "ucln_cmn.h"

U_NAMESPACE_BEGIN

inline void assertNotBogus(const UnicodeString &s, UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode) && s.isBogus()) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
    }
}

// Public API dispatch via Normalizer2 subclasses -------------------------- ***

class Normalizer2WithImpl : public Normalizer2 {
protected:
    Normalizer2WithImpl(const Normalizer2Impl &ni) : impl(ni) {}
    const Normalizer2Impl &impl;
};

class DecomposeNormalizer2 : public Normalizer2WithImpl {
public:
    DecomposeNormalizer2(const Normalizer2Impl &ni) : Normalizer2WithImpl(ni) {}

    virtual UnicodeString &
    normalize(const UnicodeString &src,
              UnicodeString &dest,
              UErrorCode &errorCode) const {
        assertNotBogus(src, errorCode);
        impl.decompose(src.getBuffer(), src.length(), dest, errorCode);
        return dest;
    }
    virtual UnicodeString &
    normalizeSecondAndAppend(UnicodeString &first,
                             const UnicodeString &second,
                             UErrorCode &errorCode) const {
        assertNotBogus(second, errorCode);
        impl.decomposeAndAppend(second.getBuffer(), second.length(), first, TRUE, errorCode);
        return first;
    }
    virtual UnicodeString &
    append(UnicodeString &first,
           const UnicodeString &second,
           UErrorCode &errorCode) const {
        assertNotBogus(second, errorCode);
        impl.decomposeAndAppend(second.getBuffer(), second.length(), first, FALSE, errorCode);
        return first;
    }
};

class ComposeNormalizer2 : public Normalizer2WithImpl {
public:
    ComposeNormalizer2(const Normalizer2Impl &ni, UBool fcc) :
        Normalizer2WithImpl(ni), onlyContiguous(fcc) {}

    virtual UnicodeString &
    normalize(const UnicodeString &src,
              UnicodeString &dest,
              UErrorCode &errorCode) const {
        assertNotBogus(src, errorCode);
        impl.compose(src.getBuffer(), src.length(), dest, onlyContiguous, errorCode);
        return dest;
    }
    virtual UnicodeString &
    normalizeSecondAndAppend(UnicodeString &first,
                             const UnicodeString &second,
                             UErrorCode &errorCode) const {
        assertNotBogus(second, errorCode);
        impl.composeAndAppend(second.getBuffer(), second.length(), first,
                              TRUE, onlyContiguous, errorCode);
        return first;
    }
    virtual UnicodeString &
    append(UnicodeString &first,
           const UnicodeString &second,
           UErrorCode &errorCode) const {
        assertNotBogus(second, errorCode);
        impl.composeAndAppend(second.getBuffer(), second.length(), first,
                              FALSE, onlyContiguous, errorCode);
        return first;
    }
private:
    UBool onlyContiguous;
};

// instance cache ---------------------------------------------------------- ***

struct Norm2AllModes : public UMemory {
    static Norm2AllModes *createInstance(const char *packageName,
                                         const char *name,
                                         UErrorCode &errorCode);
    Norm2AllModes() : comp(impl, FALSE), decomp(impl), fcc(impl, TRUE) {}

    Normalizer2Impl impl;
    ComposeNormalizer2 comp;
    DecomposeNormalizer2 decomp;
    ComposeNormalizer2 fcc;
};

Norm2AllModes *
Norm2AllModes::createInstance(const char *packageName,
                              const char *name,
                              UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return NULL;
    }
    LocalPointer<Norm2AllModes> allModes(new Norm2AllModes);
    if(allModes.isNull()) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    allModes->impl.load(packageName, name, errorCode);
    return U_SUCCESS(errorCode) ? allModes.orphan() : NULL;
}

U_CDECL_BEGIN
static UBool U_CALLCONV uprv_normalizer2_cleanup();
U_CDECL_END

class Norm2AllModesSingleton : public IcuSingletonWrapper<Norm2AllModes> {
public:
    Norm2AllModesSingleton(IcuSingleton &s, const char *n) :
        IcuSingletonWrapper<Norm2AllModes>(s), name(n) {}
    Norm2AllModes *getInstance(UErrorCode &errorCode) {
        return IcuSingletonWrapper<Norm2AllModes>::getInstance(createInstance, name, errorCode);
    }
private:
    static void *createInstance(const void *context, UErrorCode &errorCode) {
        ucln_common_registerCleanup(UCLN_COMMON_NORMALIZER2, uprv_normalizer2_cleanup);
        return Norm2AllModes::createInstance(NULL, (const char *)context, errorCode);
    }

    const char *name;
};

STATIC_ICU_SINGLETON(nfcSingleton);
STATIC_ICU_SINGLETON(nfkcSingleton);
STATIC_ICU_SINGLETON(nfkc_cfSingleton);

U_CDECL_BEGIN

static UBool U_CALLCONV uprv_normalizer2_cleanup() {
    Norm2AllModesSingleton(nfcSingleton, NULL).deleteInstance();
    Norm2AllModesSingleton(nfkcSingleton, NULL).deleteInstance();
    Norm2AllModesSingleton(nfkc_cfSingleton, NULL).deleteInstance();
    return TRUE;
}

U_CDECL_END

Normalizer2 *InternalNormalizer2Provider::getNFCInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=Norm2AllModesSingleton(nfcSingleton, "nfc").getInstance(errorCode);
    return allModes!=NULL ? &allModes->comp : NULL;
}

Normalizer2 *InternalNormalizer2Provider::getNFDInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=Norm2AllModesSingleton(nfcSingleton, "nfc").getInstance(errorCode);
    return allModes!=NULL ? &allModes->decomp : NULL;
}

Normalizer2 *InternalNormalizer2Provider::getFCCInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=Norm2AllModesSingleton(nfcSingleton, "nfc").getInstance(errorCode);
    return allModes!=NULL ? &allModes->fcc : NULL;
}

Normalizer2 *InternalNormalizer2Provider::getNFKCInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=
        Norm2AllModesSingleton(nfkcSingleton, "nfkc").getInstance(errorCode);
    return allModes!=NULL ? &allModes->comp : NULL;
}

Normalizer2 *InternalNormalizer2Provider::getNFKDInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=
        Norm2AllModesSingleton(nfkcSingleton, "nfkc").getInstance(errorCode);
    return allModes!=NULL ? &allModes->decomp : NULL;
}

Normalizer2 *InternalNormalizer2Provider::getNFKC_CFInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=
        Norm2AllModesSingleton(nfkc_cfSingleton, "nfkc_cf").getInstance(errorCode);
    return allModes!=NULL ? &allModes->comp : NULL;
}

Normalizer2 *
Normalizer2::getInstance(const char *packageName,
                         const char *name,
                         UNormalization2Mode mode,
                         UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return NULL;
    }
    if(packageName==NULL) {
        Norm2AllModes *allModes=NULL;
        if(0==uprv_strcmp(name, "nfc")) {
            allModes=Norm2AllModesSingleton(nfcSingleton, "nfc").getInstance(errorCode);
        } else if(0==uprv_strcmp(name, "nfkc")) {
            allModes=Norm2AllModesSingleton(nfkcSingleton, "nfkc").getInstance(errorCode);
        } else if(0==uprv_strcmp(name, "nfkc_cf")) {
            allModes=Norm2AllModesSingleton(nfkc_cfSingleton, "nfkc_cf").getInstance(errorCode);
        }
        if(allModes!=NULL) {
            switch(mode) {
            case UNORM2_COMPOSE:
                return &allModes->comp;
            case UNORM2_DECOMPOSE:
                return &allModes->decomp;
            case UNORM2_COMPOSE_CONTIGUOUS:
                return &allModes->fcc;
            default:
                break;  // do nothing
            }
        }
    }
    if(U_SUCCESS(errorCode)) {
        // TODO: Real loading and caching...
        errorCode=U_UNSUPPORTED_ERROR;
    }
    return NULL;
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(Normalizer2)

U_NAMESPACE_END

#endif  // !UCONFIG_NO_NORMALIZATION
