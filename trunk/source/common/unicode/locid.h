/*
******************************************************************************
*
*   Copyright (C) 1996-2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File locid.h
*
* Created by: Helena Shih
*
* Modification History:
*
*   Date        Name        Description
*   02/11/97    aliu        Changed gLocPath to fgLocPath and added methods to
*                           get and set it.
*   04/02/97    aliu        Made operator!= inline; fixed return value of getName().
*   04/15/97    aliu        Cleanup for AIX/Win32.
*   04/24/97    aliu        Numerous changes per code review.
*   08/18/98    stephen     Added tokenizeString(),changed getDisplayName()
*   09/08/98    stephen     Moved definition of kEmptyString for Mac Port
*   11/09/99    weiv        Added const char * getName() const;
*   04/12/00    srl         removing unicodestring api's and cached hash code
******************************************************************************
*/

#ifndef LOCID_H
#define LOCID_H


#include "unicode/putil.h"

#define ULOC_LANG_CAPACITY 12
#define ULOC_COUNTRY_CAPACITY 3
#define ULOC_FULLNAME_CAPACITY 50

#ifdef XP_CPLUSPLUS

#include "unicode/unistr.h"

#ifdef ICU_LOCID_USE_DEPRECATES
typedef struct UHashtable UHashtable;
#endif

