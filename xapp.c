#include "xapp.h"
#include <esp_event.h>

static const char *TAG = "xapp";

typedef struct {
    xapp_t *app;
    xapp_schedule_data_t *schedule_data;
} schedule_msg_t;

static void free_func_data(xapp_func_data_t *func_data) {
    if (func_data == NULL) {
        return;
    }
    if (func_data->data != NULL) {
        free(func_data->data);
    }
}

static void remove_schedule_func_item(xapp_t *app, xlist_item_t *item) {
    if (item == NULL) {
        return;
    }

    if (item->data == NULL) {
        xlist_remove(app->schedules, item);
        return;
    }

    xapp_schedule_data_t *schedule_data = (xapp_schedule_data_t *)item->data;
    // Stop and delete timer
    if (schedule_data->timer != NULL) {
        esp_timer_stop(schedule_data->timer);
        esp_timer_delete(schedule_data->timer);
        schedule_data->timer = NULL;
    }
    // Remove item from list
    xlist_remove(app->schedules, item);
}

xapp_t *xapp_get_instance() {
    static xapp_t app = { 0 };

    return &app;
}

static void destroy_app(xapp_t *app) {
    if (app == NULL) {
        return;
    }

    if (app->task != NULL) {
        vTaskDelete(app->task);
        app->task = NULL;
    }

    if (app->schedule_timer != NULL) {
        esp_timer_stop(app->schedule_timer);
        esp_timer_delete(app->schedule_timer);
        app->schedule_timer = NULL;
    }

    if (app->funcs != NULL) {
        xlist_destroy(app->funcs);
        app->funcs = NULL;
    }

    if (app->schedules != NULL) {
        xlist_destroy(app->schedules);
        app->schedules = NULL;
    }

    if (app->events != NULL) {
        vEventGroupDelete(app->events);
        app->events = NULL;
    }
}

int xapp_init(xapp_t *app, void (*app_loop_func)(struct xApp *app, void *data), void *app_data, int max_count) {
    /* App User Data */
    app->data = app_data;

    /* App Loop Function */
    app->loop_func = app_loop_func;

    /* Esp Event Loop for App Events */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* NVS Flash */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Event Group for App Events */
    app->events = xEventGroupCreate();

    /* Schedule Timer */
    app->schedule_timer = NULL;

    /* Functions List */
    app->funcs = xlist_create(max_count);
    if (app->funcs == NULL) {
        ESP_LOGE(TAG, "Create functions list failed");
        destroy_app(app);
        return -1;
    }

    /* Schedule Functions List */
    app->schedules = xlist_create(max_count);
    if (app->schedules == NULL) {
        ESP_LOGE(TAG, "Create schedule functions list failed");
        destroy_app(app);
        return -1;
    }

    return 0;
}

static void on_schedule_tmr(void *arg) {
    xapp_t *app = (xapp_t *)arg;

    xEventGroupSetBits(app->events, XAPP_EVENT_SCHEDULE);
}

static void dispatch_funcs(xapp_t *app) {
    if (app == NULL) {
        return;
    }

    if (xlist_is_empty(app->funcs)) {
        return;
    }

    int count = xlist_size(app->funcs);
    for (int i = 0; i < count; i++) {
        xlist_item_t *item = xlist_get(app->funcs, 0);
        if (item == NULL) {
            continue;
        }
        xapp_func_data_t *func_data = (xapp_func_data_t *)item->data;
        if (func_data == NULL) {
            xlist_remove(app->funcs, item);
            continue;
        }
        func_data->func(func_data->data, func_data->data_length);
        free_func_data(func_data);
        xlist_remove(app->funcs, item);
    }
}

static void app_loop(void *arg) {
    xapp_t *app = (xapp_t *)arg;

    while (1) {
        uint32_t bits = xEventGroupWaitBits(app->events,
                                            XAPP_EVENT_NOTIFY |
                                                XAPP_EVENT_ASYNC |
                                                XAPP_EVENT_SCHEDULE,
                                            pdTRUE, pdFALSE, portMAX_DELAY);

        if (app->loop_func != NULL) {
            app->loop_func(app, bits);
        }

        if (bits & XAPP_EVENT_ASYNC) {
            dispatch_funcs(app->funcs);
        }
    }
}

