#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, void **argv) {
    int i;
    for (i = 1; i < argc; i+=2) {
        Link(argv[i], argv[i + 1]);
    }
    return 0;
}