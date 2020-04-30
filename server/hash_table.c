#ifndef HASHTABLE_C
#define HASHTABLE_C

#include "hash_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void insert_after(hash_entry *a, int key, void *ptr) {
    hash_entry *b = (hash_entry *)malloc(sizeof(hash_entry));
    b->key = key;
    b->ptr = ptr;
    b->pre = b->nxt = NULL;

    hash_entry *c = a->nxt;
    a->nxt = b;
    b->pre = a;
    if (c != NULL) {
        b->nxt = c;
        c->pre = b;
    }
}

static void erase_after(hash_entry *a) {
    hash_entry *b = a->nxt;
    if (b == NULL)
        return;
    hash_entry *c = b->nxt;
    a->nxt = c;
    if (c != NULL) {
        c->pre = a;
    }
    free(b);
}

static ui hash(int key, int hash_size) {
    //make it unsigned so that we can hash negative int as well
    ui ret = (ui)key;
    return ret % hash_size;
}

int set_ht(hash_table *ht, int key, void *ptr) {
    ui hv = hash(key, ht->hash_size);
    if (has_key_ht(ht, key)) {
        //we have this key in our hash table, update the ptr value then
        hash_entry *p = ht->data[hv].nxt;
        while (p) {
            if (p->key == key) {
                p->ptr = ptr;
                break;
            }
            p = p->nxt;
        }
        return 1;
    }
    insert_after(&ht->data[hv], key, ptr);
    ht->count++;
    return 1;
}

int has_key_ht(hash_table *ht, int key) {
    ui hv = hash(key, ht->hash_size);
    hash_entry *p = ht->data[hv].nxt;
    while (p) {
        if (p->key == key) {
            return 1;
        }
        p = p->nxt;
    }
    return 0;
}

int erase_ht(hash_table *ht, int key) {
    ui hv = hash(key, ht->hash_size);
    hash_entry *p = ht->data[hv].nxt;
    while (p) {
        if (p->key == key) {
            erase_after(p->pre);
            ht->count--;
            return 1;
        }
        p = p->nxt;
    }

    //we don't have this key in our table, can't erase anything, return 0
    return 0;
}

hash_entry *get_ht(hash_table *ht, int key) {
    ui hv = hash(key, ht->hash_size);
    hash_entry *p = ht->data[hv].nxt;
    while (p) {
        if (p->key == key) {
            return p;
        }
        p = p->nxt;
    }
    return NULL;
}

int clear_ht(hash_table *ht) {
    int i;
    for (i = 0; i < ht->hash_size; i++) {
        hash_entry *p = ht->data[i].nxt;
        while (p) {
            hash_entry *nxt = p->nxt;
            erase_after(p->pre);
            ht->count--;
            p = nxt;
        }
    }
    free(ht->data);
    return 0;
}

int init_ht(hash_table *ht) {
    //499 is an resonably small prime number we use to hash the integer keys
    ht->hash_size = 499;
    ht->data = (hash_entry *)malloc(ht->hash_size * sizeof(hash_entry));
    //set it to all 0
    memset(ht->data, 0, ht->hash_size * sizeof(hash_entry));
    return 0;
}

#endif