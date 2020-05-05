#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * This test make sure the link and unlink is working
 */
static void test(int a, int b) {
    printf("%d, should be %d\n", a, b);
}
int main(int argc, void **argv) {
    test(MkDir("dir_0"), 0);
    test(MkDir("dir_0/dir_1"), 0);
    test(MkDir("dir_0/dir_1/dir_2"), 0);
    test(ChDir("../../"), 0);
    test(MkDir("dir_a"), 0);
    test(MkDir("dir_a/dir_b"), 0);
    test(MkDir("dir_a/dir_b/dir_c"), 0);
    int fd1 = Create("/dir_0/file_0");
    test(Link("/dir_0/file_0", "/dir_a/file_a"), 0);
    test(Link("/dir_0/file_0", "/dir_a/dir_b/file_b"), 0);
    test(Link("/dir_0/file_0", "/dir_a/dir_b/dir_c/file_c"), 0);
    int fd2 = Open("/dir_a/dir_b/file_b");
    test(fd2, 1);
    char read_buf[512];
    char write_buf[26];
    int i;
    for(i = 0; i < 26; i++) {
        write_buf[i] = 'a' + i;
    }
    test(Write(fd2, write_buf, 10), 10);
    test(Read(fd1, read_buf, 20), 10);
    printf("%.*s\n", 10, read_buf);
    Close(fd2);
    Unlink("/dir_a/dir_b/file_b");
    test(Open("/dir_a/dir_b/file_b"), -1);
    fd2 = Create("/dir_a/dir_b/file_b");
    test(fd2, 1);
    Unlink("/dir_a/file_a");
    Unlink("/dir_a/dir_b/dir_c/file_c");
    Seek(fd1, 0, SEEK_SET);
    test(Read(fd1, read_buf, 20), 10);
    printf("%.*s\n", 10, read_buf);
    Seek(fd1, 0, SEEK_SET);
    test(ChDir("/dir_0"), 0);
    test(Unlink("file_0"), 0);
    test(Read(fd1, read_buf, 20), -1);
    test(Write(fd1, write_buf, 10), -1);

    //test truncate
    Seek(fd2, 10000, SEEK_SET);
    test(Write(fd2, write_buf, 10), 10);
    struct Stat s;
    Stat("../dir_a/dir_b/file_b", &s);
    test(s.size, 10010);
    fd2 = Create("../dir_a/dir_b/file_b");

    Stat("../dir_a/dir_b/file_b", &s);
    test(s.size, 0);
    Stat(".", &s);
    test(s.inum, 47);
    test(Close(10), -1);
    Sync();
    Shutdown();
    return 0;
}