int xapp_start(xapp_t *app, xapp_context_t *context) {
    uint64_t task_stack_size = 4096;
    uint64_t task_priority = 3;
    uint64_t schedule_interval_us = 1000 * 1000ULL;
    esp_err_t ret = ESP_OK;

    if (app == NULL) {
        return -1;
    }

    if (context != NULL) {
        if (context->task_stack_size > 0) {
            task_stack_size = context->task_stack_size;
        }
        if (context->task_priority > 0) {
            task_priority = context->task_priority;
        }
        if (context->schedule_interval_us > 0) {
            schedule_interval_us = context->schedule_interval_us;
        }
        app->using_soft_timer = context->using_soft_timer;
    }

    /* Create App Task */
    xTaskCreate(app_loop, "xapp", task_stack_size, app, task_priority, app->task);
    if (app->task == NULL) {
        ESP_LOGE(TAG, "Create app task failed");
        destroy_app(app);
        return -1;
    }

    /* Create Schedule Timer */
    const esp_timer_create_args_t timer_args = {
        .callback = &on_schedule_tmr,
        .name = "xapp schedule",
        .arg = app,
    };
    ret = esp_timer_create(&timer_args, &app->schedule_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create schedule timer failed: %s", esp_err_to_name(ret));
        destroy_app(app);
        return -1;
    }
    esp_timer_start_periodic(app->schedule_timer, schedule_interval_us);
    return 0;
}

void xapp_stop(xapp_t *app) {
    if (app == NULL) {
        return;
    }
    if (app->schedule_timer != NULL) {
        esp_timer_stop(app->schedule_timer);
        esp_timer_delete(app->schedule_timer);
        app->schedule_timer = NULL;
    }
    if (app->task != NULL) {
        vTaskDelete(app->task);
        app->task = NULL;
    }
}

void xapp_destroy(xapp_t *app) {
    destroy_app(app);
}

void *xapp_get_data(xapp_t *app) {
    if (app == NULL) {
        return NULL;
    }

    return app->data;
}

static int add_func_to_list(xapp_t *app, void (*app_loop_func)(struct xApp *app, void *data), void *data, int data_length) {
    if (xlist_is_full(app->funcs)) {
        ESP_LOGE(TAG, "Functions list is full");
        return -1;
    }

    /* Allocate function data */
    xapp_func_data_t func_data = { 0 };
    func_data.func = app_loop_func;
    func_data.data_length = data_length;
    func_data.data = NULL;
    if (data_length > 0) {
        func_data.data = (uint8_t *)malloc(data_length);
        if (func_data.data == NULL) {
            ESP_LOGE(TAG, "Allocate data failed");
            return -1;
        }
        memcpy(func_data.data, data, data_length);
    }
    if (xlist_add(app->funcs, &func_data, sizeof(xapp_func_data_t)) < 0) {
        ESP_LOGE(TAG, "Add function to functions list failed");
        if (func_data.data != NULL) {
            free(func_data.data);
        }
        return -1;
    }
    return 0;
}

int xapp_call(xapp_t *app, void (*app_loop_func)(struct xApp *app, void *data), void *data, int data_length) {
    if (app == NULL || func == NULL) {
        return -1;
    }

    /* Add function to functions list */
    if (add_func_to_list(app, app_loop_func, data, data_length) < 0) {
        return -1;
    }

    xEventGroupSetBits(app->events, XAPP_EVENT_ASYNC);
    return 0;
}

