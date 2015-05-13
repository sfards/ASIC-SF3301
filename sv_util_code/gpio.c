#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define GPIO_RESET "/sys/class/gpio/gpio4/value"

void gpio_reset(void)
{
    int fd = open(GPIO_RESET,O_RDWR);
    int len = 0;
    if (fd < 0)
    {
        return;
    }

    len = write(fd,"0",1);
    usleep(500000);
    len = write(fd,"1",1);
    //usleep(500000);
    len = len;
    sleep(2);
}

