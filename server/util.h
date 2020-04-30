#ifndef UTIL_H
#define UTIL_H

typedef struct dir_entry dir_entry;
//get how many blocks are used by this size
#define size_to_block(x) (((x) + BLOCKSIZE - 1) / BLOCKSIZE)
//get how many bytes in the last block
#define last_block_bytes(x) ((x) - (((x) - 1) / BLOCKSIZE) * BLOCKSIZE)
/**
 * This file defines the untility functions for the yfs.
 */

/**
 * Print the directory
 */
void print_dir(int inode_id);

/**
 * Print the file tree
 */
void print_tree(int rt, int pa, int depth);

/**
 * Returns the inode number of the input pathname
 * If the pathname is not valid, return -1
 */
int parse(char *pathname, int pathlen, int cur_inode, int depth);

/**
 * Add a new entry to the directory, if there are already are free entries, overwirte that.
 * Otherwise, append a new one.
 */
int add_dir_entry(int inode_id, int inum, char *name, int len);

/**
 * Try removing one direcotry entry, return 0 if success, and -1 if any error
 */
int remove_dir_entry(int inode_id, char *name, int len);

/**
 * Clean the inode, free all the blocks and inodes
 */
void clean_inode(int inode_id);
void truncate_inode(int inode_id);

int read_file(int inode_id, int offset, char *buf, int buf_len, int pid);

int write_file(int inode_id, int offset, char *buf, int buf_len, int pid);
#endif