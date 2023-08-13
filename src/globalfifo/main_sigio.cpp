/**
 * @file main_sigio.cpp
 * @brief Async I/O example.
 * @author Shu, Zhong Hua (77599567@qq.com)
 * @date 2023-07-30
 * @copyright Copyright (c) 2023 Siemens AG
 */
#include <iostream>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_LEN 100
void inputHandler(int signum)
{
	char data[MAX_LEN];
    int len = ::read(STDIN_FILENO, &data, MAX_LEN);
    data[len] = 0;

    std::cout << "Input available: " << data << "\n";
}

int main(void)
{
	int oflags;

    // Connect SIGIO signal to handler function.
    ::signal(SIGIO, inputHandler);
    // Set current process as owner to receive signal.
    ::fcntl(STDIN_FILENO, F_SETOWN, getpid());
    // Enable async notify signal.
    oflags = ::fcntl(STDIN_FILENO, F_GETFL);
    ::fcntl(STDIN_FILENO, F_SETFL, oflags | FASYNC);

    while (1);

    return EXIT_SUCCESS;
}
