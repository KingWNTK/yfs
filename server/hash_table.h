#ifndef HASHTABLE_H
#define HASHTABLE_H

typedef unsigned int ui;

typedef struct hash_entry {
    int key;
    void *ptr;
    struct hash_entry *pre, *nxt;
}hash_entry;

typedef struct hash_table{
    int hash_size;
    int count;
    hash_entry *data;
}hash_table;

int set_ht(hash_table *ht, int key, void *ptr);

int erase_ht(hash_table *ht, int key);

hash_entry *get_ht(hash_table *ht, int key);

int has_key_ht(hash_table *ht, int key);

int clear_ht(hash_table *ht);

int init_ht(hash_table *ht);

#endif