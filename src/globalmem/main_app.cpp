#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include<sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <assert.h>
#include <sys/epoll.h>

#define EPOLL_SIZE ( 256 )
#define MAX_EVENTS (  20 )

/***************** read / write device *******************/
int8_t write_buf[1024];
int8_t read_buf[1024];
#define GLOBALMEM_MAGIC 'g'
#define MEM_CLEAR _IOW(GLOBALMEM_MAGIC, 0x01, int32_t*)

/***************** signal handler *******************/
#define REG_CURRENT_TASK _IOW(GLOBALMEM_MAGIC, 0x02, int32_t*)
#define SIGETX 44
static int done = 0;
int check = 0;

static void ctrl_c_handler(int n, siginfo_t *info, void *unused)
{
    if (n == SIGINT) {
        printf("\nRecieved ctrl-c\n");
        done = 1;
    }
}

static void sig_event_handler(int n, siginfo_t *info, void *unused)
{
    if (n == SIGETX) {
        int check = info->si_int;
        printf ("Received signal from kernel : Value =  %u\n", check);
    }
}

static void register_event_handler()
{
    struct sigaction act;

    /* install ctrl-c interrupt handler to cleanup at exit */
    sigemptyset (&act.sa_mask);
    act.sa_flags = (SA_SIGINFO | SA_RESETHAND);
    act.sa_sigaction = ctrl_c_handler;
    sigaction (SIGINT, &act, NULL);

    /* install custom signal handler */
    sigemptyset(&act.sa_mask);
    act.sa_flags = (SA_SIGINFO | SA_RESTART);
    act.sa_sigaction = sig_event_handler;
    sigaction(SIGETX, &act, NULL);

    printf("Installed signal handler for SIGETX = %d\n", SIGETX);
}

static void poll_main(int fd)
{
    char kernel_val[20];
    int ret;
    struct pollfd pfd;

    pfd.fd = fd;
    pfd.events = (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM);

    while (1) {
        puts("Starting poll...");

        ret = poll(&pfd /* file descriptors array */, (unsigned long)1 /* file desc number */, 5000 /* timeout */);

        if (ret < 0) {
            perror("poll");
            assert(0);
        }

        // Device can be read.
        if ((pfd.revents & POLLIN) == POLLIN) {
            lseek(pfd.fd, 0, SEEK_SET);
            read(pfd.fd, kernel_val, sizeof(kernel_val));
            printf("POLLIN : Kernel_val = %s\n", kernel_val);
        }

        // Device can be written.
        if ((pfd.revents & POLLOUT) == POLLOUT) {
            strcpy(kernel_val, "User space");
            lseek(fd, 0, SEEK_SET);
            write(pfd.fd, kernel_val, strlen(kernel_val) + 1);
            printf("POLLOUT : Kernel_val = %s\n", kernel_val);
        }
    }

    if (close(fd)) {
        perror("Failed to close file descriptor\n");
    }
}

static void select_main(int fd)
{
    char kernel_val[20];
    fd_set read_fd, write_fd;
    struct timeval timeout;
    int ret;

    while (1) {
        puts("Starting Select...");

        /* Initialize the file descriptor set. */
        FD_ZERO(&read_fd);
        FD_SET(fd, &read_fd);
        FD_ZERO(&write_fd);
        FD_SET(fd, &write_fd);

        /* Initialize the timeout */
        timeout.tv_sec = 5; // 5 Seconds
        timeout.tv_usec = 0;

        ret = select(FD_SETSIZE, &read_fd, &write_fd, NULL, &timeout);

        if (ret < 0) {
            perror("select");
            assert(0);
        }

        if (FD_ISSET(fd, &read_fd)) {
            lseek(fd, 0, SEEK_SET);
            read(fd, &kernel_val, sizeof(kernel_val));
            printf("READ : Kernel_val = %s\n", kernel_val);
        }

        if (FD_ISSET(fd, &write_fd)) {
            strcpy(kernel_val, "User Space");
            lseek(fd, 0, SEEK_SET);
            write(fd, &kernel_val, strlen(kernel_val) + 1);
            printf("WRITE : Kernel_val = %s\n", kernel_val);
        }
    }

    if (close(fd)) {
        perror("Failed to close file descriptor\n");
    }
}

