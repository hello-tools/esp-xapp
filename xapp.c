#include "xapp.h"

static const char *TAG = "xapp";

xapp_t *xapp() {
    static xapp_t app = {0};

    return &app;
}
