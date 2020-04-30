#ifndef UTIL_C
#define UTIL_C

#include "util.h"
#include "cache.h"
#include "free_list.h"
#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITER_NOT_EMPTY 1
#define ITER_ANY 2
#define ITER_EMPTY 3

typedef struct dir_entry_iterator {
    int inode_id;
    int block_idx;
    int block_id;
    int block_offset;
} dir_entry_iterator;

static inline void block_id_check(int block_id) {
    if (!is_valid_block(block_id, _data_blocks_start, _num_blocks)) {
        //broken disk, nothing we can do
        fprintf(stderr, "num blocks: %d, block_id: %d->", _num_blocks, block_id);
        fprintf(stderr, "broken disk\n");
        Exit(-1);
    }
}

/**
 * Check if a and b are same, note that b is bounded by len
 */
static int is_same_dir_name(char *a, char *b, int len) {
    int i;
    for (i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }

    for (i = len; i < DIRNAMELEN; i++) {
        if (a[i] != '\0') {
            return 0;
        }
    }
    return 1;
}

/**
 * Get the iterator according to the inode_id
 */
void get_iterator(dir_entry_iterator *iter, int inode_id) {
    iter->inode_id = inode_id;
    iter->block_idx = -1;
    iter->block_id = -1;
    iter->block_offset = -1;
}

/**
 * Check if the iter has next dir_entry, return 1 if ok, 0 its' reached the end
 */
