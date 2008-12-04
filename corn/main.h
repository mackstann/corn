#ifndef __main_h
#define __main_h

#include <glib.h>

typedef enum {
    CORN_STARTING,
    CORN_RUNNING,
    CORN_EXITING
} CornStatus;

extern gboolean main_loop_at_end;
extern gboolean main_random_order;
extern gboolean main_repeat_track;

extern CornStatus main_status;

extern GStaticMutex main_mutex;
extern GStaticMutex main_signal_mutex;

void main_quit(void);

#define thread_lock() do { g_static_mutex_lock (&main_mutex); \
                           g_static_mutex_lock (&main_signal_mutex); } while(0)
#define thread_unlock() do { g_static_mutex_unlock (&main_mutex); \
                             g_static_mutex_unlock (&main_signal_mutex); } while(0)

#endif
