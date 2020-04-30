#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, void **argv) {
    int i;
    
    struct Stat s;
    for (i = 1; i < argc; i++) {
        Stat(argv[i], &s);
        printf("inum: %d, type: %d, size: %d, nlink: %d\n", s.inum, s.type, s.size, s.nlink);
    }
}