int has_next(dir_entry_iterator *iter) {
    ic_entry *ic_e = request_ic(iter->inode_id);
    if (iter->block_idx == -1) {
        //this is a brand new iterator
        if (ic_e->data.size < (int)sizeof(dir_entry)) {
            //empty directory
            return 0;
        }
        iter->block_idx = 0;
        iter->block_id = ic_e->data.direct[0];
        block_id_check(iter->block_id);
        iter->block_offset = 0;
    }

    // printf("inode: %d, block_idx: %d, block_offset: %d\n", iter->inode_id, iter->block_idx, iter->block_offset);
    if ((int)(iter->block_idx * BLOCKSIZE + iter->block_offset + sizeof(dir_entry)) <= ic_e->data.size) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * IMPORTANT:must call has_next before calling get_next !!
 * 
 * The return value of this get_next function is the pointer to cache.
 * We should not keep this value for a long time, if we are going to visit other blocks
 */
dir_entry *get_next(dir_entry_iterator *iter) {
    ic_entry *ic_e = request_ic(iter->inode_id);
    // printf("inode: %d, num blocks: %d\n", iter->inode_id, numb);
    if (iter->block_offset + sizeof(dir_entry) > BLOCKSIZE) {
        //need to switch to another block
        iter->block_idx++;
        iter->block_offset = 0;

        if (iter->block_idx < NUM_DIRECT) {
            iter->block_id = ic_e->data.direct[iter->block_idx];
        } else {
            int delta = iter->block_idx - NUM_DIRECT;
            bc_entry *bc_e = request_bc(ic_e->data.indirect);
            iter->block_id = *(((int *)bc_e->data) + delta);
        }
        block_id_check(iter->block_id);
    }

    bc_entry *bc_e = request_bc(iter->block_id);
    dir_entry *ret = (dir_entry *)(bc_e->data + iter->block_offset);
    //remember to increase the offset
    iter->block_offset += sizeof(dir_entry);
    return ret;
}

/**
 * Mark the iterator dirty
 */
void wrote_iter(dir_entry_iterator *iter) {
    bc_entry *bc_e = request_bc(iter->block_id);
    bc_e->dirty = 1;
}

void print_dir(int inode_id) {
    dir_entry_iterator iter;
    get_iterator(&iter, inode_id);
    while (has_next(&iter)) {
        dir_entry *e = get_next(&iter);
        if (e->inum) {
            printf("%d %s\n", e->inum, e->name);
        }
    }
}

void print_tree(int rt, int pa, int depth) {
    dir_entry_iterator iter;
    get_iterator(&iter, rt);
    int i;
    while (has_next(&iter)) {
        dir_entry *e = get_next(&iter);
        if (e->inum == 0) {
            continue;
        }
        for (i = 0; i < depth; i++) {
            printf("-");
        }
        printf("%s nlink: %d\n", e->name, request_ic(e->inum)->data.nlink);
        if (e->inum != rt && e->inum != pa && request_ic(e->inum)->data.type == INODE_DIRECTORY) {
            print_tree(e->inum, rt, depth + 1);
        }
    }
}

/**
 * Return the inode number if we found the dir, otherwise return 0
 */
int find_dir_entry(int inode_id, char *name, int len) {

    //empty name, simply return 0
    if (len == 0)
        return 0;

    ic_entry *ic_e = request_ic(inode_id);

    if (ic_e->data.type != INODE_DIRECTORY) {
        //this is not a directory, impossible to find directory entry
        return 0;
    }

    dir_entry_iterator iter;
    get_iterator(&iter, inode_id);
    while (has_next(&iter)) {
        dir_entry *e = get_next(&iter);
        if (e->inum == 0) {
            //this entry is not valid
            continue;
        }
        if (is_same_dir_name(e->name, name, len)) {
            //found it
            return e->inum;
        }
    }
    return 0;
}

/**
 * append a new dir_entry into the directory represented by inode_id
 * if any error(running out of blocks), return -1
 */
int append_dir_entry(int inode_id, int inum, char *name, int len) {
    //IMPORTANT: mark cache dirty if any change
    dir_entry de;
    de.inum = inum;
    memset(de.name, 0, DIRNAMELEN);
    memcpy(de.name, name, len);

    // printf("name: %.*s\n", len, name);

    ic_entry *ic_e = request_ic(inode_id);
    int numb = size_to_block(ic_e->data.size);

    if ((int)(numb * BLOCKSIZE) < (int)(ic_e->data.size + sizeof(dir_entry))) {
        //need a new block
        if (numb < NUM_DIRECT) {
            int b = grab_block();
            if (b == -1) {
                //no more blocks
                return -1;
            }
            //can assign a block directly
            ic_e->data.direct[numb] = b;
            bc_entry *bc_e = request_bc(b);
            *(dir_entry *)(bc_e->data) = de;
            //IMPORTANT: mark them dirty
            bc_e->dirty = 1;
            ic_e->dirty = 1;
        } else {
            int ib = -1;
            if (numb == NUM_DIRECT) {
                ib = grab_block();
                if (ib == -1) {
                    //no more blocks
                    return -1;
                }
                ic_e->data.indirect = ib;
                ic_e->dirty = 1;
            }
            bc_entry *ibc_e = request_bc(ic_e->data.indirect);
            int b = grab_block();
            if (b == -1) {
                //no more blocks
                //important to free ib here
                if (ib != -1) {
                    free_block(ib);
                }
                return -1;
            }

            if (numb - NUM_DIRECT >= BLOCKSIZE / sizeof(int)) {
                //can't put it into indirect block
                free_block(b);
                free_block(ib);
                return -1;
            }

            //write the new block into indirect block
            *((int *)(ibc_e->data) + (numb - NUM_DIRECT)) = b;
            ibc_e->dirty = 1;

            //write the new dir_entry into block b
            bc_entry *bc_e = request_bc(b);
            *(dir_entry *)(bc_e->data) = de;
            bc_e->dirty = 1;
        }
    } else {
        //don't need a new block, simply append it
        int delta = (ic_e->data.size % BLOCKSIZE) / sizeof(dir_entry);
        bc_entry *bc_e;
        if (numb <= NUM_DIRECT) {
            //direct block
            bc_e = request_bc(ic_e->data.direct[numb - 1]);
        } else {
            //indirect block
            bc_entry *ibc_e = request_bc(ic_e->data.indirect);
            int b = *((int *)(ibc_e->data) + (numb - 1 - NUM_DIRECT));
            bc_e = request_bc(b);
        }
        // printf("delta: %d\n", delta);
        *((dir_entry *)(bc_e->data) + delta) = de;
        bc_e->dirty = 1;
    }
    //finally update the data size
    ic_e->data.size += sizeof(dir_entry);
    ic_e->dirty = 1;
    return 0;
}

void truncate_inode(int inode_id) {
    ic_entry *ic_e = request_ic(inode_id);
    int numb = size_to_block(ic_e->data.size);

    int lim = numb > NUM_DIRECT ? NUM_DIRECT : numb;
    int i;
    //free direct blocks
    for (i = 0; i < lim; i++) {
        free_block(ic_e->data.direct[i]);
    }
    //free indirect blocks
    for (i = 0; i < numb - lim; i++) {
        bc_entry *bc_e = request_bc(ic_e->data.indirect);
        free_block(*((int *)(bc_e->data) + i));
    }
    //free the indirect block
    if (numb > lim) {
        free_block(ic_e->data.indirect);
    }
    ic_e->data.size = 0;
    ic_e->dirty = 1;
}

void clean_inode(int inode_id) {
    truncate_inode(inode_id);
    //finally free this inode
    ic_entry *ic_e = request_ic(inode_id);
    ic_e->data.type = INODE_FREE;
    ic_e->dirty = 1;
    free_inode(inode_id);
}

//len does not count '\0'
int add_dir_entry(int inode_id, int inum, char *name, int len) {
    dir_entry_iterator iter;
    get_iterator(&iter, inode_id);
    int ok = 0;
    while (has_next(&iter)) {
        dir_entry *e = get_next(&iter);
        if (e->inum == 0) {
            //we can simply use this entry
            e->inum = inum;
            memset(e->name, 0, DIRNAMELEN);
            memcpy(e->name, name, len);
            ok = 1;
            wrote_iter(&iter);
            break;
        }
    }
    int ret = 0;
    if (!ok) {
        ret = append_dir_entry(inode_id, inum, name, len);
    }
    printf("append ret: %d, len: %d, name: %.*s\n", ret, len, len, name);
    //return -1 if run out of blocks
    return ret;
}

int remove_dir_entry(int inode_id, char *name, int len) {
    dir_entry_iterator iter;
    get_iterator(&iter, inode_id);
    while (has_next(&iter)) {
        dir_entry *e = get_next(&iter);
        if (e->inum != 0 && is_same_dir_name(e->name, name, len)) {
            ic_entry *ic_e = request_ic(e->inum);
            if (ic_e->data.type == INODE_DIRECTORY) {
                if (ic_e->data.nlink > 2) {
                    //it has sub dir entries, cannot remove it
                    return -1;
                } else {
                    dir_entry_iterator sub_iter;
                    get_iterator(&sub_iter, e->inum);

                    int cnt = 0;
                    while (has_next(&sub_iter)) {
                        if (get_next(&sub_iter)->inum != 0) {
                            cnt++;
                            if (cnt >= 3) {
                                //it still has other dir entries
                                return -1;
                            }
                        }
                    }

                    //mark it free
                    e->inum = 0;
                    wrote_iter(&iter);

                    printf("before clean\n");
                    //clean this inode
                    clean_inode(ic_e->inode_id);

                    //decrease its parent's nlink count
                    ic_entry *pic_e = request_ic(inode_id);
                    pic_e->data.nlink--;
                    pic_e->dirty = 1;
                }
            } else if (ic_e->data.type == INODE_REGULAR) {
                e->inum = 0;
                wrote_iter(&iter);

                ic_e->data.nlink--;
                ic_e->dirty = 1;

                if (ic_e->data.nlink == 0) {
                    clean_inode(ic_e->inode_id);
                }
            } else if (ic_e->data.type == INODE_SYMLINK) {
                //TODO
            } else {
                //not possible
                return -1;
            }
            continue;
        }
    }

    return 0;
}

int parse(char *pathname, int pathlen, int cur_inode, int depth) {
    //return -1 if failed
    int ret = 0;
    while (pathlen > 0) {
        int p = 0;
        //ignore the leading '/'s
        while (p < pathlen && pathname[p] == '/')
            p++;
        pathname += p;
        pathlen -= p;

        int len = 0;
        //get the current dir name
        while (len < pathlen && pathname[len] != '/')
            len++;
        if (len > DIRNAMELEN) {
            //this file name is too long, never gonna match anything
            return -1;
        }

        if (len == 0) {
            pathname = ".";
            len = 1;
        }

        // printf("%.*s\n", len, pathname);
        int nxt = find_dir_entry(cur_inode, pathname, len);
        if (nxt) {
            if (request_ic(nxt)->data.type != INODE_SYMLINK) {
                ret = nxt;
                pathlen -= len;
                pathname += len;
            } else {
                //TODO: this is a symlink, basically we are gonna do this recursively
            }
            cur_inode = nxt;
        } else {
            //file or directory does not exist
            return -1;
        }
    }
    return ret;
}

/**
 * get the block at block_idx in inode, return -1 if any error
 */
static int get_block(int inode_id, int block_idx) {
    ic_entry *ic_e = request_ic(inode_id);
    int numb = size_to_block(ic_e->data.size);
    if (block_idx >= numb) {
        return -1;
    }
    if (block_idx < NUM_DIRECT) {
        return ic_e->data.direct[block_idx];
    } else {
        int delta = block_idx - NUM_DIRECT;
        bc_entry *bc_e = request_bc(ic_e->data.indirect);
        if (delta < BLOCKSIZE / sizeof(int)) {
            return *((int *)(bc_e->data) + delta);
        }
    }
    return -1;
}

/**
 * set a block in inode, return -1 if any error
 */
static int set_block(int inode_id, int block_idx, int block_id) {
    ic_entry *ic_e = request_ic(inode_id);
    int numb = size_to_block(ic_e->data.size);

    if (block_idx < NUM_DIRECT) {
        ic_e->data.direct[block_idx] = block_id;
        ic_e->dirty = 1;
        return 0;
    } else {
        if (numb <= NUM_DIRECT) {
            int ib = grab_block();
            if (ib == -1) {
                return -1;
            }
            ic_e->data.indirect = ib;
            ic_e->dirty = 1;
        }
        int delta = block_idx - NUM_DIRECT;
        bc_entry *bc_e = request_bc(ic_e->data.indirect);
        if (delta < BLOCKSIZE / sizeof(int)) {
            *((int *)(bc_e->data) + delta) = block_id;
            bc_e->dirty = 1;
            return 0;
        }
    }
    return -1;
}
/**
 * append a block in inode, return -1 if any error
 */
static int append_block(int inode_id, int block_id) {
    ic_entry *ic_e = request_ic(inode_id);
    int numb = size_to_block(ic_e->data.size);
    return set_block(inode_id, numb, block_id);
}

char hole[BLOCKSIZE];
/**
 * Read the file, return how many bytes is read, return -1 if any error
 */
int read_file(int inode_id, int offset, char *buf, int buf_len, int pid) {
    if (buf_len == 0) {
        return 0;
    }

    ic_entry *ic_e = request_ic(inode_id);

    if (ic_e->data.size <= offset) {
        //offset exceed the file size
        return offset == ic_e->data.size ? 0 : -1;
    }
    int numb = size_to_block(ic_e->data.size);

    int block_idx = -1;
    //go to the block before the first block we are going to read
    block_idx += offset / BLOCKSIZE;
    offset = offset % BLOCKSIZE;

    //now move the cursor to the first block to read;
    block_idx++;
    int size_passed = block_idx * BLOCKSIZE + offset;
    int read = 0;
    while (buf_len) {
        int cur = get_block(inode_id, block_idx);
        if (cur == -1) {
            //reached end of file
            return read;
        }

        int size = BLOCKSIZE - offset > buf_len ? buf_len : BLOCKSIZE - offset;
        if (size >= ic_e->data.size - size_passed) {
            //reached end of file
            if (cur) {
                bc_entry *bc_e = request_bc(cur);
                CopyTo(pid, buf, bc_e->data + offset, ic_e->data.size - size_passed);
            } else {
                CopyTo(pid, buf, hole, ic_e->data.size - size_passed);
            }

            read += ic_e->data.size - size_passed;
            return read;
        }
        if(cur) {
            bc_entry *bc_e = request_bc(cur);
            CopyTo(pid, buf, bc_e->data + offset, size);
        } else {
            CopyTo(pid, buf, hole, size);
        }
        block_idx++;
        offset = 0;
        read += size;
        size_passed += size;
        buf += size;
        buf_len -= size;
    }
    return read;
}

/**
 * Write the file start from offset(count from 0), return how many bytes are written, return -1 if any error 
 * This function should have the ability to handle holes in file.
 */
int write_file(int inode_id, int offset, char *buf, int buf_len, int pid) {
    if (buf_len == 0) {
        return 0;
    }

    ic_entry *ic_e = request_ic(inode_id);

    int numb = size_to_block(ic_e->data.size);

    //move the cursor to the block before the first block we want to write
    int block_idx = -1;
    // int size_inced = 0;
    while (offset >= BLOCKSIZE) {
        block_idx++;
        if (block_idx >= numb) {
            printf("putted hole\n");
            //need to put a hole
            if (append_block(ic_e->inode_id, 0) == -1) {
                //indirect block full
                // //roll back the size change
                // ic_e->data.size -= size_inced;
                // ic_e->dirty = 1;
                return -1;
            }
            numb++;
            ic_e->data.size += BLOCKSIZE;
            // size_inced += BLOCKSIZE;
            ic_e->dirty = 1;
        } else if(block_idx == numb - 1) {
            ic_e->data.size += BLOCKSIZE - last_block_bytes(ic_e->data.size);
            ic_e->dirty = 1;
        }
        offset -= BLOCKSIZE;
    }

    //increase the cursor, this is the first block we want to write
    block_idx++;

    // printf("write block_idx: %d\n", block_idx);
    int written = 0;
    while (buf_len) {
        int cur = get_block(inode_id, block_idx);
        int appended = 0;
        if (cur == -1 || cur == 0) {
            //need to set this block
            int b = grab_block();
            if (b == -1) {
                return written;
            }
            if (set_block(inode_id, block_idx, b) == -1) {
                //indirect block full
                free_block(b);
                // if(written == 0) {
                //     //we have not written anything, roll back the size change
                //     ic_e->data.size -= size_inced;
                //     ic_e->dirty = 1;
                // }
                fprintf(stderr, "set block failed: block_idx: %d\n", block_idx);
                return written;
            }
            if (cur == -1) {
                printf("appended\n");
                appended = 1;
            }
            cur = b;
        }

        bc_entry *bc_e = request_bc(cur);
        //how many bytes are we going to write to this page
        int size = BLOCKSIZE - offset > buf_len ? buf_len : BLOCKSIZE - offset;
        int cf = CopyFrom(pid, bc_e->data + offset, buf, size);
        printf("cf: %d, util wrote: %.*s\n", cf, size, bc_e->data + offset);
        bc_e->dirty = 1;
        if (appended) {
            //if we appended a new page, we'll need to increase the size
            ic_e->data.size += size + offset;
            ic_e->dirty = 1;
        }
        //if we are writing to the last block, be careful
        if (block_idx == numb - 1) {
            int delta = offset + size - last_block_bytes(ic_e->data.size);
            if (delta > 0) {
                ic_e->data.size += delta;
                ic_e->dirty = 1;
            }
        }
        block_idx++;
        offset = 0;
        buf += size;
        buf_len -= size;
        written += size;
    }

    printf("file size: %d\n", ic_e->data.size);
    return written;
}

#endif