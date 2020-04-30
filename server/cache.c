#ifndef CACHE_C
#define CACHE_C

#include "cache.h"
#include "hash_table.h"
#include "stdlib.h"
#include <assert.h>
#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <string.h>

block_cache bc;
inode_cache ic;

static void insert_after(LRU_node *a, void *ptr) {
    LRU_node *b = (LRU_node *)malloc(sizeof(LRU_node));
    b->nxt = b->pre = NULL;
    b->ptr = ptr;
    LRU_node *c = a->nxt;
    a->nxt = b;
    b->pre = a;
    b->nxt = c;
    c->pre = b;
}

// static void insert_before(LRU_node *c, void *ptr) {
//     LRU_node *b = (LRU_node *)malloc(sizeof(LRU_node));
//     b->nxt = b->pre = NULL;
//     b->ptr = ptr;
//     LRU_node *a = c->pre;
//     a->nxt = b;
//     b->pre = a;
//     b->nxt = c;
//     c->pre = b;
// }

// static void erase_after(LRU_node *a) {
//     LRU_node *b = a->nxt;
//     LRU_node *c = b->nxt;
//     a->nxt = c;
//     c->pre = a;
//     free(b);
// }

static void erase_before(LRU_node *c) {
    LRU_node *b = c->pre;
    LRU_node *a = b->pre;
    a->nxt = c;
    c->pre = a;
    free(b);
}

void sync_bc(bc_entry *e) {
    WriteSector(e->block_id, e->data);
}

void evict_bc() {
    bc_entry *e = (bc_entry *)bc.tail.pre->ptr;
    printf("going to evict block: %d\n", e->block_id);
    if (e->dirty) {
        //write it back
        sync_bc(e);
    }
    //erase it from the list
    erase_before(&bc.tail);
    //erase it from the hashtable
    erase_ht(&bc.ht, e->block_id);
    //free the bc entry
    free(e);
    bc.size--;
    // assert(bc.ht.count == bc.size);
}

bc_entry *request_bc(int block_id) {
    if (has_key_ht(&bc.ht, block_id)) {
        //this block is in the cache
        LRU_node *nd = (LRU_node *)(get_ht(&bc.ht, block_id)->ptr);
        bc_entry *e = (bc_entry *)(nd->ptr);
        //move this nd to front
        nd->pre->nxt = nd->nxt;
        nd->nxt->pre = nd->pre;
        nd->nxt = bc.head.nxt;
        nd->nxt->pre = nd;
        bc.head.nxt = nd;
        nd->pre = &bc.head;
        // erase_before(nd->nxt);
        // insert_after(&bc.head, e);
        // set_ht(&bc.ht, block_id, bc.head.nxt);
        return e;
    }
    //evict a page from bc if needed
    if (bc.size == BLOCK_CACHESIZE) {
        evict_bc();
    }
    //initialize the bc entry
    bc_entry *e = (bc_entry *)malloc(sizeof(bc_entry));
    e->block_id = block_id;
    e->dirty = 0;
    ReadSector(block_id, e->data);
    //insert this entry into the list
    insert_after(&bc.head, e);
    //put this LRU node into the hash table
    set_ht(&bc.ht, block_id, bc.head.nxt);
    bc.size++;
    //we are done then
    // assert(bc.ht.count == bc.size);
    return e;
}

void sync_ic(ic_entry *e) {
    //get the corresponding block entry in the cache
    bc_entry *bc_e = request_bc(in_to_bn(e->inode_id));
    //copy the inode to the corresponding block
    int offset = e->inode_id % (BLOCKSIZE / INODESIZE);
    struct inode *iaddr = ((struct inode *)bc_e->data) + offset;
    memcpy(iaddr, &e->data, sizeof(struct inode));
    //IMPORTANT: mark this bc entry dirty
    bc_e->dirty = 1;
}

void evict_ic() {
    ic_entry *e = (ic_entry *)ic.tail.pre->ptr;
    // printf("going to evict inode: %d\n", e->inode_id);

    if (e->dirty) {
        sync_ic(e);
    }
    //erase it from the list
    erase_before(&ic.tail);
    //erase it from the hashtable
    erase_ht(&ic.ht, e->inode_id);
    //free the ic entry
    free(e);
    ic.size--;
    // assert(ic.ht.count == ic.size);
}

ic_entry *request_ic(int inode_id) {
    if (has_key_ht(&ic.ht, inode_id)) {
        //we have this inode in the cache
        LRU_node *nd = (LRU_node *)(get_ht(&ic.ht, inode_id)->ptr);
        ic_entry *e = (ic_entry *)(nd->ptr);
        //move this inode to front
        nd->pre->nxt = nd->nxt;
        nd->nxt->pre = nd->pre;
        nd->nxt = ic.head.nxt;
        nd->nxt->pre = nd;
        ic.head.nxt = nd;
        nd->pre = &ic.head;

        // erase_before(nd->nxt);
        // insert_after(&ic.head, e);
        // set_ht(&ic.ht, inode_id, ic.head.nxt);
        return e;
    }
    //evict a page from ic if needed
    if (ic.size == INODE_CACHESIZE) {
        evict_ic();
    }
    //initialize the ic entry
    ic_entry *e = (ic_entry *)malloc(sizeof(ic_entry));
    e->inode_id = inode_id;
    e->dirty = 0;
    //read the inode content from the corresponding block
    bc_entry *bc_e = request_bc(in_to_bn(inode_id));
    int offset = e->inode_id % (BLOCKSIZE / INODESIZE);
    struct inode *iaddr = ((struct inode *)bc_e->data) + offset;
    memcpy(&e->data, iaddr, sizeof(struct inode));

    //insert this entry into the list
    insert_after(&ic.head, e);
    //put this LRU node into hashtable
    set_ht(&ic.ht, inode_id, ic.head.nxt);
    ic.size++;
    // assert(ic.ht.count == ic.size);
    return e;
}

void sync_cache() {
    LRU_node *p = ic.head.nxt;
    while(p != &ic.tail) {
        if(((ic_entry *)p->ptr)->dirty) {
            sync_ic((ic_entry *)p->ptr);
        }
        p = p->nxt;
    }
    p = bc.head.nxt;
    while(p != &bc.tail) {
        if(((bc_entry *)p->ptr)->dirty) {
            sync_bc((bc_entry *)p->ptr);
        }
        p = p->nxt;
    }
}

void init_cache() {
    memset(&bc.head, 0, sizeof(bc.head));
    memset(&bc.tail, 0, sizeof(bc.tail));
    bc.head.nxt = &bc.tail;
    bc.tail.pre = &bc.head;
    bc.size = 0;
    init_ht(&bc.ht);

    memset(&ic.head, 0, sizeof(ic.head));
    memset(&ic.tail, 0, sizeof(ic.tail));
    ic.head.nxt = &ic.tail;
    ic.tail.pre = &ic.head;
    init_ht(&ic.ht);
}

#endif
