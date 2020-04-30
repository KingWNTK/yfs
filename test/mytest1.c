#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * This test make sure that the reuse number is working
 */
int main(int argc, void **argv) {
    MkDir("b");
    ChDir("b");
    RmDir("../b");
    ChDir("..");
    ChDir("/");
    MkDir("./../..///./.././c");
    ChDir("c");
    RmDir("../c");
    ChDir("..");
    return 0;
}