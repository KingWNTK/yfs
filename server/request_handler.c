#ifndef REQUEST_HANDLER_C
#define REQUEST_HANDLER_C

#include "request_handler.h"
#include "cache.h"
#include "free_list.h"
#include "util.h"
#include <comp421/filesystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int get_last_name(char *pathname, int pl, char **name, int *name_len) {
    //first, remove the appended '/'s
    while (pl > 0 && pathname[pl - 1] == '/')
        pl--;
    int tmp = pl;

    //next, get the new dir name
    while (tmp > 0 && pathname[tmp - 1] != '/')
        tmp--;
    *name_len = pl - tmp;
    if (*name_len == 0) {
        //empty name
        return -1;
    }
    //save this name, remember to free it later
    *name = (char *)malloc(*name_len);
    memcpy(*name, pathname + tmp, *name_len);

    return tmp;
}

int handle_mkdir(single_ptr_msg *msg, char *pathname_buf) {
    //first of all, param check
    int pl = get_pathlen(pathname_buf);
    //double check this parameter here

    if (msg->pathlen != pl || pl == -1) {
        return -1;
    }

    int ok = parse(pathname_buf, pl, msg->current_dir, 0);
    //also don't allow the / at the back
    if (ok != -1 || pathname_buf[pl - 1] == '/') {
        //path name exist, cannot make it
        return -1;
    }

    char *name;
    int name_len;
    int tmp = get_last_name(pathname_buf, pl, &name, &name_len);
    if (tmp == -1 || (name_len == 1 && strncmp(name, ".", name_len) == 0) ||(name_len == 2 && strncmp(name, "..", name_len) == 0)) {
        //cannot let the name be . or ..
        free(name);
        return -1;
    }
    pl = tmp;

    //now we know which directory should we make the new directory in
    int par = (pl == 0 ? msg->current_dir : parse(pathname_buf, pl, msg->current_dir, 0));
    printf("parent path: %.*s\n", pl, pathname_buf);
    if (par == -1 || request_ic(par)->data.type != INODE_DIRECTORY) {
        //the directory does not exist
        return -1;
    }

    //finally we can create the new directory
    //grab an inode
    int ni = grab_inode();
    if (ni == -1) {
        return -1;
    }

    if (add_dir_entry(par, ni, name, name_len) == -1) {
        //run out of blocks
        free_inode(ni);
        return -1;
    }
    free(name);

    ic_entry *ic_e = request_ic(ni);
    //IMPORTANT: increase the reuse number
    ic_e->data.size = 0;
    ic_e->data.nlink = 2;
    ic_e->data.reuse++;
    ic_e->data.type = INODE_DIRECTORY;
    ic_e->dirty = 1;

    add_dir_entry(ni, ni, ".", 1);
    add_dir_entry(ni, par, "..", 2);

    //since we have ".." linking to its parent, increase its nlink count
    ic_entry *par_ic_e = request_ic(par);
    par_ic_e->data.nlink++;
    par_ic_e->dirty = 1;
    return 0;
}

int handle_rmdir(single_ptr_msg *msg, char *pathname_buf) {
    //first of all, param check
    int pl = get_pathlen(pathname_buf);
    //double check this parameter here

    //also don't allow the / at the back
    if (msg->pathlen != pl || pl == -1 || pathname_buf[pl - 1] == '/') {
        return -1;
    }

    int dir = parse(pathname_buf, pl, msg->current_dir, 0);
    if (dir == -1 || request_ic(dir)->data.type != INODE_DIRECTORY) {
        //its not a directory
        return -1;
    }

    char *name;
    int name_len;
    int tmp = get_last_name(pathname_buf, pl, &name, &name_len);
    if (tmp == -1 || (name_len == 1 && strncmp(name, ".", name_len) == 0) ||(name_len == 2 && strncmp(name, "..", name_len) == 0)) {
        //no last name, can't remove it
        //cannot let the name be . or ..
        free(name);
        return -1;
    }
    pl = tmp;

    printf("parent path: %.*s\n", pl, pathname_buf);
    //this par is gauranteed to be valid(not 0 or -1) since the whole path is valid
    int par = (pl == 0 ? msg->current_dir : parse(pathname_buf, pl, msg->current_dir, 0));
    return remove_dir_entry(par, name, name_len);
}

