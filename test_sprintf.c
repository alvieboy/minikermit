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

struct p_context
{
    int count;
    int max;
    char line[128];
    unsigned linelen;
    unsigned linepos;
};

struct p_context contexts[1];


int kopen(const char *c)
{
    (void)c;
    contexts[0].count = 0;
    contexts[0].max = 128;
    contexts[0].linelen = 0;
    contexts[0].linepos = 0;

    return 0;
}

static int do_print_line(struct p_context *ctx)
{
    if (ctx->count < ctx->max)
    {
        int r = sprintf(ctx->line, "This is sequence %d with a very large line so it does "
                        "not fit within the packet size. Again, seq %d\r\n",
                        ctx->count,
                        ctx->count);
        ctx->count++;
        return r;
    } else {
        return 0;
    }
}

int kread(int fd, uint8_t *target, int len)
{
    struct p_context *p = &contexts[fd];

    if (p->linepos && p->linelen) {
        /* Send remainder */
        int queued = p->linelen - p->linepos;
        int tlen = queued > len ? len : queued;
        memcpy(target, &p->line[p->linepos], tlen);
        p->linepos += tlen;
        if (p->linepos >= p->linelen) {
            // Fill next;
            p->linelen = do_print_line(p);
            p->linepos = 0;
        }
        return tlen;
    }
    else
    {
        p->linelen = do_print_line(p);
        p->linepos = 0;
        int tlen = (int)p->linelen > len ? len : (int)p->linelen;
        if (tlen>0)
            memcpy(target, &p->line[p->linepos], tlen);

        p->linepos += tlen;
        if (p->linepos >= p->linelen) {
            p->linelen = 0;
            p->linepos = 0;
        }
        return tlen;
    }
}
int kclose(int fd)
{
    (void)fd;
    return 0;
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

int main()
{
    int slave;
    struct kermit k;
    char name[128];

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

    minikermit_sendfile(&k, "test.dat");

    return 0;
}
