#ifndef __main_h
#define __main_h

#include <glib.h>

typedef enum {
    QUARK_STARTING,
    QUARK_RUNNING,
    QUARK_EXITING
} QuarkStatus;

extern gboolean        main_loop_at_end;
extern gboolean        main_random_order;
extern QuarkStatus     main_status;
extern GStaticMutex    main_mutex;
extern GStaticMutex    main_fifo_mutex;
extern GStaticMutex    main_signal_mutex;

void main_quit             ();
void main_set_loop_at_end  (gboolean loop);
void main_set_random_order (gboolean random);

#define thread_lock()   (g_static_mutex_lock (&main_mutex), \
                         g_static_mutex_lock (&main_fifo_mutex), \
                         g_static_mutex_lock (&main_signal_mutex))
#define thread_unlock() (g_static_mutex_unlock (&main_mutex), \
                         g_static_mutex_unlock (&main_fifo_mutex), \
                         g_static_mutex_unlock (&main_signal_mutex))

#endif
