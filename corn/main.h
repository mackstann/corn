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
extern guint main_time_counter;
extern gchar * main_instance_name;
extern gchar * main_service_name;

void main_quit(void);

#endif
