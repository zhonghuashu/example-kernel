#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include<sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

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

void ctrl_c_handler(int n, siginfo_t *info, void *unused)
{
    if (n == SIGINT) {
        printf("\nRecieved ctrl-c\n");
        done = 1;
    }
}

void sig_event_handler(int n, siginfo_t *info, void *unused)
{
    if (n == SIGETX) {
        int check = info->si_int;
        printf ("Received signal from kernel : Value =  %u\n", check);
    }
}

void register_event_handler()
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
        printf("        5. Exit                         \n");
        printf("****************************************\n");
        scanf(" %c", &option);
        printf("Your Option = %c\n", option);

        switch (option) {
        case '1':
            printf("Enter the string to write into driver :");
            scanf("  %[^\t\n]s", write_buf);
            printf("Data Writing ...");
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