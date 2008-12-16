#ifndef __main_h
#define __main_h

#include <glib.h>

typedef enum
{
    CORN_STARTING,
    CORN_RUNNING,
    CORN_EXITING
} CornStatus;

extern CornStatus main_status;

void main_quit(void);

#endif