bool xapp_schedule_waiting(xapp_t *app, int32_t flag) {
    if (app == NULL || flag <= 0) {
        return false;
    }

    if (xlist_is_empty(app->schedules)) {
        return false;
    }

    int count = xlist_size(app->schedules);
    xlist_item_t *item = NULL;
    xapp_schedule_data_t *schedule_data = NULL;
    for (int i = 0; i < count; i++) {
        item = xlist_get(app->schedules, 0);
        schedule_data = (xapp_schedule_data_t *)item->data;
        if (schedule_data->flag == flag) {
            return true;
        }
    }
    return false;
}

void xapp_schedule_clear(xapp_t *app, int32_t flag) {
    if (app == NULL || flag <= 0) {
        return;
    }

    if (xlist_is_empty(app->schedules)) {
        return;
    }

    int count = xlist_size(app->schedules);
    xlist_item_t *item = NULL;
    xapp_schedule_data_t *schedule_data = NULL;
    for (int i = 0; i < count; i++) {
        item = xlist_get(app->schedules, 0);
        schedule_data = (xapp_schedule_data_t *)item->data;
        if (schedule_data->flag == flag) {
            free_func_data(&schedule_data->func_data);
            remove_schedule_func_item(app, item);
            return;
        }
    }
}

static void on_schedule_func_tmr(void *arg) {
    schedule_msg_t *msg = (schedule_msg_t *)arg;
    if (msg == NULL) {
        return;
    }

    xapp_t *app = msg->app;
    xapp_schedule_data_t *schedule_data = msg->schedule_data;
    if (schedule_data == NULL) {
        free(msg);
        return;
    }

    if (xapp_call(app, schedule_data->func_data.func, schedule_data->func_data.data, schedule_data->func_data.data_length) < 0) {
        ESP_LOGE(TAG, "Call schedule function failed");
        free_func_data(&schedule_data->func_data);
    }

    remove_schedule_func_item(app, msg->schedule_data);
    free(msg);
}

int xapp_schedule(xapp_t *app, uint64_t delay_us, int32_t flag, void (*func)(uint8_t *data, int data_len), uint8_t *data, int data_len) {
    if (app == NULL || func == NULL || delay_us <= 0) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    if (xlist_is_full(app->schedules)) {
        ESP_LOGE(TAG, "Schedule functions list is full");
        return -1;
    }

    /* Allocate schedule function data */
    xapp_schedule_data_t schedule_data = {
        .func_data = {
            .func = func,
            .data_length = data_len,
            .data = NULL,
        },
        .delay_us = delay_us,
        .flag = flag,
        .timer = NULL,
    };
    if (data_len > 0) {
        schedule_data.func_data.data = (uint8_t *)malloc(data_len);
        if (schedule_data.func_data.data == NULL) {
            ESP_LOGE(TAG, "Allocate data failed");
            free_func_data(&schedule_data.func_data);
            return -1;
        }
        memcpy(schedule_data.func_data.data, data, data_len);
    }

    /* Create schedule message */
    schedule_msg_t *msg = (schedule_msg_t *)malloc(sizeof(schedule_msg_t));
    if (msg == NULL) {
        ESP_LOGE(TAG, "Allocate schedule message failed");
        free_func_data(&schedule_data.func_data);
        return -1;
    }
    msg->app = app;
    msg->schedule_data = NULL;

    /* Create schedule timer */
    const esp_timer_create_args_t timer_args = {
        .callback = &on_schedule_tmr,
        .name = "xapp schedule",
        .arg = msg,
    };
    ret = esp_timer_create(&timer_args, &schedule_data.timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create schedule timer failed: %s", esp_err_to_name(ret));
        free_func_data(&schedule_data.func_data);
        return -1;
    }

    /* Add schedule function to schedule functions list */
    msg->schedule_data = xlist_add(app->schedules, &schedule_data, sizeof(xapp_schedule_data_t));
    if (msg->schedule_data == NULL) {
        ESP_LOGE(TAG, "Add schedule function to schedule functions list failed");
        if (schedule_data.func_data.data != NULL) {
            free(schedule_data.func_data.data);
        }
        return -1;
    }

    /* Start schedule timer */
    esp_timer_start_once(schedule_data.timer, delay_us);

    return 0;
}
