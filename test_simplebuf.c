
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#define DEVICE "/dev/simplebuf4"
#define BUFFER_SIZE 128

static int passed = 0;
static int failed = 0;

void check(const char *name, int condition)
{
    if (condition) {
        printf("%s : PASS\n", name);
        passed++;
    } else {
        printf("%s : FAIL\n", name);
        failed++;
    }
}

int main(void)
{
    int fd;
    ssize_t n;
    off_t pos;
    char buf[160] = {0};
    char data[150];
    char expected[128];
    void *bad_ptr;

    /* T01: Doc khi bo dem rong */
    fd = open(DEVICE, O_RDWR);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    n = read(fd, buf, 10);
    check("T01 - Empty buffer read", n == 0);

    close(fd);
    fd = open(DEVICE, O_RDWR);

    pos = lseek(fd, 0, SEEK_CUR);
    n = read(fd, buf, 0);

    check("T02 - Zero-length read",
          n == 0 && lseek(fd, 0, SEEK_CUR) == pos);

    n = write(fd, "X", 0);

    check("T02 - Zero-length write",
          n == 0 && lseek(fd, 0, SEEK_CUR) == pos);

    close(fd);
    fd = open(DEVICE, O_RDWR);

    n = write(fd, "ABCDEFGHIJ", 10);

    check("T03 - Write 10 bytes", n == 10);

    lseek(fd, 0, SEEK_SET);

    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, 4);

    check("T03 - Read back data",
          n == 4 && memcmp(buf, "ABCD", 4) == 0);

    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, 4);

    check("T04 - Repeated read 2",
          n == 4 && memcmp(buf, "EFGH", 4) == 0);

    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, 4);
    check("T04 - Partial read", n == 2 && memcmp(buf, "IJ", 2) == 0);
    n = read(fd, buf, 4);
    check("T04 - EOF", n == 0);
    pos = lseek(fd, 0, SEEK_SET);
    check("T08 - SEEK_SET", pos == 0);
    n = read(fd, buf, 3);
    check("T08 - Read ABC", n == 3 && memcmp(buf, "ABC", 3) == 0);
    pos = lseek(fd, 2, SEEK_CUR);
    check("T08 - SEEK_CUR", pos == 5);
    n = read(fd, buf, 2);
    check("T08 - Read FG", n == 2 && memcmp(buf, "FG", 2) == 0);
    pos = lseek(fd, -3, SEEK_END);
    check("T08 - SEEK_END", pos == 7);
    n = read(fd, buf, 3);
    check("T08 - Read HIJ", n == 3 && memcmp(buf, "HIJ", 3) == 0);
    lseek(fd, 4, SEEK_SET);
    n = write(fd, "xyz", 3);
    lseek(fd, 0, SEEK_SET);
    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, 10);
    check("T08 - Overwrite data",
          n == 10 && memcmp(buf, "ABCDxyzHIJ", 10) == 0);
    lseek(fd, 10, SEEK_SET);

    errno = 0;
    pos = lseek(fd, -1, SEEK_SET);
    check("T09 - Negative SEEK_SET",
          pos == -1 && errno == EINVAL);

    errno = 0;
    pos = lseek(fd, 11, SEEK_SET);
    check("T09 - SEEK_SET beyond data_size",
          pos == -1 && errno == EINVAL);

    errno = 0;
    pos = lseek(fd, 1, SEEK_END);
    check("T09 - SEEK_END beyond data_size",
          pos == -1 && errno == EINVAL);

    pos = lseek(fd, 0, SEEK_CUR);
    check("T09 - Offset unchanged after invalid seek",
          pos == 10);

    close(fd);
    bad_ptr = mmap(NULL, 4096, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (bad_ptr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    fd = open(DEVICE, O_RDWR);

    lseek(fd, 0, SEEK_SET);

    errno = 0;
    n = write(fd, bad_ptr, 3);

    check("T10 - Invalid write pointer",
          n == -1 && errno == EFAULT);

    pos = lseek(fd, 0, SEEK_CUR);
    check("T10 - Write offset unchanged",
          pos == 0);

    errno = 0;
    n = read(fd, bad_ptr, 3);

    check("T10 - Invalid read pointer",
          n == -1 && errno == EFAULT);

    pos = lseek(fd, 0, SEEK_CUR);
    check("T10 - Read offset unchanged",
          pos == 0);

    close(fd);
    munmap(bad_ptr, 4096);
    for (int i = 0; i < 150; i++)
        data[i] = 'A' + (i % 26);

    memcpy(expected, data, BUFFER_SIZE);
    fd = open(DEVICE, O_RDWR);

    n = write(fd, data, BUFFER_SIZE);

    check("T05 - Write exactly 128 bytes",
          n == 128);

    lseek(fd, 0, SEEK_SET);
    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, BUFFER_SIZE);

    check("T05 - Verify 128 bytes",
          n == 128 &&
          memcmp(buf, expected, BUFFER_SIZE) == 0);

    close(fd);
    fd = open(DEVICE, O_RDWR);

    n = write(fd, data, 150);

    check("T06 - Oversized write",
          n == 128);

    errno = 0;
    n = write(fd, "Z", 1);

    check("T07 - Full buffer ENOSPC",
          n == -1 && errno == ENOSPC);

    close(fd);
    {
        int ok = 1;

        for (int i = 0; i < 3; i++) {
            fd = open(DEVICE, O_RDWR);

            if (fd < 0) {
                ok = 0;
                break;
            }

            memset(buf, 0, sizeof(buf));
            n = read(fd, buf, 4);

            if (n != 4 || memcmp(buf, expected, 4) != 0)
                ok = 0;

            close(fd);

            if (!ok)
                break;
        }

        check("T11 - Repeated open/close",
              ok);
    }

    printf("\nSummary: %d passed, %d failed\n",
           passed, failed);

    return failed == 0 ? 0 : 1;
}
