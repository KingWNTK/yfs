#ifndef CACHE_H
#define CACHE_H

#include <comp421/filesystem.h>
#include "hash_table.h"

#define in_to_bn(x) (((x) / (BLOCKSIZE / INODESIZE) + 1))

typedef struct block_cache_entry {
    int block_id;
    int dirty;
    char data[BLOCKSIZE];
}bc_entry;

typedef struct inode_cache_entry {
    int inode_id;
    int dirty;
    struct inode data;
}ic_entry;

typedef struct LRU_node{
    void *ptr;
    struct LRU_node *pre, *nxt;
}LRU_node;

typedef struct block_cache {
    LRU_node head, tail;
    int size;
    hash_table ht;
}block_cache;

typedef struct inode_cache {
    LRU_node head, tail;
    int size;
    hash_table ht;
}inode_cache;

extern block_cache bc;
extern inode_cache ic;

void init_cache();

bc_entry *request_bc(int block_id);

ic_entry *request_ic(int inode_id);

void sync_cache();

#endif