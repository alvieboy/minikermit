#include <pty.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "minikermit.h"
#include <unistd.h>
#include <fcntl.h>

#define DEBUG(x...) fprintf(stderr, x)

int master;

int kopen(const char *c)
{
    return open(c, O_RDONLY);
}
int kread(int fd, uint8_t *target, int len)
{
    int actual;
    actual = read(fd, target, len);

    DEBUG("Reading %d bytes: %d", len, actual);

    return actual;
}

int kclose(int fd)
{
    return close(fd);
}

int ktx(const char c)
{
    return write(master, &c, 1);
}

int krx(int timeout)
{
    uint8_t c;

    (void)timeout;

    if (read(master, &c,1 )<0)
        return -1;
    return c;
}

int main(int argc, char **argv)
{
    int slave;
    struct kermit k;
    char name[128];

    if (argc<2)
        return -1;

    const struct kermit_ops ops =
    {
        .open = kopen,
        .read = kread,
        .close = kclose,
        .tx = ktx,
        .rx = krx
    };

    openpty(&master, &slave, name, NULL, NULL);

    printf("%s\n", name);
    sleep(5);

    minikermit_init(&k, &ops);

    minikermit_sendfile(&k, argv[1]);

}
