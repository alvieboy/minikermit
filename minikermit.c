#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "minikermit.h"
#include "minikermit.h"

#define DEBUG(x...) fprintf(stderr, x)

#define MAXL 64
#define TIMO 2
#define NPAD 0
#define PADC 0
#define EOL 0
#define QCTL '#'
#define EBQ '#'


static void minikermit_send(struct kermit *k,
                            char type,
                            int seq,
                            const uint8_t *payload,
                            uint16_t len);

static inline char tochar(int x)
{
    if (x>94)
        abort();
    return ' ' + x;
}

static inline char ctl(char x)
{
    return x ^ (1<<6);
}

static inline int unchar(char c)
{
    int v = c - ' ';
    if (v<0) {
        printf("Cannot unchar 0x%02x, result %d", (unsigned)c, v);
        abort();
    }

    return v;
}

struct send_settings_basic local_settings =
{
    .maxl = MAXL,
    .timo = TIMO,
    .npad = NPAD,
    .padc = PADC,
    .eol = EOL,
    .qctl = QCTL,
};

static void minikermit_send_sinit(struct kermit *k, const struct send_settings_basic *s)
{
    uint8_t payload[sizeof(struct send_settings_basic)];
    int c = 0;
    payload[c++] = tochar(s->maxl);
    payload[c++] = tochar(s->timo);
    payload[c++] = tochar(s->npad);
    payload[c++] = tochar(s->padc);
    payload[c++] = tochar(s->eol);
    payload[c++] = s->qctl;

    /*
     Extended settings not implemented

    payload[c++] = s->ebq;
    payload[c++] = tochar(s->bct);
    payload[c++] = s->rpt;
    payload[c++] = tochar(s->capas);
    payload[c++] = tochar(s->wslots);
    payload[c++] = tochar(s->maxlx1);
    payload[c++] = tochar(s->maxlx2);
    payload[c++] = s->chkpnt;
    payload[c++] = s->chkinf1;
    payload[c++] = s->chkinf2;
    payload[c++] = s->chkinf3;
    payload[c++] = tochar(s->whatami);
    payload[c++] = tochar(s->sysidl);
    payload[c++] = s->sysid1;
    payload[c++] = s->sysid2;
    payload[c++] = tochar(s->whatami2);
    */
    minikermit_send(k, K_TYPE_SINIT, 0, payload, sizeof(payload));
}


static void txc(struct kermit *k, const char c, uint32_t *checksum)
{
    if (checksum) {
        *checksum = (*checksum + (uint32_t)c);
    }
    // putc(c, stdout);
    k->ops->tx(c);
}

static void txcl(struct kermit *k, const uint8_t *c, int len, uint32_t *checksum)
{
    while (len--) {
        txc(k, *c++, checksum);
    }
}

static void minikermit_send(struct kermit *k,
                            char type,
                            int seq,
                            const uint8_t *payload,
                            uint16_t len)
{
    bool extended = false;
    uint32_t check = 0;

    unsigned total_len = len + 3;

    if (total_len>95) {
        total_len+=2; // Additional len
        extended = true;
    }
    check = 0;

    txc(k, K_MARK, NULL);

    if (extended) {
        abort();
        //tx(' ');
    } else {
        DEBUG("Transmitting a packet with %d bytes, sequence %d", total_len, seq);
        txc(k, tochar(total_len), &check);
        txc(k, tochar(seq), &check);
        txc(k, type, &check);
        txcl(k, payload, len, &check);

        check = (((check & 0300) >> 6) + check) & 077;

        txc(k, tochar(check), NULL);
        txc(k, K_EOP, NULL);
    }
    printf("\n\n\nok\n");
}

#define DEBUG(x...) fprintf(stderr, x)

void minikermit_error(struct kermit *k)
{
    DEBUG("Kermit error\n");
    k->state = K_STATE_IDLE;
}

static bool minikermit_is_control(unsigned char c)
{
    return (((c & 0x60)==0) || (c==0xff));
}

static void minikermit_data(struct kermit *k, char c)
{
    DEBUG("KData: %c\n", c);
    k->rxdata[k->proc_len++] = c;
}

static void minikermit_send_filename(struct kermit *k)
{
    minikermit_send(k, K_TYPE_FILEHEADER, k->seq, (uint8_t*)k->filename, strlen(k->filename));
}

static void minikermit_send_eof(struct kermit *k)
{
    DEBUG("Sending EOF\n");
    minikermit_send(k, K_TYPE_EOF, k->seq, NULL,0);
}

static void minikermit_send_break(struct kermit *k)
{
    DEBUG("Sending BREAK\n");
    minikermit_send(k, K_TYPE_BREAK, k->seq, NULL,0);
}

static void minikermit_send_chunk(struct kermit *k)
{
    uint8_t temp[KERMIT_MAX_TX_BUF/2];

    while (1)
    {
        // Worst case is we need to escape everything.
        unsigned allowed_size = (KERMIT_MAX_TX_BUF - k->txptr) >> 1;

        if (allowed_size<1)
            break;

        int r = k->ops->read(k->fd, temp, allowed_size);

        if(r==0) {
            k->is_eof = true;
            break;
        }

        if (r<0)
            abort();

        for (int i=0; i<r; i++)
        {
            char c = temp[i];
            if (minikermit_is_control(c))
            {
                k->txdata[k->txptr++] = local_settings.qctl;
                k->txdata[k->txptr++] = ctl(c);
            } else if (c==local_settings.qctl) {
                k->txdata[k->txptr++] = local_settings.qctl;
                k->txdata[k->txptr++] = c;
            } else {
                k->txdata[k->txptr++] = c;
            }
        }
    }

    minikermit_send(k, K_TYPE_DATA, k->seq, k->txdata, k->txptr);
    k->txptr = 0;
}


