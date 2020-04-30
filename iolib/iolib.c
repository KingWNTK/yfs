#ifndef IOLIB_C
#define IOLIB_C
#include "../public/msg.h"
#include <comp421/filesystem.h>
#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>

static int cur_dir = 1;
static int cur_dir_reuse = 1;

static void build_single_ptr_msg(single_ptr_msg *msg, char *pathname, int pathlen) {
    msg->current_dir = cur_dir;
    msg->path = pathname;
    msg->reuse = cur_dir_reuse;
    msg->pathlen = pathlen;
}

static reply_msg send_msg(void *msg, int type) {
    msg_wrap *wrap = (msg_wrap *)malloc(sizeof(msg_wrap));
    wrap->type = type;
    wrap->msg = msg;
    Send(wrap, -FILE_SERVER);
    reply_msg rep = *(reply_msg *)wrap;
    free(wrap);
    return rep;
}

static int get_pathlen(char *pathname) {
    if (pathname[0] == '\0')
        return -1;
    int i = 0;
    while (i < MAXPATHNAMELEN && pathname[i] != '\0')
        i++;
    if (i == MAXPATHNAMELEN) {
        return -1;
    }
    return i;
}

int Open(char *pathname) {
    int pathlen = get_pathlen(pathname);
    if(pathlen == -1) {
        return ERROR;
    }
    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    send_msg(&msg, OPEN_MSG);
    return 0;
}
int Close(int fd) {
    send_msg(NULL, 0);
    return 0;
}
int Create(char *pathname) {
    int pathlen = get_pathlen(pathname);
    if(pathlen == -1) {
        return ERROR;
    }
    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    reply_msg rep = send_msg(&msg, CREATE_MSG);
    printf("create reply: %d\n", rep.val);
    return rep.val;
}
int Read(int fd, void *buf, int size) {
    read_write_msg msg;
    msg.inode_id = 1;
    msg.buf = buf;
    msg.size = 10;
    msg.offset = 10030;
    msg.reuse = 1;
    reply_msg rep = send_msg(&msg, READ_MSG);
    printf("read reply: %d\n", rep.val);
    // printf("read: %.*s\n", rep.val, buf);
    return rep.val;
}
int Write(int fd, void *buf, int size) {
    read_write_msg msg;
    msg.inode_id = 1;
    msg.buf = buf;
    msg.size = 30;
    msg.offset = 10000;
    msg.reuse = 1;
    reply_msg rep = send_msg(&msg, WRITE_MSG);
    printf("write reply: %d\n", rep.val);
    return rep.val;
}
int Seek(int fd, int offset, int whence) {
    send_msg(NULL, 0);
    return 0;
}
int Link(char *oldname, char *newname) {
    send_msg(NULL, 0);
    return 0;
}
int Unlink(char *pathname) {
    int pathlen = get_pathlen(pathname);
    if(pathlen == -1) {
        return ERROR;
    }
    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    reply_msg rep = send_msg(&msg, UNLINK_MSG);
    printf("unlink reply: %d\n", rep.val);
    return rep.val;
}
int SymLink(char *oldname, char *newname) {
    send_msg(NULL, 0);
    return 0;
}
int ReadLink(char *pathname, char *buf, int len) {
    send_msg(NULL, 0);
    return 0;
}
int MkDir(char *pathname) {
    int pathlen = get_pathlen(pathname);
    if(pathlen == -1) {
        return ERROR;
    }
    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    reply_msg rep = send_msg(&msg, MKDIR_MSG);
    printf("mkdir reply: %d\n", rep.val);
    return rep.val;
}
int RmDir(char *pathname) {
    int pathlen = get_pathlen(pathname);
    if(pathlen == -1) {
        return ERROR;
    }
    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    reply_msg rep = send_msg(&msg, RMDIR_MSG);
    printf("rmdir reply: %d\n", rep.val);
    return rep.val;
}
int ChDir(char *pathname) {
    int pathlen = get_pathlen(pathname);
    if(pathlen == -1) {
        return ERROR;
    }
    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    reply_msg rep = send_msg(&msg, CHDIR_MSG);
    printf("chdir reply: %d\n", rep.val);

    if(rep.val == 0) {
        cur_dir = rep.inode_id;
        cur_dir_reuse = rep.reuse;
        printf("new dir: %d, reuse: %d\n", cur_dir, cur_dir_reuse);
    }
    return rep.val;
}
int Stat(char *pathname, struct Stat *statbuf) {
    send_msg(NULL, 0);
    return 0;
}
int Sync() {
    send_msg(NULL, 0);
    return 0;
}
int Shutdown() {
    send_msg(NULL, 0);
    return 0;
}

#endif