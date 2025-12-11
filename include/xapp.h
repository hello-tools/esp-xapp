#ifndef __XAPP_H__
#define __XAPP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "xlist.h"

#include <esp_timer.h>

#define XAPP_EVENT_NOTIFY   (1 << 0)
#define XAPP_EVENT_ASYNC    (1 << 1)
#define XAPP_EVENT_SCHEDULE (1 << 2)
// Othen App Events ... (define in your application)

typedef struct {
    void (*func)(uint8_t *data, int data_length);
    uint8_t *data;
    int data_length;
} xapp_func_data_t;

typedef struct {
    xapp_func_data_t func_data;
    uint64_t delay_us;
    int32_t flag;
    esp_timer_handle_t timer;
} xapp_schedule_data_t;

typedef struct {
    uint64_t task_stack_size;
    uint64_t task_priority;
    uint64_t schedule_interval_us;
} xapp_context_t;

typedef struct xApp {
    void *data; // User data

    TaskHandle_t task;
    EventGroupHandle_t events;
    esp_timer_handle_t schedule_timer;

    xlist_t *funcs; // functions to be executed in the main thread
    xlist_t *schedules;

    void (*loop_func)(struct xApp *app, void *data);
} xapp_t;

xapp_t *xapp_get_instance(void);
int xapp_init(xapp_t *app, void (*app_loop_func)(struct xApp *app, void *data), void *app_data, int max_count);
int xapp_start(xapp_t *app, xapp_context_t *context);
void xapp_stop(xapp_t *app);
void xapp_destroy(xapp_t *app);

void *xapp_get_data(xapp_t *app);
int xapp_call(xapp_t *app, void (*app_loop_func)(struct xApp *app, void *data), void *data, int data_length);
bool xapp_schedule_waiting(xapp_t *app, int32_t flag);
void xapp_schedule_clear(xapp_t *app, int32_t flag);
int xapp_schedule(xapp_t *app, uint64_t delay_us, int32_t flag, void (*func)(uint8_t *data, int data_len), uint8_t *data, int data_len);

#ifdef __cplusplus
}
#endif

#endif /* __XAPP_H__ */