static void epoll_main(int fd)
{
    char kernel_val[20];
    int epoll_fd, ret, n;
    struct epoll_event ev, events[20];

    // Create epoll instance
    epoll_fd = epoll_create(EPOLL_SIZE);

    if (epoll_fd < 0) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }

    ev.data.fd = fd;
    ev.events = (EPOLLIN | EPOLLOUT);

    // Add the fd to the epoll
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev)) {
        perror("Failed to add file descriptor to epoll\n");
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    while (1) {
        puts("Starting Epoll...");

        ret = epoll_wait(epoll_fd, events, MAX_EVENTS, 5000);

        if (ret < 0) {
            perror("epoll_wait");
            close(epoll_fd);
            assert(0);
        }

        for (n = 0; n < ret; n++) {
            if ((events[n].events & EPOLLIN) == EPOLLIN) {
                lseek(fd, 0, SEEK_SET);
                read(events[n].data.fd, &kernel_val, sizeof(kernel_val));
                printf("EPOLLIN : Kernel_val = %s\n", kernel_val);
            }

            if ((events[n].events & EPOLLOUT) == EPOLLOUT) {
                lseek(fd, 0, SEEK_SET);
                strcpy(kernel_val, "User Space");
                write(events[n].data.fd, &kernel_val, strlen(kernel_val) + 1);
                printf("EPOLLOUT : Kernel_val = %s\n", kernel_val);
            }
        }
    }

    if (close(epoll_fd)) {
        perror("Failed to close epoll file descriptor\n");
    }

    if (close(fd)) {
        perror("Failed to close file descriptor\n");
    }
}

int main()
{
    int fd;
    char option;
    printf("*********************************\n");

    register_event_handler();

    fd = open("/dev/globalmem", O_RDWR);
    if (fd < 0) {
        printf("Cannot open device file...\n");
        return 0;
    }

    while (1) {
        printf("****Please Enter the Option*************\n");
        printf("        1. Write                        \n");
        printf("        2. Read                         \n");
        printf("        3. Ioctl - MEM_CLEAR            \n");
        printf("        4. Ioctl - REG_CURRENT_TASK     \n");
        printf("        5. poll main                    \n");
        printf("        6. select main                  \n");
        printf("        7. epoll main                   \n");
        printf("        8. Exit                         \n");
        printf("****************************************\n");
        scanf(" %c", &option);
        printf("Your Option = %c\n", option);

        switch (option) {
        case '1':
            printf("Enter the string to write into driver :");
            scanf("  %[^\t\n]s", write_buf);
            printf("Data Writing ...");
            lseek(fd, 0, SEEK_SET);
            write(fd, write_buf, strlen((char *)write_buf) + 1);
            printf("Done!\n");
            break;
        case '2':
            printf("Data Reading ...");
            // Set offset to file beginning.
            lseek(fd, 0, SEEK_SET);
            read(fd, read_buf, 1024);
            printf("Done!\n\n");
            printf("Data = %s\n\n", read_buf);
            break;
        case '3':
            ioctl(fd, MEM_CLEAR, (int *) 0);
            break;
        case '4':
            ioctl(fd, REG_CURRENT_TASK, (int *) 0);
            break;
        case '5':
            poll_main(fd);
            break;
        case '6':
            select_main(fd);
            break;
        case '7':
            epoll_main(fd);
            break;
        case '8':
            close(fd);
            exit(1);
            break;
        default:
            printf("Enter Valid option = %c\n", option);
            break;
        }
    }
    close(fd);
}