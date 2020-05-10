#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>


char *to_string(int x) {
    int len = 0;
    int t = x;
    while(t) {
        t /= 10;
        len ++;
    }
    char *ret = (char *)malloc(len + 1);
    t = x;
    int cnt = 0;
    while(t) {
        ret[len - 1 - cnt] = '0' + t % 10;
        t /= 10;
        cnt++;
    }
    ret[len] = '\0';
    return ret;
}
/**
 * This test make sure the indirect blocks are loaded properly
 */
static void test(int a, int b) {
    printf("%d, should be %d\n", a, b);
}
int main(int argc, void **argv) {
    MkDir("/test");
    Create("/test/a");
    int i;
    ChDir("/test");
    for(i = 1; i <= 250; i++) {
        char *s = to_string(i);
        Link("/test/a", s);
        free(s);
    }
    char *s = to_string(2);
    ChDir("/..");
    Unlink("/1");
    Unlink("/test/2");
    Unlink("/test/3");
    ChDir("../././///test");
    Unlink("./4");
    Unlink("./2");
    Unlink("/test/5");
    test(RmDir("test"), -1);
    Shutdown();
    free(s);
    return 0;
}