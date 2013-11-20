#include "unicode/reldatefmt.h"

#include "unicode/localpointer.h"
#include "unicode/plurrule.h"
#include "unicode/msgfmt.h"
#include "unicode/numfmt.h"
#include "unicode/lrucache.h"

#include "sharedptr.h"

U_NAMESPACE_BEGIN

#define MAX_PLURAL_FORMS 6

class QualitativeUnits : public UObject {
public:
    UnicodeString data[UDAT_ABSOLUTE_COUNT][UDAT_DIRECTION_COUNT];
    virtual ~QualitativeUnits() {
    }
private:
    QualitativeUnits(const QualitativeUnits &other);
    QualitativeUnits &operator=(const QualitativeUnits& other);
};

class QuantitativeUnits : public UObject {
public:
    UnicodeString data[UDAT_RELATIVE_COUNT][2][MAX_PLURAL_FORMS];
    virtual ~QuantitativeUnits() {
    }
private:
    QuantitativeUnits(const QuantitativeUnits &other);
    QuantitativeUnits &operator=(const QuantitativeUnits& other);
};

class RelativeDateTimeData : public UObject {
public:
    RelativeDateTimeData() {
    }
    SharedPtr<QualitativeUnits> qualitativeUnits;
    SharedPtr<QuantitativeUnits> quantitativeUnits;
    SharedPtr<MessageFormat> combinedDateAndTime;
    SharedPtr<PluralRules> pluralRules;
    SharedPtr<NumberFormat> numberFormat;
    RelativeDateTimeData *clone() const {
        return new RelativeDateTimeData(*this);
    }
    virtual ~RelativeDateTimeData() {
    }
private:
    RelativeDateTimeData(const RelativeDateTimeData &other);
    RelativeDateTimeData &operator=(const RelativeDateTimeData& other);
};

RelativeDateTimeData::RelativeDateTimeData(
        const RelativeDateTimeData &other) :
        qualitativeUnits(other.qualitativeUnits),
        quantitativeUnits(other.quantitativeUnits),
        combinedDateAndTime(other.combinedDateAndTime),
        pluralRules(other.pluralRules),
        numberFormat(other.numberFormat) {
}

static LRUCache *gCache = NULL;
static UMutex gCacheMutex = U_MUTEX_INITIALIZER;
static UInitOnce gCacheInitOnce = U_INITONCE_INITIALIZER;

static UObject *U_CALLCONV createData(const char *localeId, UErrorCode &status) {
    // TODO(rocketman): Do qualitative and quantitative units.
    LocalPointer<RelativeDateTimeData> result(new RelativeDateTimeData());
    // TODO(rocketman): Change this to use CLDR data
    LocalPointer<MessageFormat> mf(new MessageFormat(UnicodeString("{0}, {1}"), localeId, status));
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (!result->combinedDateAndTime.adoptInstead(mf.orphan())) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    LocalPointer<PluralRules> pr(PluralRules::forLocale(localeId, status));
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (!result->pluralRules.adoptInstead(pr.orphan())) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }

    LocalPointer<NumberFormat> nf(NumberFormat::createInstance(localeId, status));
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (!result->numberFormat.adoptInstead(nf.orphan())) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    return result.orphan();
}

static void U_CALLCONV cacheInit(UErrorCode &status) {
    U_ASSERT(gCache == NULL);
    // TODO(tkeep): Do cleanup routine stuff
    gCache = new SimpleLRUCache(100, &gCacheMutex, &createData, status);
    if (U_FAILURE(status)) {
        delete gCache;
        gCache = NULL;
        return;
    }
}

static void getFromCache(const char *locale, SharedPtr<RelativeDateTimeData>& ptr, UErrorCode &status) {
    umtx_initOnce(gCacheInitOnce, &cacheInit, status);
    if (U_FAILURE(status)) {
        return;
    }
    gCache->get(locale, ptr, status);
}

RelativeDateTimeFormatter::RelativeDateTimeFormatter(UErrorCode& status) {
  getFromCache(Locale::getDefault().getName(), ptr, status);
}

RelativeDateTimeFormatter::RelativeDateTimeFormatter(const Locale& locale, UErrorCode& status) {
    getFromCache(locale.getName(), ptr, status);
}


RelativeDateTimeFormatter::RelativeDateTimeFormatter(const RelativeDateTimeFormatter& other) : ptr(other.ptr) {
}

RelativeDateTimeFormatter& RelativeDateTimeFormatter::operator=(const RelativeDateTimeFormatter& other) {
    if (this != &other) {
        ptr = other.ptr;
    }
    return *this;
}

RelativeDateTimeFormatter::~RelativeDateTimeFormatter() {
}


UnicodeString& RelativeDateTimeFormatter::format(
    double quantity, UDateDirection direction, UDateRelativeUnit unit,
    UnicodeString& appendTo, UErrorCode& status) const {
    return appendTo;
}

UnicodeString& RelativeDateTimeFormatter::format(
    UDateDirection direction, UDateAbsoluteUnit unit,
    UnicodeString& appendTo, UErrorCode& status) const {
    return appendTo;
}

UnicodeString& RelativeDateTimeFormatter::combineDateAndTime(
    const UnicodeString& relativeDateString, const UnicodeString& timeString,
    UnicodeString& appendTo, UErrorCode& status) const {
    return appendTo;
}

void RelativeDateTimeFormatter::setNumberFormat(const NumberFormat& nf) {
    RelativeDateTimeData *wptr = ptr.readWrite();
    NumberFormat *newNf = (NumberFormat *) nf.clone();
    if (newNf != NULL && wptr != NULL) {
        wptr->numberFormat.adoptInstead(newNf);
    }
}

U_NAMESPACE_END

