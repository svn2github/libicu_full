// uts46demo.cpp
// 2010mar12
// Markus Scherer
#include <stdio.h>
#include <string>
#include "unicode/utypes.h"
#include "unicode/idna.h"
#include "unicode/localpointer.h"
#include "unicode/unistr.h"

static UChar hex(int32_t h) {
    h&=0xf;
    return h<=9 ? 0x30+h : (0x41-10)+h;
}

static icu::UnicodeString &
escapeControls(icu::UnicodeString &s) {
    for(int32_t i=0; i<s.length(); ++i) {
        UChar c=s[i];
        if(c<=0x1f || c==0x7f || c==0x200c || c==0x200d) {
          // "\\uhhhh"
          s.replace(i, 1, icu::UnicodeString().append(0x5c).append(0x75).
                          append(hex(c>>12)).append(hex(c>>8)).append(hex(c>>4)).append(hex(c)));
          i+=5;
        }
    }
    return s;
}

extern int
main(int argc, const char *argv[]) {
    if(argc<=1) { return 1; }
    uint32_t options=0;
    const char *optionsString=argv[1];
    if(argc>=3 && *optionsString++=='-') {
        ++argv;
        char c;
        while((c=*optionsString++)!=0) {
            switch(c) {
            case '3': options|=UIDNA_USE_STD3_RULES; break;
            case 'b': options|=UIDNA_CHECK_BIDI; break;
            case 'j': options|=UIDNA_CHECK_CONTEXTJ; break;
            case 'n': options|=UIDNA_NONTRANSITIONAL_TO_ASCII; break;
            default: return 2;
            }
        }
    }
    puts("have options");
    icu::UnicodeString input=icu::UnicodeString::fromUTF8(argv[1]).unescape();
    icu::UnicodeString ascii, unicode;
    uint32_t toASCIIErrors=0, toUnicodeErrors=0;
    UErrorCode errorCode=U_ZERO_ERROR;
    icu::LocalPointer<icu::IDNA> idna(icu::IDNA::createUTS46Instance(options, errorCode));
    idna->nameToASCII(input, ascii, toASCIIErrors, errorCode);
    idna->nameToUnicode(input, unicode, toUnicodeErrors, errorCode);
    if(U_FAILURE(errorCode)) {
        return 3;
    }
    puts("did IDNA");
    std::string utf8;
    printf("toASCII:   \"%s\"  errors 0x%04lX\n",
           escapeControls(ascii).toUTF8String(utf8).c_str(), (long)toASCIIErrors);
    printf("toUnicode: \"%s\"  errors 0x%04lX\n",
           escapeControls(unicode).toUTF8String(utf8).c_str(), (long)toUnicodeErrors);
    return 0;
}
