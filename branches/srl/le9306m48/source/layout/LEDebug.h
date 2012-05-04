#ifndef LEDEBUG_H
#define LEDEBUG_H

#ifndef LE_DEBUG
#define LE_DEBUG 0
#endif

#if LE_DEBUG
#include <stdio.h>
#define LETRACE printf("%s:%d: ",__FILE__,__LINE__);printf
#define LEMSG(x) LETRACE(x)
#else
#define LETRACE 0&&
#define LEMSG(x)
#endif



#endif
