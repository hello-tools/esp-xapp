#ifndef __XAPP_H__
#define __XAPP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "xlist.h"

typedef struct {
    xlist_t list;
} xapp_t;

xapp_t *xapp();

#ifdef __cplusplus
}
#endif

#endif /* __XAPP_H__ */
