#include <comp421/filesystem.h>
#include <comp421/hardware.h>
#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <string.h>

#include "../public/msg.h"
#include "cache.h"
#include "free_list.h"
#include "hash_table.h"
#include "request_handler.h"
#include "util.h"
// #define HASH_TABLE_TEST
#ifdef HASH_TABLE_TEST
void hash_table_test() {
    printf("hashtable test begin\n");
    hash_table ht;
    init_ht(&ht);
    int i;
    for (i = 0; i < 1500; i++) {
        set_ht(&ht, i, (void *)(long long)i);
    }
    printf("count should be 1500, count: %d\n", ht.count);
    for (i = 0; i < 1500; i++) {
        erase_ht(&ht, i);
    }
    printf("count should be 0, count: %d\n", ht.count);
    for (i = 0; i < 1000; i++) {
        set_ht(&ht, i, (void *)(long long)i);
    }

    for (i = 0; i < 1000; i += 100) {
        if (has_key_ht(&ht, i)) {
            printf("val: %lld\n", (long long)get_ht(&ht, i)->ptr);
            set_ht(&ht, i, (void *)(long long)(i + 10));
            printf("val: %lld\n", (long long)get_ht(&ht, i)->ptr);
        }
    }
    clear_ht(&ht);
    printf("count should be 0, count: %d\n", ht.count);
    printf("hashtable test done\n");
}
#endif

// #define CACHE_TEST
#ifdef CACHE_TEST
void cache_test() {
    /**
     * !!IMPORTANT!!
     * please run mkyfs after this test,
     * because its going to mess up the disk 
     */
    printf("start cache test\n");
    init_cache();
    ic_entry *e = request_ic(0);
    struct fs_header *hd = (struct fs_header *)(&(e->data));
    int numb = hd->num_blocks;
    int numi = hd->num_inodes;
    printf("%d, %d\n", numb, numi);

    int i;
    for (i = 50; i < 100; i++) {
        bc_entry *bc_e = request_bc(i);
        *((int *)bc_e->data) = i;
        bc_e->dirty = 1;
    }
    for (i = 1; i <= numi; i++) {
        ic_entry *ic_e = request_ic(i);
        printf("inode id: %d, type: %d, size: %d\n", ic_e->inode_id, ic_e->data.type, ic_e->data.size);
        ic_e->data.size = i;
        ic_e->dirty = 1;
    }

    for (i = 50; i < 100; i++) {
        bc_entry *bc_e = request_bc(i);
        printf("block id: %d, val: %d\n", bc_e->block_id, *((int *)bc_e->data));
    }

    for (i = 1; i <= numi; i++) {
        ic_entry *ic_e = request_ic(i);
        printf("inode id: %d, size: %d\n", ic_e->inode_id, ic_e->data.size);
    }
    printf("cache test done\n");
}
#endif

// #define FREE_LIST_TEST
#ifdef FREE_LIST_TEST
void free_list_test() {
    init_cache();

    ic_entry *e = request_ic(0);
    struct fs_header *hd = (struct fs_header *)(&(e->data));
    int numb = hd->num_blocks;
    int numi = hd->num_inodes;
    printf("total blocks: %d, total inodes: %d\n", numb, numi);
    init_free_list(numb, numi);

    int i;
    char tmp[30];
    for (i = 0; i < 30; i++) {
        tmp[i] = grab_inode();
        printf("%d\n", tmp[i]);
    }
    for (i = 0; i < 30; i++) {
        free_inode(tmp[i]);
    }
    for (i = 0; i < 30; i++) {
        tmp[i] = grab_inode();
        printf("%d\n", tmp[i]);
    }
    for (i = 0; i < 30; i++) {
        free_inode(tmp[i]);
    }

    for (i = 0; i < 30; i++) {
        tmp[i] = grab_block();
        printf("%d\n", tmp[i]);
    }
    for (i = 0; i < 30; i++) {
        free_block(tmp[i]);
    }
}
#endif

//buffer to receive message
char buf[32];
//buffer storing the pathname
char pathname_buf[MAXPATHNAMELEN];
char new_pathname_buf[MAXPATHNAMELEN];

