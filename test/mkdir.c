#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, void **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        MkDir(argv[i]);
    }
    // char tmp[20];
    // for(i = 0; i < 26; i++) {
    // 	tmp[0] = 'a' + i;
    // 	tmp[1] = '\0';
    // 	MkDir(tmp);
    // }
    // for(i = 0; i < 20; i++) {
    // 	tmp[0] = 'A' + i;
    // 	tmp[1] = '\0';
    // 	MkDir(tmp);
    // }
}
