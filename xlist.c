#include <stdio.h>
#include "xlist.h"

#include <string.h>
#include <stdlib.h>

xlist_t *xlist_create(int max_count) {
    xlist_t *list = (xlist_t *)malloc(sizeof(xlist_t));
    if (list == NULL) {
        return NULL;
    }

    if (max_count <= 0) {
        free(list);
        return NULL;
    }

    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    list->max_count = max_count;
    return list;
}

static xlist_item_t *create_item(void *data, int data_length) {
    xlist_item_t *item = (xlist_item_t *)calloc(1, sizeof(xlist_item_t));
    if (item == NULL) {
        return NULL;
    }

    item->data_length = data_length;
    if (data_length > 0) {
        item->data = (uint8_t *)calloc(1, data_length);
        if (item->data == NULL) {
            free(item);
            return NULL;
        }
        memcpy(item->data, data, data_length);
    }
    item->next = NULL;
    item->prev = NULL;

    return item;
}

void free_item(xlist_item_t *item) {
    if (item == NULL) {
        return;
    }
    if (item->data != NULL) {
        free(item->data);
    }
    free(item);
}

void xlist_destroy(xlist_t *list) {
    if (list == NULL) {
        return;
    }

    xlist_item_t *current = list->head;
    while (current != NULL) {
        xlist_item_t *next = current->next;
        free(current);
        current = next;
    }
    free(list);
}

xlist_item_t *xlist_add(xlist_t *list, void *data, int data_length) {
    if (list == NULL) {
        return NULL;
    }
    if (list->count >= list->max_count) {
        return NULL;
    }

    xlist_item_t *new_item = create_item(data, data_length);
    if (new_item == NULL) {
        return NULL;
    }

    if (list->tail == NULL) {
        list->head = new_item;
        list->tail = new_item;
    } else {
        new_item->prev = list->tail;
        list->tail->next = new_item;
        list->tail = new_item;
    }
    list->count++;
    return new_item;
}

int xlist_remove(xlist_t *list, xlist_item_t *item) {
    if (list == NULL || item == NULL) {
        return -1;
    }

    if (list->count == 0) {
        return 0;
    }

    if (item == list->head) {
        if (list->head == list->tail) {
            list->head = NULL;
            list->tail = NULL;
        } else {
            list->head = list->head->next;
            list->head->prev = NULL;
        }
    } else if (item == list->tail) {
        if (list->head == list->tail) {
            list->head = NULL;
            list->tail = NULL;
        } else {
            list->tail = list->tail->prev;
            list->tail->next = NULL;
        }
    } else {
        item->prev->next = item->next;
        item->next->prev = item->prev;
    }
    list->count--;
    free_item(item);
    return 0;
}

xlist_item_t *xlist_get(xlist_t *list, int index) {
    if (list == NULL || index < 0 || index >= list->count) {
        return NULL;
    }

    xlist_item_t *current = list->head;
    for (int i = 0; i < index; i++) {
        current = current->next;
    }
    return current;
}

int xlist_size(xlist_t *list) {
    if (list == NULL) {
        return 0;
    }
    return list->count;
}

int xlist_is_empty(xlist_t *list) {
    if (list == NULL) {
        return 0;
    }
    return list->count == 0;
}

int xlist_is_full(xlist_t *list) {
    if (list == NULL) {
        return 0;
    }
    return list->count >= list->max_count;
}

int xlist_clear(xlist_t *list) {
    if (list == NULL) {
        return -1;
    }

    while (list->head != NULL) {
        xlist_item_t *current = list->head;
        list->head = list->head->next;
        free_item(current);
    }

    list->tail = NULL;
    list->count = 0;
    return 0;
}
