#ifndef FREE_LIST_H
#define FREE_LIST_H

extern int _num_blocks, _num_inodes, _data_blocks_start;

inline int is_valid_block(int id, int l, int r);

inline int is_valid_inode(int id, int r);

/**
 * Return -1 if run out of inodes
 */
int grab_inode();

/**
 * Return -1 if run out of blocks
 */
int grab_block();

int free_inode(int inode_id);

int free_block(int block_id);

void init_free_list(int num_block, int num_inode);

#endif