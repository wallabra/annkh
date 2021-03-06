#include <stdlib.h>

#include "pool.h"

#include "munit/munit.h"


void p_init_pool(struct p_list *pool, struct p_list *prev, int item_size, int capacity) {
    pool->data = malloc(item_size * capacity);
    pool->items = malloc(sizeof(struct p_item) * capacity);

    pool->item_size = item_size;
    pool->capacity = capacity;
    pool->first_free = 0;
    pool->num_used = 0;
    pool->next = NULL;

    if (prev) {
        pool->prev = prev;
        prev->next = pool;
    }

    else {
        pool->prev = NULL;
    }

    for (int i = 0; i < pool->capacity; i++) {
        pool->items[i].occupied = 0;
        pool->items[i].index = i;
        pool->items[i].pool = pool;
        pool->items[i].data = pool->data + i * pool->item_size;
    }
}


void p_free_pool(struct p_list *pool) {
    if (pool->next != NULL) {
        pool->next->prev = pool->prev;
    }

    if (pool->prev != NULL) {
        pool->prev->next = pool->next;
    }

    free(pool->items);
    free(pool->data);
}

struct p_item *p_alloc_item_in_pool(struct p_list *pool, int *index) {
    struct p_item *res = &pool->items[pool->first_free];

    if (index) {
        *index += pool->first_free;
    }

    pool->num_used++;
    res->data = pool->data + pool->first_free * pool->item_size;
    res->occupied = 1;

    if (pool->num_used == pool->capacity) {
        pool->first_free = pool->capacity;

        return res;
    }

    do {
        pool->first_free++;
    } while (pool->items[pool->first_free].occupied && pool->first_free < pool->capacity);

    return res;
}

struct p_item *p_alloc_item(struct p_list *pool, int *index) {
    if (index) {
        *index = 0;
    }

    do {
        if (pool->num_used < pool->capacity) {
            struct p_item *res = p_alloc_item_in_pool(pool, index);

            return res;
        }

        if (pool->next) {
            if (index) {
                *index += pool->capacity;
            }

            pool = pool->next;
        }
    } while (pool->next);

    if (pool->num_used == pool->capacity) {
        // pool->next is NULL
        pool->next = malloc(sizeof(struct p_list));
        p_init_pool(pool->next, pool, pool->item_size, pool->capacity);
        pool = pool->next;
    }

    return p_alloc_item_in_pool(pool, index);
}

int p_free_item(struct p_item *item) {
    struct p_list *pool = item->pool;

    if (!item->occupied) {
        return 0;
    }

    item->occupied = 0;

    pool->num_used--;

    if (item->index < pool->first_free) {
        pool->first_free = item->index;
    }

    if (pool->num_used == 0) {
        if (pool->prev) {
            pool->prev->next = pool->next;
        }

        if (pool->next) {
            munit_assert_ptr(pool->prev, !=, NULL);
            pool->next->prev = pool->prev;
        }

        if (pool->prev) {
            free(pool);
        }
        
        return 1;
    }

    return 0;
}

int p_free_at(struct p_list *pool, int which) {
    return p_free_item(p_get_item(pool, which));
}

struct p_item *p_get_item(struct p_list *pool, int which) {
    int after_segs = which / pool->capacity;
    which %= pool->capacity;

    while (after_segs) {
        after_segs--;
        pool = pool->next;

        if (!pool) {
            return NULL;
        }
    }

//#ifdef DEBUG
    munit_assert_ptr(pool->items[which].data, ==, pool->data + which * pool->item_size);
//#endif

    return &pool->items[which];
}

void p_deinit(struct p_list *pool) {
    if (pool == NULL) {
        return;
    }

    struct p_list *head = pool;

    while (head->next != NULL) {
        head = head->next;
    }

    while (head->prev) {
        free(head->items);
        free(head->data);

        head = head->prev;

        free(head->next);
    }

    free(head->items);
    free(head->data);
    free(head);
}

static void p_root_guarantee_list(struct p_root *root) {
    if (root->list == NULL) {
        root->list = malloc(sizeof(struct p_list));
        p_init_pool(root->list, NULL, root->item_size, root->capacity);

        root->head = root->list;
    }

    else if (root->head == NULL) {
        int num_segs = 0;

        for (root->head = root->list; root->head != NULL && root->head->next != NULL; root->head = root->head->next) {
            num_segs++;
        }

        root->head_index_offs = root->capacity * num_segs;
    }
}

void p_root_initialize(struct p_root *root, int item_size, int capacity) {
    root->list = NULL;
    root->head = NULL;
    root->prehead = NULL;

    root->head_index_offs = 0;
    root->capacity = capacity;
    root->item_size = item_size;
    root->num_items = 0;

    p_root_guarantee_list(root);
}

static void p_root_recoil_head(struct p_root *root) {
    root->head = root->prehead;

    if (root->head != NULL) {
        root->prehead = root->head->prev;
    }

    else {
        // frick
        root->prehead = NULL;
        root->list = NULL;
    }
}

struct p_item *p_root_alloc_item(struct p_root *root, int *index) {
    p_root_guarantee_list(root);

    struct p_item *res;

    if (root->middle_free > 0) {
        res = p_alloc_item(root->list, index);
        root->middle_free--;
    }

    else {
        res = p_alloc_item(root->head, index);
    }

    while (root->head->next != NULL) {
        root->head = root->head->next;
        root->head_index_offs += root->capacity;
    }

    root->prehead = root->head->prev;

    if (index) {
        *index += root->head_index_offs;
    }

    root->num_items++;
    return res;
}

int p_root_free_item(struct p_root *root, struct p_item *item) {
    struct p_list *pool = item->pool;

    if (item->pool != root->head) {
        root->middle_free++;
    }

    if (pool->num_used == 1) {
        root->head_index_offs -= root->capacity;

        if (pool == root->head) {
            p_root_recoil_head(root);
        }

        if (pool == root->list) {
            if (pool->next && pool->next->num_used > 0) {
                root->list = pool->next;
            }

            else {
                root->list = NULL;
            }
        }
    }

    // avoid having null pointers in a p_root
    if (root->head == NULL || root->list == NULL) {
        p_root_guarantee_list(root);
    }

    root->num_items--;

    return p_free_item(item);
}

struct p_item *p_root_get_item(struct p_root *root, int which) {
    if (root->list == NULL) {
        return NULL;
    }

    return p_get_item(root->list, which);
}

void p_root_free_at(struct p_root *root, int which) {
    if (which < root->head_index_offs) {
        root->middle_free++;
    }

    if (p_free_at(root->list, which)) {
        p_root_recoil_head(root);
        root->head_index_offs -= root->capacity;
    }

    // avoid having null pointers in a p_root
    if (root->head == NULL || root->list == NULL) {
        p_root_guarantee_list(root);
    }

    root->num_items--;
}

void p_root_empty(struct p_root *root) {
    p_deinit(root->list);

    root->head = NULL;
    root->list = NULL;
    root->prehead = NULL;

    root->middle_free = 0;
    root->head_index_offs = 0;
    root->num_items = 0;
}

int p_has(struct p_list *pool, int which) {
    return p_get_item(pool, which)->occupied;
}

int p_root_has(struct p_root *root, int which) {
    struct p_item *item = p_root_get_item(root, which);

    if (!item) {
        return 0;
    }
    
    return item->occupied;
}
