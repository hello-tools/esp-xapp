#ifndef __XLIST_H__
#define __XLIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct xListItem {
    void *data;
    int data_length;
    struct xListItem *next;
    struct xListItem *prev;
} xlist_item_t;

typedef struct {
    xlist_item_t *head;
    xlist_item_t *tail;
    int count;
    int max_count;
} xlist_t;   

xlist_t *xlist_create(int max_count);
void xlist_destroy(xlist_t *list);
int xlist_add(xlist_t *list, void *data, int data_length);
int xlist_remove(xlist_t *list, xlist_item_t *item);
xlist_item_t *xlist_get(xlist_t *list, int index);
int xlist_size(xlist_t *list);
int xlist_is_empty(xlist_t *list);
int xlist_is_full(xlist_t *list);
int xlist_clear(xlist_t *list);

#ifdef __cplusplus
}
#endif

#endif /* __XLIST_H__ */