/**    
 *
 * A <code>Locale</code> object represents a specific geographical, political,
 * or cultural region. An operation that requires a <code>Locale</code> to perform
 * its task is called <em>locale-sensitive</em> and uses the <code>Locale</code>
 * to tailor information for the user. For example, displaying a number
 * is a locale-sensitive operation--the number should be formatted
 * according to the customs/conventions of the user's native country,
 * region, or culture.
 *
 * <P>
 * You create a <code>Locale</code> object using the constructor in
 * this class:
 * <blockquote>
 * <pre>
 * .      Locale( const   char*  language, 
 * .              const   char*  country, 
 * .              const   char*  variant);
 * </pre>
 * </blockquote>
 * The first argument to the constructors is a valid <STRONG>ISO
 * Language Code.</STRONG> These codes are the lower-case two-letter
 * codes as defined by ISO-639.
 * You can find a full list of these codes at a number of sites, such as:
 * <BR><a href ="http://www.ics.uci.edu/pub/ietf/http/related/iso639.txt">
 * <code>http://www.ics.uci.edu/pub/ietf/http/related/iso639.txt</code></a>
 *
 * <P>
 * The second argument to the constructors is a valid <STRONG>ISO Country
 * Code.</STRONG> These codes are the upper-case two-letter codes
 * as defined by ISO-3166.
 * You can find a full list of these codes at a number of sites, such as:
 * <BR><a href="http://www.chemie.fu-berlin.de/diverse/doc/ISO_3166.html">
 * <code>http://www.chemie.fu-berlin.de/diverse/doc/ISO_3166.html</code></a>
 *
 * <P>
 * The third constructor requires a third argument--the <STRONG>Variant.</STRONG>
 * The Variant codes are vendor and browser-specific.
 * For example, use WIN for Windows, MAC for Macintosh, and POSIX for POSIX.
 * Where there are two variants, separate them with an underscore, and
 * put the most important one first. For
 * example, a Traditional Spanish collation might be referenced, with
 * "ES", "ES", "Traditional_WIN".
 *
 * <P>
 * Because a <code>Locale</code> object is just an identifier for a region,
 * no validity check is performed when you construct a <code>Locale</code>.
 * If you want to see whether particular resources are available for the
 * <code>Locale</code> you construct, you must query those resources. For
 * example, ask the <code>NumberFormat</code> for the locales it supports
 * using its <code>getAvailableLocales</code> method.
 * <BR><STRONG>Note:</STRONG> When you ask for a resource for a particular
 * locale, you get back the best available match, not necessarily
 * precisely what you asked for. For more information, look at
 * <a href="java.util.ResourceBundle.html"><code>ResourceBundle</code></a>.
 *
 * <P>
 * The <code>Locale</code> class provides a number of convenient constants
 * that you can use to create <code>Locale</code> objects for commonly used
 * locales. For example, the following refers to a <code>Locale</code> object
 * for the United States:
 * <blockquote>
 * <pre>
 * .      Locale::US
 * </pre>
 * </blockquote>
 *
 * <P>
 * Once you've created a <code>Locale</code> you can query it for information about
 * itself. Use <code>getCountry</code> to get the ISO Country Code and
 * <code>getLanguage</code> to get the ISO Language Code. You can
 * use <code>getDisplayCountry</code> to get the
 * name of the country suitable for displaying to the user. Similarly,
 * you can use <code>getDisplayLanguage</code> to get the name of
 * the language suitable for displaying to the user. Interestingly,
 * the <code>getDisplayXXX</code> methods are themselves locale-sensitive
 * and have two versions: one that uses the default locale and one
 * that takes a locale as an argument and displays the name or country in
 * a language appropriate to that locale.
 *
 * <P>
 * The TIFC provides a number of classes that perform locale-sensitive
 * operations. For example, the <code>NumberFormat</code> class formats
 * numbers, currency, or percentages in a locale-sensitive manner. Classes
 * such as <code>NumberFormat</code> have a number of convenience methods
 * for creating a default object of that type. For example, the
 * <code>NumberFormat</code> class provides these three convenience methods
 * for creating a default <code>NumberFormat</code> object:
 * <blockquote>
 * <pre>
 * .    UErrorCode success = U_ZERO_ERROR;
 * .    Locale myLocale;
 * .    NumberFormat *nf;
 * .
 * .    nf = NumberFormat::createInstance( success );          delete nf;
 * .    nf = NumberFormat::createCurrencyInstance( success );  delete nf;
 * .    nf = NumberFormat::createPercentInstance( success );   delete nf;
 * </pre>
 * </blockquote>
 * Each of these methods has two variants; one with an explicit locale
 * and one without; the latter using the default locale.
 * <blockquote>
 * <pre>
 * .    nf = NumberFormat::createInstance( myLocale, success );          delete nf;
 * .    nf = NumberFormat::createCurrencyInstance( myLocale, success );  delete nf;
 * .    nf = NumberFormat::createPercentInstance( myLocale, success );   delete nf;
 * </pre>
 * </blockquote>
 * A <code>Locale</code> is the mechanism for identifying the kind of object
 * (<code>NumberFormat</code>) that you would like to get. The locale is
 * <STRONG>just</STRONG> a mechanism for identifying objects,
 * <STRONG>not</STRONG> a container for the objects themselves.
 *
 * <P>
 * Each class that performs locale-sensitive operations allows you
 * to get all the available objects of that type. You can sift
 * through these objects by language, country, or variant,
 * and use the display names to present a menu to the user.
 * For example, you can create a menu of all the collation objects
 * suitable for a given language. Such classes implement these
 * three class methods:
 * <blockquote>
 * <pre>
 * .      static Locale* getAvailableLocales(int32_t& numLocales)
 * .      static UnicodeString& getDisplayName(const Locale&  objectLocale,
 * .                                           const Locale&  displayLocale,
 * .                                           UnicodeString& displayName)
 * .      static UnicodeString& getDisplayName(const Locale&  objectLocale,
 * .                                           UnicodeString& displayName)
 * </pre>
 * </blockquote>
 */
class U_COMMON_API Locale 
{
public:
    /**
     * Useful constants for language.
     */
    static const Locale ENGLISH;
    static const Locale FRENCH;
    static const Locale GERMAN;
    static const Locale ITALIAN;
    static const Locale JAPANESE;
    static const Locale KOREAN;
    static const Locale CHINESE;
    static const Locale SIMPLIFIED_CHINESE;
    static const Locale TRADITIONAL_CHINESE;

    /**
     * Useful constants for country.
     */
    static const Locale FRANCE;
    static const Locale GERMANY;
    static const Locale ITALY;
    static const Locale JAPAN;
    static const Locale KOREA;
    static const Locale CHINA;      /* Alias for PRC */
    static const Locale PRC;        /* Peoples Republic of China */
    static const Locale TAIWAN;
    static const Locale UK;
    static const Locale US;
    static const Locale CANADA;
    static const Locale CANADA_FRENCH;

   /**
    * Construct an empty locale. It's only used when a fill-in parameter is
    * needed.
    * @stable
    */
    Locale(); 