int hanlde_chdir(single_ptr_msg *msg, char *pathname_buf, int *new_dir, int *reuse) {
    //first of all, param check
    int pl = get_pathlen(pathname_buf);
    //double check this parameter here

    if (msg->pathlen != pl || pl == -1) {
        return -1;
    }

    int dir = parse(pathname_buf, pl, msg->current_dir, 0);
    printf("chdir: %d\n", dir);
    if (dir == -1 || request_ic(dir)->data.type != INODE_DIRECTORY) {
        //its not a directory
        return -1;
    }

    *new_dir = dir;
    *reuse = request_ic(dir)->data.reuse;
    return 0;
}

int handle_create(single_ptr_msg *msg, char *pathname_buf, int *inode_id, int *reuse) {
    //first of all, param check
    int pl = get_pathlen(pathname_buf);
    //double check this parameter here

    if (msg->pathlen != pl || pl == -1 || pathname_buf[pl - 1] == '/') {
        //invalid file pathname
        return -1;
    }

    int nd = parse(pathname_buf, pl, msg->current_dir, 0);

    if (nd != -1) {
        ic_entry *e = request_ic(nd);
        if (e->data.type == INODE_REGULAR) {
            //file name exist, truncate it
            truncate_inode(nd);
            *inode_id = e->inode_id;
            *reuse = e->data.reuse;
            return 0;
        } else {
            //cannot truncate non-file inodes
            return -1;
        }
    }

    char *name;
    int name_len;
    int tmp = get_last_name(pathname_buf, pl, &name, &name_len);
    if (tmp == -1) {
        //can't get last name
        free(name);
        return -1;
    }
    pl = tmp;

    //now we know which directory should we make the new directory in
    int par = (pl == 0 ? msg->current_dir : parse(pathname_buf, pl, msg->current_dir, 0));
    printf("parent path: %.*s\n", pl, pathname_buf);
    if (par == -1 || request_ic(par)->data.type != INODE_DIRECTORY) {
        //the directory does not exist
        return -1;
    }

    //finally we can create a new file
    //grab an inode
    int ni = grab_inode();
    if (ni == -1) {
        return -1;
    }
    if (add_dir_entry(par, ni, name, name_len) == -1) {
        //run out of blocks
        free_inode(ni);
        return -1;
    }
    free(name);

    ic_entry *ic_e = request_ic(ni);
    //IMPORTANT: increase the reuse number
    ic_e->data.size = 0;
    ic_e->data.nlink = 1;
    ic_e->data.reuse++;
    ic_e->data.type = INODE_REGULAR;
    ic_e->dirty = 1;

    *inode_id = ni;
    *reuse = ic_e->data.reuse;
    return 0;
}

int handle_open(single_ptr_msg *msg, char *pathname_buf, int *inode_id, int *reuse) {
    int pl = get_pathlen(pathname_buf);
    //double check this parameter here

    if (msg->pathlen != pl || pl == -1) {
        return -1;
    }

    int nd = parse(pathname_buf, pl, msg->current_dir, 0);
    if (nd == -1) {
        //file does not exist
        return -1;
    }

    ic_entry *ic_e = request_ic(nd);
    *inode_id = nd;
    *reuse = ic_e->data.reuse;
    return 0;
}

