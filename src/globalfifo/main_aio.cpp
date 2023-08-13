/**
 * @file main_aio.cpp
 * @brief Test async I/O notify from globalfifo.
 * @author Shu, Zhong Hua (77599567@qq.com)
 * @date 2023-07-30
 * @copyright Copyright (c) 2023 Siemens AG
 */
#include <iostream>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_LEN 100
void sigioHandler(int signum)
{
    std::cout << "Receive a signal from globalfifo: " << signum << "\n";
}

int main(void)
{
	int oflags;
    int fd;

    if ((fd = ::open("/dev/globalfifo", O_RDWR, S_IRUSR | S_IWUSR)) == -1)
    {
        return EXIT_FAILURE;
    }

    // Connect SIGIO signal to handler function.
    ::signal(SIGIO, sigioHandler);
    // Set current process as owner to receive signal.
    ::fcntl(fd, F_SETOWN, getpid());
    // Enable async notify signal.
    oflags = ::fcntl(fd, F_GETFL);
    ::fcntl(fd, F_SETFL, oflags | FASYNC);

    while (1)
    {
        ::sleep(1000);
    }

    return EXIT_SUCCESS;
}