    /**
     * Construct a locale from language, country, variant.
     *
     * @param language Lowercase two-letter ISO-639 code.
     * @param country  Uppercase two-letter ISO-3166 code. (optional)
     * @param variant  Uppercase vendor and browser specific code. See class
     *                 description. (optional)
     * @draft
     */
    Locale( const   char * language,
            const   char * country  = 0, 
            const   char * variant  = 0);

#ifdef ICU_LOCID_USE_DEPRECATES
    /**
     * Construct a locale from language, country, variant.
     *
     * @param language Lowercase two-letter ISO-639 code.
     * @param country  Uppercase two-letter ISO-3166 code. (optional)
     * @param variant  Uppercase vendor and browser specific code. See class
     *                 description. (optional)
     * @deprecated Remove in the first release of 2001.
     */
    Locale( const   UnicodeString&  language, 
            const   UnicodeString&  country , 
            const   UnicodeString&  variant );

     /**
     * Construct a locale from language, country.
     *
     * @param language Lowercase two-letter ISO-639 code.
     * @param country  Uppercase two-letter ISO-3166 code. (optional)
     * @deprecated Remove in the first release of 2001.
     */
    Locale( const   UnicodeString&  language, 
                            const   UnicodeString&  country );

    /**
     * Construct a locale from language.
     *
     * @param language Lowercase two-letter ISO-639 code.
     * @deprecated Remove in the first release of 2001.
     */
    Locale( const   UnicodeString&  language);


#endif /* ICU_LOCID_USE_DEPRECATES */
    /**
     * Initializes a Locale object from another Locale object.
     *
     * @param other The Locale object being copied in.
     * @stable
     */
    Locale(const    Locale& other);


    /**
     * Destructor
     * @stable
     */
    ~Locale() ;

    /**
     * Replaces the entire contents of *this with the specified value.
     *
     * @param other The Locale object being copied in.
     * @return      *this
     * @stable
     */
    Locale& operator=(const Locale& other);

    /**
     * Checks if two locale keys are the same.
     *
     * @param other The locale key object to be compared with this.
     * @return      True if the two locale keys are the same, false otherwise.
     * @stable
     */
    UBool   operator==(const    Locale&     other) const;

    /**
     * Checks if two locale keys are not the same.
     *
     * @param other The locale key object to be compared with this.
     * @return      True if the two locale keys are not the same, false
     *              otherwise.
     * @stable
     */
    UBool   operator!=(const    Locale&     other) const;

    /**
     * Common methods of getting the current default Locale. Used for the
     * presentation: menus, dialogs, etc. Generally set once when your applet or
     * application is initialized, then never reset. (If you do reset the
     * default locale, you probably want to reload your GUI, so that the change
     * is reflected in your interface.)
     *
     * More advanced programs will allow users to use different locales for
     * different fields, e.g. in a spreadsheet.
     *
     * Note that the initial setting will match the host system.
     * @system
     * @stable
     */
    static  Locale& getDefault(void);

    /**
     * Sets the default. Normally set once at the beginning of applet or
     * application, then never reset. setDefault does NOT reset the host locale.
     *
     * @param newLocale Locale to set to.
     * @system
     * @stable
     */
    static  void    setDefault(const    Locale&     newLocale,
                                                    UErrorCode&  success);

    
    /**
     * Creates a locale which has had minimal canonicalization 
     * as per uloc_getName(). 
     * @param name The name to create from
     * @return new locale object
     * @draft
     * @see uloc_getName
     */
     
    static Locale createFromName(const char *name);

    
    /**
     * Returns the locale's two-letter ISO-639 language code.
     * @return      An alias to the code
     * @draft
     */
    inline const char *  getLanguage( ) const;

    /**
     * Returns the locale's two-letter ISO-3166 country code.
     * @return      An alias to the code
     * @draft
     */
    inline const char *  getCountry( ) const;

    /**
     * Returns the locale's variant code.
     * @return      An alias to the code
     * @draft
     */
    inline const char *  getVariant( ) const;

    /**
     * Returns the programmatic name of the entire locale, with the language,
     * country and variant separated by underbars. If a field is missing, up
     * to two leading underbars will occur. Example: "en", "de_DE", "en_US_WIN",
     * "de__POSIX", "fr__MAC", "__MAC", "_MT", "_FR_EURO"
     * @return      A pointer to "name".
     */
    inline const char * getName() const;

