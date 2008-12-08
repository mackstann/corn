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

void main_quit(void);

#endif
