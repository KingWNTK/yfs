#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <comp421/iolib.h>
#include "../public/msg.h"

int handle_mkdir(single_ptr_msg *msg, char *pathname_buf);

int handle_rmdir(single_ptr_msg *msg, char *pathname_buf);

int hanlde_chdir(single_ptr_msg *msg, char *pathname_buf, int *new_dir, int *reuse);

int handle_create(single_ptr_msg *msg, char *pathname_buf, int *inode_id, int *reuse);

int handle_open(single_ptr_msg *msg, char *pathname_buf, int *inode_id, int *reuse);

int handle_link(double_ptr_msg *msg, char *pathname_buf, char *new_pathname_buf);

int handle_unlink(single_ptr_msg *msg, char *pathname_buf);

int handle_read(read_write_msg *msg, int pid);

int handle_write(read_write_msg *msg, int pid);

int handle_stat(double_ptr_msg *msg, char *pathname_buf, struct Stat *stat);

#endif