    /**
     * returns the locale's three-letter language code, as specified
     * in ISO draft standard ISO-639-2..
     * @return      An alias to the code, or NULL
     * @draft
     */
    const char * getISO3Language() const;

    /**
     * Fills in "name" with the locale's three-letter ISO-3166 country code.
     * @return      An alias to the code, or NULL
     * @draft
     */
    const char * getISO3Country() const;

#ifdef ICU_LOCID_USE_DEPRECATES
/* begin deprecated versions */

    /**
     * Fills in "lang" with the locale's two-letter ISO-639 language code.
     * @param lang  Receives the language code.
     * @return      A reference to "lang"
     * @deprecated Remove in the first release of 2001.
     */
                UnicodeString&    getLanguage(        UnicodeString&  lang) const;
    /**
     * Fills in "cntry" with the locale's two-letter ISO-3166 country code.
     * @param cntry Receives the country code.
     * @return      A reference to "cntry".
     * @deprecated Remove in the first release of 2001.
     */
                UnicodeString&   getCountry(         UnicodeString&  cntry) const;
    /**
     * Fills in "var" with the locale's variant code.
     * @param var   Receives the variant code.
     * @return      A reference to "var".
     * @deprecated Remove in the first release of 2001.
     */
                UnicodeString&  getVariant(         UnicodeString&  var) const;

    /**
     * Fills in "name" the programmatic name of the entire locale, with the language,
     * country and variant separated by underbars. If a field is missing, at
     * most one underbar will occur. Example: "en", "de_DE", "en_US_WIN",
     * "de_POSIX", "fr_MAC"
     * @param var   Receives the programmatic locale name.
     * @return      A reference to "name".
     * @deprecated Remove in the first release of 2001.
     */
                UnicodeString&  getName(        UnicodeString&  name) const;


    /**
     * Fills in "name" with the locale's three-letter language code, as specified
     * in ISO draft standard ISO-639-2..
     * @param name  Receives the three-letter language code.
     * @param status An UErrorCode to receive any MISSING_RESOURCE_ERRORs
     * @return      A reference to "name".
     * @deprecated Remove in the first release of 2001.
     */
                UnicodeString&  getISO3Language(UnicodeString&  name, UErrorCode& status) const;

    /**
     * Fills in "name" with the locale's three-letter ISO-3166 country code.
     * @param name  Receives the three-letter country code.
     * @param status An UErrorCode to receive any MISSING_RESOURCE_ERRORs
     * @return      A reference to "name".
     * @deprecated Remove in the first release of 2001.
     */
                UnicodeString&  getISO3Country( UnicodeString&  name, UErrorCode& status) const;

/* END deprecated [ ICU_LOCID_USE_DEPRECATES ] */
#endif /* ICU_LOCID_USE_DEPRECATES */
    /**
     * Returns the Windows LCID value corresponding to this locale.
     * This value is stored in the resource data for the locale as a one-to-four-digit
     * hexadecimal number.  If the resource is missing, in the wrong format, or
     * there is no Windows LCID value that corresponds to this locale, returns 0.
     * @stable
     */
    uint32_t        getLCID(void) const;

    /**
     * Fills in "dispLang" with the name of this locale's language in a format suitable for
     * user display in the default locale.  For example, if the locale's language code is
     * "fr" and the default locale's language code is "en", this function would set
     * dispLang to "French".
     * @param dispLang  Receives the language's display name.
     * @return          A reference to "dispLang".
     * @stable
     */
    UnicodeString&  getDisplayLanguage(UnicodeString&   dispLang) const;

    /**
     * Fills in "dispLang" with the name of this locale's language in a format suitable for
     * user display in the locale specified by "inLocale".  For example, if the locale's
     * language code is "en" and inLocale's language code is "fr", this function would set
     * dispLang to "Anglais".
     * @param inLocale  Specifies the locale to be used to display the name.  In other words,
     *                  if the locale's language code is "en", passing Locale::FRENCH for
     *                  inLocale would result in "Anglais", while passing Locale::GERMAN
     *                  for inLocale would result in "Englisch".
     * @param dispLang  Receives the language's display name.
     * @return          A reference to "dispLang".
     * @stable
     */
    UnicodeString&  getDisplayLanguage( const   Locale&         inLocale,
                                                UnicodeString&  dispLang) const;

