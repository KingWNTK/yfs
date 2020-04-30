#ifndef MSG_H
#define MSG_H

#define SINGLE_PTR_MSG_LOW 1
#define OPEN_MSG 1
#define CREATE_MSG 2
#define UNLINK_MSG 3
#define MKDIR_MSG 4
#define RMDIR_MSG 5
#define CHDIR_MSG 6
#define SIGNLE_PTR_MSG_HIGH 6

#define READ_WRITE_MSG_LOW 7
#define READ_MSG 7
#define WRITE_MSG 8
#define READ_WRITE_MSG_HIGH 8

#define DOUBLE_PTR_MSG_LOW 9
#define LINK_MSG 9
#define SYMLINK_MSG 10
#define READLINK_MSG 11
#define STAT_MSG 12
#define DOUBLE_PTR_MSG_HIGH 12

#define CONTROL_MSG_LOW 13
#define SYNC_MSG 13
#define SHUTDOWN_MSG 14
#define CONTROL_MSG_HIGH 14

#define GETFILEZIE_MSG 15

typedef struct single_ptr_msg {
    int current_dir;
    int reuse;
    int pathlen;
    void *path;
} single_ptr_msg;

typedef struct read_write_msg {
    int inode_id;
    int reuse;
    int offset;
    int size;
    void *buf;
} read_write_msg;

typedef struct double_ptr_msg {
    int current_dir;
    int reuse;
    int p1_len;
    int p2_len;
    void *p1;
    void *p2;
} double_ptr_msg;

typedef struct msg_wrap {
    int type;
    void *msg;
    char padding[16];
} msg_wrap;

typedef struct get_file_size_msg {
    int inode_id;
    int reuse;
    char padding[24];
}get_file_size_msg;


typedef struct reply_msg {
    int val;
    int inode_id;
    int reuse;
    int size;
    char padding[16];
}reply_msg;
#endif