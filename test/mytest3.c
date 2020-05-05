#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * This test make sure the file name length is gauranteed
 */
int main(int argc, void **argv) {
    char name30[40];
    name30[0] = '/';
    int i;
    for (i = 1; i <= 26; i++) {
        name30[i] = 'a' + i - 1;
    }
    for (i = 27; i <= 38; i++) {
        name30[i] = '0' + i - 27;
    }
    name30[33] = '\0';
    //suppose to fail
    printf("MkDir ret: %d, should be -1\n", MkDir(name30));
    name30[31] = '\0';
    //suppose to success
    printf("Create ret: %d, should be 0\n", Create(name30));
    name30[31] = '0';
    //suppose to fail;
    printf("Unlink ret: %d, should be -1\n", Unlink(name30));

    char name300[302];
    for (i = 0; i < 256; i++) {
        name300[i] = '0';
    }
    name300[256] = '\0';
    //suppose to fail directly without going to the sever
    printf("MkDir ret: %d, should be -1\n", MkDir(name300));
    Shutdown();
    return 0;
}