    /**
     * Fills in "dispCountry" with the name of this locale's country in a format suitable
     * for user display in the default locale.  For example, if the locale's country code
     * is "FR" and the default locale's language code is "en", this function would set
     * dispCountry to "France".
     * @param dispCountry   Receives the country's display name.
     * @return              A reference to "dispCountry".
     * @stable
     */
    UnicodeString&  getDisplayCountry(          UnicodeString& dispCountry) const;

    /**
     * Fills in "dispCountry" with the name of this locale's country in a format suitable
     * for user display in the locale specified by "inLocale".  For example, if the locale's
     * country code is "US" and inLocale's language code is "fr", this function would set
     * dispCountry to "Etats-Unis".
     * @param inLocale      Specifies the locale to be used to display the name.  In other
     *                      words, if the locale's country code is "US", passing
     *                      Locale::FRENCH for inLocale would result in "�tats-Unis", while
     *                      passing Locale::GERMAN for inLocale would result in
     *                      "Vereinigte Staaten".
     * @param dispCountry   Receives the country's display name.
     * @return              A reference to "dispCountry".
     * @stable
     */
    UnicodeString&  getDisplayCountry(  const   Locale&         inLocale,
                                                UnicodeString&  dispCountry) const;

    /**
     * Fills in "dispVar" with the name of this locale's variant code in a format suitable
     * for user display in the default locale.
     * @param dispVar   Receives the variant's name.
     * @return          A reference to "dispVar".
     * @stable
     */
    UnicodeString&  getDisplayVariant(      UnicodeString& dispVar) const;

    /**
     * Fills in "dispVar" with the name of this locale's variant code in a format
     * suitable for user display in the locale specified by "inLocale".
     * @param inLocale  Specifies the locale to be used to display the name.
     * @param dispVar   Receives the variant's display name.
     * @return          A reference to "dispVar".
     * @stable
     */
    UnicodeString&  getDisplayVariant(  const   Locale&         inLocale,
                                                UnicodeString&  dispVar) const;

    /**
     * Fills in "name" with the name of this locale in a format suitable for user display 
     * in the default locale.  This function uses getDisplayLanguage(), getDisplayCountry(),
     * and getDisplayVariant() to do its work, and outputs the display name in the format
     * "language (country[,variant])".  For example, if the default locale is en_US, then
     * fr_FR's display name would be "French (France)", and es_MX_Traditional's display name
     * would be "Spanish (Mexico,Traditional)".
     * @param name  Receives the locale's display name.
     * @return      A reference to "name".
     * @stable
     */
    UnicodeString&  getDisplayName(         UnicodeString&  name) const;

    /**
     * Fills in "name" with the name of this locale in a format suitable for user display 
     * in the locale specfied by "inLocale".  This function uses getDisplayLanguage(),
     * getDisplayCountry(), and getDisplayVariant() to do its work, and outputs the display
     * name in the format "language (country[,variant])".  For example, if inLocale is
     * fr_FR, then en_US's display name would be "Anglais (�tats-Unis)", and no_NO_NY's
     * display name would be "norv�gien (Norv�ge,NY)".
     * @param inLocale  Specifies the locale to be used to display the name.
     * @param name      Receives the locale's display name.
     * @return          A reference to "name".
     * @stable
     */
    UnicodeString&  getDisplayName( const   Locale&         inLocale,
                                            UnicodeString&  name) const;

    /**
     * Generates a hash code for the locale.
     * @stable
     */
    int32_t         hashCode(void) const;

    /**
     * Returns a list of all installed locales.
     * @param count Receives the number of locales in the list.
     * @return      A pointer to an array of Locale objects.  This array is the list
     *              of all locales with installed resource files.  The called does NOT
     *              get ownership of this list, and must NOT delete it.
     * @stable
     */
    static  const   Locale*     getAvailableLocales(int32_t& count);