static void minikermit_ack(struct kermit*k)
{
    int pos = 0;
    k->seq++;
    k->seq %= 64;
    DEBUG("Got ACK\n");
    switch (k->state)
    {
    case K_STATE_INIT_SEND:

        DEBUG("Link init ack received len=%d %ld %ld",
              k->len,
              sizeof(struct send_settings_basic),
              sizeof(struct send_settings));
        if (k->len == sizeof(struct send_settings_basic) ||
            (k->len == sizeof(struct send_settings)))
        {
            // Decode basic
            k->peer_settings.basic.maxl = unchar(k->rxdata[pos++]);
            k->peer_settings.basic.timo = unchar(k->rxdata[pos++]);
            k->peer_settings.basic.npad = unchar(k->rxdata[pos++]);
            k->peer_settings.basic.padc = ctl(k->rxdata[pos++]);
            k->peer_settings.basic.eol = ctl(k->rxdata[pos++]);
            k->peer_settings.basic.qctl = k->rxdata[pos++];
        }
        else
        {
            minikermit_error(k);
        }
        k->state =  K_STATE_SEND_FILENAME;
        minikermit_send_filename(k);
        break;
    case K_STATE_SEND_FILENAME:
        k->state = K_STATE_SEND_FILEDATA;
        k->txptr = 0;
        minikermit_send_chunk(k);
        break;
    case K_STATE_SEND_FILEDATA:
        if (k->is_eof) {
            k->state = K_STATE_SEND_EOF;
            minikermit_send_eof(k);
        } else {
            minikermit_send_chunk(k);
        }
        break;
    case K_STATE_SEND_EOF:
        minikermit_send_break(k);
        k->state = K_STATE_SEND_BREAK;
        break;
    case K_STATE_SEND_BREAK:
        k->state = K_STATE_IDLE;
        break;
    default:
        printf("Invalid ack in state %d\n", k->state);
        abort();
    }
}

void minikermit_nack(struct kermit*k)
{
    // TBD
    (void)k;
}

void minikermit_packet(struct kermit *k)
{
    switch (k->type)
    {
    case 'Y':
        minikermit_ack(k);
        break;
    case 'N':
        minikermit_nack(k);
        break;
    }
}


void minikermit_parse(struct kermit *k, char c)
{
    switch (k->parser_state)
    {
    case WAIT_MARK:
        if (c=='\001') {
            DEBUG("Wait len\n");
            k->parser_state = WAIT_LEN;
            k->cksum = 0;
        }
        break;
    case WAIT_LEN:
        if (!isascii(c)) {
            minikermit_error(k);
            k->parser_state = WAIT_MARK;
        } else {
            k->cksum+=c;
            k->len = unchar(c);
            if (k->len<3) {

                minikermit_error(k);
                break;
            }

            k->len-=3;

            DEBUG("LEN=%d\n", k->len);

            k->parser_state = WAIT_SEQ;
        }
        break;

    case WAIT_SEQ:
        if (!isascii(c)) {
            minikermit_error(k);
            k->parser_state = WAIT_MARK;
        } else {
            k->cksum+=c;

            k->seq = unchar(c);
            DEBUG("SEQ=%d\n", k->seq);
            k->parser_state = WAIT_TYPE;
        }
        break;
    case WAIT_TYPE:
        if (!isascii(c)) {
            minikermit_error(k);
            k->parser_state = WAIT_MARK;
        } else {
            k->cksum+=c;
            k->type = c;
            DEBUG("TYPE=%c\n", k->type);

            k->parser_state = WAIT_DATA;
            k->proc_len = 0;
            if (k->len==0)
            {
                minikermit_packet(k);
                k->parser_state = WAIT_MARK;
            }
        }
        break;
    case WAIT_DATA:
        k->cksum+=c;
        minikermit_data(k, c);
        if (k->proc_len >= k->len) {
            k->parser_state = WAIT_CHECK;
        }
        break;
    case WAIT_CHECK:
        k->cksum = (((k->cksum & 0300) >> 6) + k->cksum) & 077;
        DEBUG("SUM expect %d got %d\n", k->cksum, unchar(c));
        k->parser_state = WAIT_END;
        break;
    case WAIT_END:
        if (c != 0x0d)
        {
            minikermit_error(k);
        } else {
            minikermit_packet(k);
        }
        k->parser_state = WAIT_MARK;
        break;
    }
}

void minikermit_init(struct kermit *k, const struct kermit_ops *ops)
{
    k->ops = ops;
    k->parser_state = WAIT_MARK;
    k->state = K_STATE_IDLE;
}

int minikermit_sendfile(struct kermit *k, const char *name)
{
    int r = 0;

    k->state = K_STATE_INIT_SEND;

    k->fd = k->ops->open(name);

    if (k->fd<0)
        return k->fd;

    k->is_eof = false;

    strncpy(k->filename, name, 16);
    k->filename[16] = '\0';

    minikermit_send_sinit(k, &local_settings);

    do {
        char c = k->ops->rx(1000);
        minikermit_parse(k, c);
        if (k->state == K_STATE_IDLE)
            break;
    } while (1);
    return r;
}