static int is_valid_cur_dir(int cur_dir, int reuse, char *pathname_buf, int pathlen) {
    //absolute path, always valid
    if (pathlen > 0 && pathname_buf[0] == '/') {
        return 1;
    }

    if (request_ic(cur_dir)->data.reuse == reuse && request_ic(cur_dir)->data.type == INODE_DIRECTORY) {
        return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    //testing
#ifdef HASH_TABLE_TEST
    hash_table_test();
#endif
#ifdef CACHE_TEST
    cache_test();
#endif
#ifdef FREE_LIST_TEST
    free_list_test();
    return 0;
#endif
    //end testing

    //initialize the LRU cache
    init_cache();

    ic_entry *e = request_ic(0);
    struct fs_header *hd = (struct fs_header *)(&(e->data));
    int numb = hd->num_blocks;
    int numi = hd->num_inodes;
    printf("total blocks: %d, total inodes: %d\n", numb, numi);

    //initialize the free block/inode list
    init_free_list(numb, numi);

    //register this service
    if (Register(FILE_SERVER) == ERROR) {
        fprintf(stderr, "Failed to register file server\n");
    }

    if (argc > 1) {
        int ret = Fork();
        if (ret == 0) {
            Exec(argv[1], argv + 1);
            return 0;
        }
    }

    // print_tree(1, 0, 1);

    while (1) {
        int pid = Receive(buf);
        if (pid == 0 || pid == ERROR) {
            //invalid receive
            continue;
        }
        msg_wrap *wrap = (msg_wrap *)buf;
        int ret = ERROR;
        reply_msg rep;

        if (SINGLE_PTR_MSG_LOW <= wrap->type && wrap->type <= SIGNLE_PTR_MSG_HIGH) {
            single_ptr_msg msg;
            //copy the real message here
            CopyFrom(pid, &msg, wrap->msg, sizeof(msg));
            memset(pathname_buf, 0, sizeof(pathname_buf));
            CopyFrom(pid, pathname_buf, msg.path, msg.pathlen);

            printf("current inode: %d, reuse: %d\n", msg.current_dir, request_ic(msg.current_dir)->data.reuse);
            // printf("%d %d\n", request_ic(msg.current_dir)->data.reuse, request_ic(msg.current_dir)->data.type);
            if (msg.pathlen > 0 && pathname_buf[0] == '/') {
                //this is absolute path
                msg.current_dir = 1;
                msg.reuse = 1;
            }
            if (is_valid_cur_dir(msg.current_dir, msg.reuse, pathname_buf, msg.pathlen)) {
                switch (wrap->type) {
                case MKDIR_MSG:
                    ret = handle_mkdir(&msg, pathname_buf);
                    break;
                case RMDIR_MSG:
                    ret = handle_rmdir(&msg, pathname_buf);
                    break;
                case CHDIR_MSG:
                    ret = hanlde_chdir(&msg, pathname_buf, &rep.inode_id, &rep.reuse);
                    break;
                case CREATE_MSG:
                    ret = handle_create(&msg, pathname_buf, &rep.inode_id, &rep.reuse);
                    break;
                case UNLINK_MSG:
                    ret = handle_unlink(&msg, pathname_buf);
                    break;
                case OPEN_MSG:
                    ret = handle_open(&msg, pathname_buf, &rep.inode_id, &rep.reuse);
                    break;
                default:
                    break;
                }
            }
        }

        if (DOUBLE_PTR_MSG_LOW <= wrap->type && wrap->type <= DOUBLE_PTR_MSG_HIGH) {
            double_ptr_msg msg;
            //copy the real message here
            CopyFrom(pid, &msg, wrap->msg, sizeof(msg));
            memset(pathname_buf, 0, sizeof(pathname_buf));
            CopyFrom(pid, pathname_buf, msg.p1, msg.p1_len);

            if (msg.p1_len > 0 && pathname_buf[0] == '/') {
                //this is absolute path
                msg.current_dir = 1;
                msg.reuse = 1;
            }

            printf("current inode: %d, reuse: %d\n", msg.current_dir, request_ic(msg.current_dir)->data.reuse);

            if (is_valid_cur_dir(msg.current_dir, msg.reuse, pathname_buf, msg.p1_len)) {
                if (wrap->type == LINK_MSG || wrap->type == SYMLINK_MSG) {
                    memset(new_pathname_buf, 0, sizeof(new_pathname_buf));
                    CopyFrom(pid, new_pathname_buf, msg.p2, msg.p2_len);
                    if (wrap->type == LINK_MSG) {
                        ret = handle_link(&msg, pathname_buf, new_pathname_buf);
                    } else if (wrap->type == SYMLINK_MSG) {
                        //TODO
                    }
                } else if (wrap->type == STAT_MSG) {
                    struct Stat s;
                    ret = handle_stat(&msg, pathname_buf, &s);

                    if (ret != -1) {
                        CopyTo(pid, msg.p2, &s, sizeof(s));
                    }
                } else if (wrap->type == READLINK_MSG) {
                    //TODO
                }
            }
        }

        if (READ_WRITE_MSG_LOW <= wrap->type && wrap->type <= READ_WRITE_MSG_HIGH) {
            read_write_msg msg;
            CopyFrom(pid, &msg, wrap->msg, sizeof(msg));

            if (msg.inode_id >= 1 && msg.inode_id <= _num_inodes &&
                request_ic(msg.inode_id)->data.type != INODE_FREE &&
                request_ic(msg.inode_id)->data.reuse == msg.reuse) {
                switch (wrap->type) {
                case READ_MSG:
                    ret = handle_read(&msg, pid);
                    break;
                case WRITE_MSG:
                    if (request_ic(msg.inode_id)->data.type == INODE_DIRECTORY) {
                        //can't write the direcotry
                        break;
                    }
                    ret = handle_write(&msg, pid);
                    break;
                default:
                    break;
                }
            }
        }

        if (wrap->type == SYNC_MSG) {
            ret = 0;
            sync_cache();
        }

        if (wrap->type == SHUTDOWN_MSG) {
            printf("shutting down yfs server...\n");
            printf("syncing cache...\n");
            sync_cache();
            printf("done syncing\n");
            printf("the file tree looks like: \n");
            print_tree(1, 0, 1);
            printf("going to exit...\n");
            Exit(0);
        }

        if (wrap->type == GETFILEZIE_MSG) {
            get_file_size_msg msg;
            //copy the real message here
            CopyFrom(pid, &msg, wrap->msg, sizeof(msg));
            ic_entry *e = request_ic(msg.inode_id);
            if (msg.inode_id >= 1 && msg.inode_id <= _num_inodes &&
                e->data.type != INODE_FREE &&
                e->data.reuse == msg.reuse) {
                ret = e->data.size;
            }
        }

        rep.val = ret;
        //only for test
        sync_cache();
        print_tree(1, 0, 1);
        Reply(&rep, pid);
    }
}