int handle_link(double_ptr_msg *msg, char *pathname_buf, char *new_pathname_buf) {
    int pl = get_pathlen(pathname_buf), npl = get_pathlen(new_pathname_buf);
    //double check parameters here

    if (msg->p1_len != pl || pl == -1 || msg->p2_len != npl || npl == -1) {
        return -1;
    }

    if(pathname_buf[pl - 1] == '/' || new_pathname_buf[npl - 1] == '/') {
        //cannot link two directories
        return -1;
    }

    int nd = parse(pathname_buf, pl, msg->current_dir, 0);
    if (nd == -1 || request_ic(nd)->data.type == INODE_DIRECTORY) {
        //file does not exist or its a directory file,
        //we can not link to directory files
        return -1;
    }

    printf("oldname: %.*s, newname: %.*s\n", pl, pathname_buf, npl, new_pathname_buf);

    //determine whether this is an absolute path
    int new_cd = new_pathname_buf[0] == '/' ? 1 : msg->current_dir;

    if (new_cd != 1 && (request_ic(new_cd)->data.reuse != msg->reuse || request_ic(new_cd)->data.type != INODE_DIRECTORY)) {
        //can't use this relative directory
        return -1;
    }

    int new_nd = parse(new_pathname_buf, npl, new_cd, 0);
    if (new_nd != -1) {
        //it must not exist
        return -1;
    }

    char *name;
    int name_len;
    int tmp = get_last_name(new_pathname_buf, npl, &name, &name_len);
    if (tmp == -1 || (name_len == 1 && strncmp(name, ".", name_len) == 0) ||(name_len == 2 && strncmp(name, "..", name_len) == 0)) {
        //cannot let the name be . or ..
        free(name);
        return -1;
    }
    npl = tmp;

    //now we know which directory should we make the new directory in
    int par = (npl == 0 ? new_cd : parse(new_pathname_buf, npl, new_cd, 0));
    if (par == -1 || request_ic(par)->data.type != INODE_DIRECTORY) {
        //the directory does not exist
        return -1;
    }
    
    //here we done checking the params, now we can do the actual link
    if (add_dir_entry(par, nd, name, name_len) == -1) {
        //run out of memory
        return -1;
    }
    free(name);
    
    //increase the link count
    ic_entry *ic_e = request_ic(nd);
    ic_e->data.nlink++;
    ic_e->dirty = 1;

    return 0;
}

int handle_unlink(single_ptr_msg *msg, char *pathname_buf) {
    int pl = get_pathlen(pathname_buf);
    //double check this parameter here

    if (msg->pathlen != pl || pl == -1 || pathname_buf[pl - 1] == '/') {
        //invalid pathname
        return -1;
    }

    int nd = parse(pathname_buf, pl, msg->current_dir, 0);
    if (nd == -1 || request_ic(nd)->data.type == INODE_DIRECTORY) {
        //it does not exist, or its a directory
        return -1;
    }

    char *name;
    int name_len;
    int tmp = get_last_name(pathname_buf, pl, &name, &name_len);
    if (tmp == -1) {
        //no last name, can't remove it
        free(name);
        return -1;
    }
    pl = tmp;

    //this par is gauranteed to be valid(not 0 or -1) since the whole path is valid
    int par = (pl == 0 ? msg->current_dir : parse(pathname_buf, pl, msg->current_dir, 0));
    return remove_dir_entry(par, name, name_len);
}

int handle_read(read_write_msg *msg, int pid) {
    return read_file(msg->inode_id, msg->offset, msg->buf, msg->size, pid);
}

int handle_write(read_write_msg *msg, int pid) {
    return write_file(msg->inode_id, msg->offset, msg->buf, msg->size, pid);
}

int handle_stat(double_ptr_msg *msg, char *pathname_buf, struct Stat *stat) {
    int pl = get_pathlen(pathname_buf);
    //double check parameters here

    if (msg->p1_len != pl || pl == -1) {
        return -1;
    }

    int nd = parse(pathname_buf, pl, msg->current_dir, 0);
    if (nd == -1) {
        //file does not exist
        return -1;
    }

    ic_entry *ic_e = request_ic(nd);
    stat->inum = ic_e->inode_id;
    stat->type = ic_e->data.type;
    stat->size = ic_e->data.size;
    stat->nlink = ic_e->data.nlink;

    return 0;
}
#endif