#ifndef __corn_debug_h__
#define __corn_debug_h__

#include <sys/time.h>
#include <time.h>
#include <glib.h>
#define LOG_TIME(str) do { \
        struct timeval tv; gettimeofday(&tv, NULL); \
        g_message("%d.%06d: %s", (int)tv.tv_sec, (int)tv.tv_usec, (str)); \
    } while(0)

#endif

