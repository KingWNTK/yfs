#ifndef FREE_LIST_C
#define FREE_LIST_C


#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>
#include "cache.h"
#include "hash_table.h"
#include "free_list.h"
#include "util.h"

int _num_blocks, _num_inodes, _data_blocks_start;

typedef struct free_list_node {
    int id;
    struct free_list_node *pre, *nxt;
} fl_node;

typedef struct free_list {
    fl_node head, tail;
    int size;
} free_list;

//free inode list, free block list
free_list fil, fbl;

//[l, r)
inline int is_valid_block(int id, int l, int r) {
    return id >= l && id < r;
}

//[1, r]
inline int is_valid_inode(int id, int r) {
    return id >= 1 && id <= r;
}

static void erase_behind(fl_node *a) {
    fl_node *b = a->nxt;
    fl_node *c = b->nxt;
    a->nxt = c;
    c->pre = a;
    free(b);
}

static void insert_before(fl_node *c, int id) {
    fl_node *a = c->pre;
    fl_node *b = (fl_node *)malloc(sizeof(fl_node));
    b->nxt = b->pre = NULL;
    b->id = id;
    a->nxt = b;
    b->pre = a;
    b->nxt = c;
    c->pre = b;
}

static void insert_after(fl_node *a, int id) {
    fl_node *c = a->nxt;
    fl_node *b = (fl_node *)malloc(sizeof(fl_node));
    b->nxt = b->pre = NULL;
    b->id = id;
    a->nxt = b;
    b->pre = a;
    b->nxt = c;
    c->pre = b;
}

int grab_inode() {
    if (fil.size == 0) {
        return -1;
    }
    int ret = fil.head.nxt->id;
    erase_behind(&fil.head);
    fil.size--;
    return ret;
}

int grab_block() {
    if (fbl.size == 0) {
        return -1;
    }
    int ret = fbl.head.nxt->id;
    erase_behind(&fbl.head);
    fbl.size--;
    return ret;
}

int free_inode(int inode_id) {
    if(inode_id < 1 || inode_id > _num_inodes) {
        return -1;
    }
    // insert_before(&fil.tail, inode_id);
    insert_after(&fil.head, inode_id);
    
    fil.size++;
    return 0;
}

int free_block(int block_id) {
    if(block_id < _data_blocks_start || block_id >= _num_blocks) {
        return -1;
    }
    // insert_before(&fbl.tail, block_id);
    insert_after(&fbl.head, block_id);

    fbl.size++;
    return 0;
}

void init_free_list(int num_blocks, int num_inodes) {
    _num_blocks = num_blocks;
    _num_inodes = num_inodes;

    //initialize the free inode list and the free block list
    fil.head.nxt = &fil.tail;
    fil.tail.pre = &fil.head;
    fil.head.pre = fil.tail.nxt = NULL;
    fil.size = 0;

    fbl.head.nxt = &fbl.tail;
    fbl.tail.pre = &fbl.head;
    fbl.head.pre = fbl.tail.nxt = NULL;
    fbl.size = 0;

    //we put all the block ids into a hash table
    hash_table *blocks = (hash_table *)malloc(sizeof(hash_table));
    init_ht(blocks);
    //num_inode + 1: because we have the fs_header
    int dblock_start = ((num_inodes + 1) / (BLOCKSIZE / INODESIZE)) + 1;
    _data_blocks_start = dblock_start;
    int i;

    for (i = dblock_start; i < num_blocks; i++) {
        set_ht(blocks, i, NULL);
    }

    //now we scan every inode, and see if it is free
    for (i = 1; i <= num_inodes; i++) {
        ic_entry *e = request_ic(i);
        // printf("inode id: %d, type: %d, size: %d\n", e->inode_id, e->data.type, e->data.size);

        if (e->data.type == INODE_FREE) {
            free_inode(i);
        } else {
            //erase all used blocks from the hash table
            int block_cnt = size_to_block(e->data.size);
            int lim = block_cnt > NUM_DIRECT ? NUM_DIRECT : block_cnt;
            int j;
            for (j = 0; j < lim; j++) {
                erase_ht(blocks, e->data.direct[j]);
            }

            if (block_cnt > NUM_DIRECT) {
                if (!is_valid_block(e->data.indirect, dblock_start, num_blocks)) {
                    fprintf(stderr, "invalid indirect block number, corrupted disk, failed to init\n");
                    Exit(-1);
                }
                int *d = (int *)request_bc(e->data.indirect)->data;
                for (j = 0; j < block_cnt - NUM_DIRECT; j++) {
                    if(*d == 0) {
                        //its a hole
                        d++;
                        continue;
                    }
                    if (!is_valid_block(*d, dblock_start, num_blocks)) {
                        fprintf(stderr, "invalid indirect block entry, corrupted disk, failed to init\n");
                        Exit(-1);
                    }
                    erase_ht(blocks, *d);
                    d++;
                }
            }
        }
    }

    for (i = dblock_start; i < num_blocks; i++) {
        if (has_key_ht(blocks, i)) {
            //this block is free
            free_block(i);
        }
    }

    //done initilizing list
    printf("done init free list, free blocks: %d, free inodes: %d\n", fbl.size, fil.size);
}

#endif