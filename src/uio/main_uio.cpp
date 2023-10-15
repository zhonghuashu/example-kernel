#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define UIO_DEV "/dev/uio0"
#define UIO_ADDR "/sys/class/uio/uio0/maps/map0/addr"
#define UIO_SIZE "/sys/class/uio/uio0/maps/map0/size"

static char uio_addr_buf[16], uio_size_buf[16];

int main(void)
{
    int uio_size;
    void *uio_addr, *access_address;

    int uio_fd = open(UIO_DEV, O_RDWR);
    int addr_fd = open(UIO_ADDR, O_RDONLY);
    int size_fd = open(UIO_SIZE, O_RDONLY);

    if (addr_fd < 0 || size_fd < 0 || uio_fd < 0) {
        fprintf(stderr, "mmap: %s\n", strerror(errno));
        exit(-1);
    }
    read(addr_fd, uio_addr_buf, sizeof(uio_addr_buf));
    read(size_fd, uio_size_buf, sizeof(uio_size_buf));
    uio_addr = (void *)strtoul(uio_addr_buf, NULL, 0);
    uio_size = (int)strtol(uio_size_buf, NULL, 0);

    access_address = mmap(NULL, uio_size, PROT_READ | PROT_WRITE, MAP_SHARED, uio_fd, 0);
    if (access_address == (void *)-1) {
        printf("mmap: %s\n", strerror(errno));
        exit(-1);
    }
    printf("The device address %p (lenth %d)\n"
           "logical address %p\n",
           uio_addr, uio_size, access_address);
    for (int i = 0; i < 6; i++) {
        printf("%c", ((char *)access_address)[i]);
        ((char *)access_address)[i] += 1;
    }
    printf("\n");

    for (int i = 0; i < 6; i++)
        printf("%c", ((char *)access_address)[i]);
    printf("\n");

    munmap(access_address, uio_size);
    return 0;
}