    /**
     * Gets a list of all available 2-letter country codes defined in ISO 639.  This is a
     * pointer to an array of pointers to arrays of char.  All of these pointers are
     * owned by ICU-- do not delete them, and do not write through them.  The array is
     * terminated with a null pointer.
     * @return a list of all available country codes
     * @draft
     */
    static const char* const* getISOCountries();

    /**
     * Gets a list of all available language codes defined in ISO 639.  This is a pointer
     * to an array of pointers to arrays of char.  All of these pointers are owned
     * by ICU-- do not delete them, and do not write through them.  The array is
     * terminated with a null pointer.
     * @return a list of all available language codes
     * @draft
     */
    static const char* const*  getISOLanguages();


#ifdef ICU_LOCID_USE_DEPRECATES
    /**
     * Returns a list of all 2-letter country codes defined in ISO 3166.
     * Can be used to create Locales.
     * @param count Receives the number of countries in the list.
     * @return A pointer to an array of UnicodeString objects. The caller does NOT
     *  get ownership of this list, and must NOT delete it.
     * @deprecated Remove in the first release of 2001.
     */
    static const UnicodeString* getISOCountries(int32_t& count);

    /**
     * Returns a list of all 2-letter language codes defined in ISO 639.
     * Can be used to create Locales.
     * [NOTE:  ISO 639 is not a stable standard-- some languages' codes have changed.
     * The list this function returns includes both the new and the old codes for the
     * languages whose codes have changed.]
     * @param count Receives the number of languages in the list.
     * @return A pointer to an array of UnicodeString objects. The caller does NOT
     *  get ownership of this list, and must NOT delete it.
     * @deprecated Remove in the first release of 2001.
     */
    static const UnicodeString* getISOLanguages(int32_t& count);
#endif /* ICU_LOCID_NO_DEPRECATES */
    
protected: /* only protected for testing purposes. DO NOT USE. */
    /** set it from a single string. */
    void setFromPOSIXID(const char *posixID);

#ifdef ICU_LOCID_USE_DEPRECATES
    /* This never worked, and it's leaky */
    /**
     * Given an ISO country code, returns an array of Strings containing the ISO
     * codes of the languages spoken in that country.  Official languages are listed
     * in the returned table before unofficial languages, but other than that, the
     * order of the returned list is indeterminate.  If the value the user passes in
     * for "country" is not a valid ISO 316 country code, or if we don't have language
     * information for the specified country, this function returns an empty array.
     *
     * [This function is not currently part of Locale's API, but is needed in the
     * implementation.  We hope to add it to the API in a future release.]
     * @param country The ISO 2-letter country code of the desired country
     * @param count Receives the number of languages in the list.
     * @return A pointer to an array of UnicodeString objects. The caller does NOT
     *  get ownership of this list, and must NOT delete it.
     */
    static const UnicodeString* getLanguagesForCountry(const UnicodeString& country, 
                                                       int32_t& count);
#endif

private:
    /**
     * Initialize the locale object with a new name.
     * Was deprecated - used in implementation - moved internal
     *
     * @param cLocaleID The new locale name.
     */
    Locale& init(const char* cLocaleID);


    char language[ULOC_LANG_CAPACITY];
    char country[ULOC_COUNTRY_CAPACITY];
    char* variant;
    char* fullName;
    char fullNameBuffer[ULOC_FULLNAME_CAPACITY];
    
    static Locale *localeList;
    static int32_t localeListCount;

#ifdef ICU_LOCID_USE_DEPRECATES
/* Begin deprecated fields */
    static UnicodeString *isoLanguages;
    static int32_t isoLanguagesCount;
    static UnicodeString *isoCountries;
    static int32_t isoCountriesCount;

    static UHashtable *ctry2LangMapping;
    static const UnicodeString compressedCtry2LangMapping;
/* End deprecated fields */
#endif

    static Locale fgDefaultLocale;

friend  void locale_set_default_internal(const char *);

};

inline UBool
Locale::operator!=(const    Locale&     other) const
{
    return !operator==(other);
}

inline const char *
Locale::getCountry() const
{
    return country;
}

inline const char *
Locale::getLanguage() const
{
    return language;
}

inline const char *
Locale::getVariant() const
{
    return variant;
}

inline const char * 
Locale::getName() const
{
    return fullName;
}

#endif  /* XP_CPLUSPLUS */
#endif


