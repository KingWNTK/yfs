#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, void **argv) {
    char *write_buf = "0123456789abcdefghijklmnopqrstunwxyz";
    Write(1, write_buf, 1024 * 20);
    char read_buf[512];
    Read(1, read_buf, 1024 * 10);
    return 0;
}