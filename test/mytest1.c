#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * This test make sure that the reuse number is working
 */

static void test(int a, int b) {
    printf("%d, should be %d\n", a, b);
}

int main(int argc, void **argv) {
    MkDir("b");
    ChDir("b");
    RmDir("../b");
    test(ChDir(".."), -1);
    ChDir("/");
    MkDir("./../..///./.././c");
    ChDir("c");
    RmDir("../c");
    test(ChDir(".."), -1);
    Shutdown();
    return 0;
}