#include <iostream>
#include <unistd.h>
#include <fcntl.h>

int main()
{
    int fd;
    int counter = 0;
    int oldCounter = 0;

    if ((fd = ::open("/dev/second", O_RDONLY)) == -1)
    {
        std::cout << "Device open failure\n";

        return EXIT_FAILURE;
    }

    while (1)
    {
        ::read(fd, &counter, sizeof(unsigned int));

        if (counter != oldCounter)
        {
            std::cout << "Seconds after open /dev/second: " << counter << "\n";
            oldCounter = counter;
        }
    }

    return EXIT_SUCCESS;
}