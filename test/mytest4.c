#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * This test make sure the read and write are working
 */
static void test(int a, int b) {
    printf("%d, should be %d\n", a, b);
}
int main(int argc, void **argv) {
    test(MkDir("my_dir"), 0);
    test(ChDir("my_dir"), 0);
    int fd = Create("../my_dir/my_file");
    test(fd, 0);
    char read_buf[512];
    test(Read(fd, read_buf, 512), 0);
    char write_buf[1500];
    int i;
    memset(write_buf, 0, sizeof(write_buf));
    for(i = 0; i < 26; i++) {
        write_buf[i] = 'a' + i;
        write_buf[1020 + i] = 'A' + i;
    }
    //Check read write and seek
    test(Write(fd, write_buf, 1500), 1500);
    test(Seek(fd, 0, SEEK_SET), 0);
    test(Read(fd, read_buf, 512), 512);
    printf("%.*s\n", 26, read_buf);
    test(Seek(fd, -480, SEEK_END), 1020);
    test(Read(fd, read_buf, 512), 480);
    printf("%.*s\n", 26, read_buf);
    test(Unlink("../my_dir/my_file"), 0);
    test(Seek(fd, 0, SEEK_SET), 0);
    //Check the we are openning file correctly
    for(i = 1; i < 16; i++) {
        char tmp[2];
        tmp[0] = 'a' + i;
        tmp[1] = '\0';
        int fd = Create(tmp);
        test(fd, i);
    }
    int tmp = Create("abc");
    test(tmp, -1);
    test(Read(fd, read_buf, 512), ERROR);
    
    Close(10);
    fd = Create("abc");
    test(fd, 10);
    test(Read(fd, read_buf, 512), 0);
    
    //Check we can create holes and write these holes later
    Seek(fd, 5000, SEEK_SET);
    test(Write(fd, write_buf, 20), 20);
    test(Seek(fd, 1020, SEEK_SET), 1020);
    test(Write(fd, write_buf, 20), 20);
    Seek(fd, -20, SEEK_CUR);
    test(Read(fd, read_buf, 512), 512);
    printf("%.*s\n", 20, read_buf);
    Unlink("abc");
    Close(fd);
    fd = Create("abcd");
    test(Read(fd, read_buf, 512), -1);
    test(fd, 10);
    test(Read(fd, read_buf, 512), 0);
    test(Seek(fd, 0, SEEK_END), 0);
    Shutdown();
    return 0;
}