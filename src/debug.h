#ifndef DEBUG_H
#define DEBUG_H

#include <config.h>

#ifdef DEBUG
    #define D_PRINTF(...)      Serial.printf(__VA_ARGS__)
    #define D_PRINT(...)       Serial.print(__VA_ARGS__)
    #define D_PRINTLN(...)     Serial.println(__VA_ARGS__)
    #define D_HEXDUMP(p,len)   /* Later if we need, I will add implementation */
    #define D_WRITE(...)       Serial.write(__VA_ARGS__);
#else
    #define D_PRINTF(...)      ((void)0)
    #define D_PRINT(...)       ((void)0)
    #define D_PRINTLN(...)     ((void)0)
    #define D_HEXDUMP(p,len)   ((void)0)
#endif

#endif // DEBUG_H