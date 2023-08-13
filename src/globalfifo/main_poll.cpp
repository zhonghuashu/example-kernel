/**
 * @file main_poll.cpp
 * @brief Poll global_fifo if possible read / write using non-block.
 * @author Shu, Zhong Hua (77599567@qq.com)
 * @date 2023-07-30
 * @copyright Copyright (c) 2023 Siemens AG
 */
#include <iostream>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define FIFO_CLEAR 0x01

int main()
{
    int fd;
    int num;
    fd_set rfds;
    fd_set wfds;
    struct timeval timeout;

    // Open device file with non-block mode.
    if ((fd = ::open("/dev/globalfifo", O_RDONLY | O_NONBLOCK)) == -1)
    {
        std::cout << "Device open failure\n";
        return EXIT_FAILURE;
    }

    if (::ioctl(fd, FIFO_CLEAR) < 0)
    {
        std::cout << "ioctl command failed\n";
    }

    while (1)
    {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(fd, &rfds);
        FD_SET(fd, &wfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 00;

        ::select(fd + 1, &rfds, &wfds, nullptr, &timeout);

        if (FD_ISSET(fd, &rfds))
        {
            std::cout << "poll: data can be read\n";
        }

        if (FD_ISSET(fd, &wfds))
        {
            std::cout << "poll: data can be write\n";
        }
    }

    return EXIT_SUCCESS;
}