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

typedef struct file_node {
    int used;
    int inode_id;
    int reuse;
    //this offset is always relative to the beginning of the file
    int offset_begin;
} file_node;

file_node files[MAX_OPEN_FILES];
int files_cnt;

static inline void build_single_ptr_msg(single_ptr_msg *msg, char *pathname, int pathlen) {
    msg->current_dir = cur_dir;
    msg->reuse = cur_dir_reuse;
    msg->path = pathname;
    msg->pathlen = pathlen;
}

static inline void build_double_ptr_msg(double_ptr_msg *msg, char *p1, int p1_len, char *p2, int p2_len) {
    msg->current_dir = cur_dir;
    msg->reuse = cur_dir_reuse;
    msg->p1 = p1;
    msg->p1_len = p1_len;
    msg->p2 = p2;
    msg->p2_len = p2_len;
}

static reply_msg send_msg(void *msg, int type) {
    msg_wrap *wrap = (msg_wrap *)malloc(sizeof(msg_wrap));
    wrap->type = type;
    wrap->msg = msg;
    int s = Send(wrap, -FILE_SERVER);
    reply_msg rep = *(reply_msg *)wrap;
    free(wrap);
    if(s == ERROR) {
        rep.val = ERROR;
    }
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

static int get_new_fd() {
    int fd;
    if (files_cnt < MAX_OPEN_FILES) {
        int i;
        for (i = 0; i < MAX_OPEN_FILES; i++) {
            if (files[i].used == 0) {
                fd = i;
                break;
            }
        }
    } else {
        //can't open more files
        return ERROR;
    }
    return fd;
}

int Open(char *pathname) {
    int pathlen = get_pathlen(pathname);
    if (pathlen == -1) {
        return ERROR;
    }
    int fd = get_new_fd();
    if (fd == -1) {
        return ERROR;
    }

    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    reply_msg rep = send_msg(&msg, OPEN_MSG);
    if (rep.val != -1) {
        files[fd].used = 1;
        files[fd].inode_id = rep.inode_id;
        files[fd].offset_begin = 0;
        files[fd].reuse = rep.reuse;
        files_cnt++;
        return fd;
    }

    files[fd].used = 0;
    files_cnt++;
    return ERROR;
}

int Close(int fd) {
    if (fd >= 0 && fd < MAX_OPEN_FILES && files[fd].used == 1) {
        files[fd].used = 0;
        files_cnt--;
        return 0;
    }
    return ERROR;
}

int Create(char *pathname) {
    int pathlen = get_pathlen(pathname);
    if (pathlen == -1) {
        return ERROR;
    }
    int fd = get_new_fd();
    if (fd == -1) {
        return ERROR;
    }
    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    reply_msg rep = send_msg(&msg, CREATE_MSG);
    if (rep.val == -1) {
        files[fd].used = 0;
        files_cnt--;
        return ERROR;
    }
    

    files[fd].used = 1;
    files[fd].inode_id = rep.inode_id;
    files[fd].offset_begin = 0;
    files[fd].reuse = rep.reuse;
    files_cnt++;
    return fd;
}

int Read(int fd, void *buf, int size) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || files[fd].used == 0) {
        //invalid fd
        return ERROR;
    }
    read_write_msg msg;
    msg.inode_id = files[fd].inode_id;
    msg.buf = buf;
    msg.size = size;
    msg.offset = files[fd].offset_begin;
    msg.reuse = files[fd].reuse;
    reply_msg rep = send_msg(&msg, READ_MSG);
    // printf("read: %.*s\n", rep.val, buf);
    if(rep.val > 0) {
        files[fd].offset_begin += rep.val;
    }
    return rep.val;
}

int Write(int fd, void *buf, int size) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || files[fd].used == 0) {
        //invalid fd
        return ERROR;
    }
    read_write_msg msg;
    msg.inode_id = files[fd].inode_id;
    msg.buf = buf;
    msg.size = size;
    msg.offset = files[fd].offset_begin;
    msg.reuse = files[fd].reuse;
    reply_msg rep = send_msg(&msg, WRITE_MSG);
    if(rep.val > 0) {
        files[fd].offset_begin += rep.val;
    }
    return rep.val;
}

int Seek(int fd, int offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || files[fd].used == 0) {
        //invalid fd
        return ERROR;
    }
    switch (whence) {
    case SEEK_SET:
        if (offset < 0) {
            return ERROR;
        }
        files[fd].offset_begin = offset;

        break;
    case SEEK_CUR:
        if (files[fd].offset_begin + offset < 0) {
            return ERROR;
        }
        files[fd].offset_begin += offset;
        break;
    case SEEK_END: {
        single_ptr_msg msg;
        msg.current_dir = files[fd].inode_id;
        msg.reuse = files[fd].reuse;
        reply_msg rep = send_msg(&msg, GETFILEZIE_MSG);
        if(rep.val == -1) {
            return ERROR;
        }
        int file_size = rep.val;
        if (file_size + offset < 0) {
            return ERROR;
        }
        files[fd].offset_begin = file_size + offset;
        break;
    }
    default:
        return ERROR;
    }
    return files[fd].offset_begin;
}

int Link(char *oldname, char *newname) {
    int pathlen = get_pathlen(oldname), new_pathlen = get_pathlen(newname);
    if (pathlen == -1 || new_pathlen == -1) {
        return ERROR;
    }
    double_ptr_msg msg;
    build_double_ptr_msg(&msg, oldname, pathlen, newname, new_pathlen);

    reply_msg rep = send_msg(&msg, LINK_MSG);
    return rep.val;
}

int Unlink(char *pathname) {
    int pathlen = get_pathlen(pathname);
    if (pathlen == -1) {
        return ERROR;
    }
    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    reply_msg rep = send_msg(&msg, UNLINK_MSG);
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
    if (pathlen == -1) {
        return ERROR;
    }
    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    reply_msg rep = send_msg(&msg, MKDIR_MSG);
    return rep.val;
}

int RmDir(char *pathname) {
    int pathlen = get_pathlen(pathname);
    if (pathlen == -1) {
        return ERROR;
    }
    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    reply_msg rep = send_msg(&msg, RMDIR_MSG);
    return rep.val;
}

int ChDir(char *pathname) {
    int pathlen = get_pathlen(pathname);
    if (pathlen == -1) {
        return ERROR;
    }
    single_ptr_msg msg;
    build_single_ptr_msg(&msg, pathname, pathlen);

    reply_msg rep = send_msg(&msg, CHDIR_MSG);

    if (rep.val == 0) {
        cur_dir = rep.inode_id;
        cur_dir_reuse = rep.reuse;
        // printf("new dir: %d, reuse: %d\n", cur_dir, cur_dir_reuse);
    }
    return rep.val;
}

int Stat(char *pathname, struct Stat *s) {
    int pathlen = get_pathlen(pathname);
    if (pathlen == -1) {
        return ERROR;
    }
    double_ptr_msg msg;
    build_double_ptr_msg(&msg, pathname, pathlen, s, 0);

    reply_msg rep = send_msg(&msg, STAT_MSG);
    return rep.val;
}

int Sync() {
    send_msg(NULL, SYNC_MSG);
    return 0;
}

int Shutdown() {
    send_msg(NULL, SHUTDOWN_MSG);
    return 0;
}

#endif