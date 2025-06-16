#ifndef COROUTINE_C_LIST_H
#define COROUTINE_C_LIST_H

#include "lang_items.h"

/* List */
struct node {
    void *data;
    struct node *next;
    struct node *prev;
};

struct list {
    struct node *head;
    struct node *tail;
};

void list_init(struct list *list) {
    list->head = (struct node *) malloc(sizeof(struct node));
    list->tail = (struct node *) malloc(sizeof(struct node));
    if (!list->head || !list->tail) {
        panic("malloc struct_co_node failed");
        return;
    }
    list->head->data = NULL;
    list->tail->data = NULL;
    list->head->next = list->tail;
    list->head->prev = NULL;
    list->tail->next = NULL;
    list->tail->prev = list->head;
}

int list_inited(struct list *list) {
    if (!list || !list->head || !list->tail) {
        return 0;
    }
    return 1;
}

void list_destroy(struct list *list) {
    if (!list) {
        panic("list is NULL");
        return;
    }
    struct node *node = list->head;
    while (node) {
        struct node *next = node->next;
        free(node);
        node = next;
    }
}

void list_push_back(struct list *list, void *co) {
    if (!list_inited(list)) {
        panic("list has not been initialized");
        return;
    }
    struct node *node = (struct node *) malloc(sizeof(struct node));
    if (!node) {
        panic("malloc struct_co_node failed");
        return;
    }
    node->data = co;
    node->next = list->tail;
    node->prev = list->tail->prev;
    list->tail->prev->next = node;
    list->tail->prev = node;
}

void *list_pop_front(struct list *list) {
    if (!list_inited(list)) {
        panic("list has not been initialized");
        return NULL;
    }
    struct node *node = list->head->next;
    if (node == list->tail) {
        return NULL;
    }
    list->head->next = node->next;
    node->next->prev = list->head;
    void *co = node->data;
    free(node);
    return co;
}

int list_erase(struct list *list, void *co) {
    if (!list_inited(list)) {
        panic("list has not been initialized");
        return 0;
    }
    struct node *node = list->head->next;
    while (1)  {
        if (node->data == NULL) return 0;
        if (node->data == co) {
            node->prev->next = node->next;
            node->next->prev = node->prev;
            free(node);
            return 1;
        }
        node = node->next;
    }
}

#endif //COROUTINE_C_LIST_H
