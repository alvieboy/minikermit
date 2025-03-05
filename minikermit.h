#ifndef MINIKERMIT_H__
#define MINIKERMIT_H__

#define KERMIT_MAX_RX_BUF 90
#define KERMIT_MAX_TX_BUF 90
#define KERMIT_MAX_FILENAME (32)

#define K_TYPE_INIT 'I'
#define K_TYPE_SINIT 'S'
#define K_TYPE_FILEHEADER 'F'
#define K_TYPE_DATA 'D'
#define K_TYPE_EOF 'Z'
#define K_TYPE_BREAK 'B'
#define K_MARK '\001'
#define K_EOP '\r'

typedef enum
{
    WAIT_MARK,
    WAIT_LEN,
    WAIT_SEQ,
    WAIT_TYPE,
    WAIT_DATA,
    WAIT_CHECK,
    WAIT_END
} kparser_state_t;

typedef enum
{
    K_STATE_IDLE,
    K_STATE_INIT_SEND,
    K_STATE_SEND_FILENAME,
    K_STATE_SEND_FILEDATA,
    K_STATE_SEND_EOF,
    K_STATE_SEND_BREAK
} kstate_t;


struct kermit_ops
{
    int (*open)(const char *c);
    int (*read)(int, uint8_t *target, int len);
    int (*close)(int);
    int (*tx)(const char c);
    int (*rx)(int timeout);
};

struct send_settings_basic
{
    uint8_t maxl;
    uint8_t timo;
    uint8_t npad;
    uint8_t padc;
    uint8_t eol;
    char qctl;
};

struct send_settings_extended
{
    /* Optional settings */
    char ebq;
    uint8_t bct;
    char rpt;
    uint8_t capas;
    uint8_t wslots;
    uint8_t maxlx1;
    uint8_t maxlx2;
    char chkpnt;
    char chkinf1;
    char chkinf2;
    char chkinf3;
    uint8_t whatami;
    uint8_t sysidl;
    char sysid1;
    char sysid2;
    uint8_t whatami2;
};

struct send_settings
{
    struct send_settings_basic basic;
    struct send_settings_extended extended;
};

struct kermit
{
    const struct kermit_ops *ops;
    kparser_state_t parser_state;
    /* Receive state */
    kstate_t state;
    uint16_t len;
    uint16_t proc_len;
    uint8_t seq;
    char type;
    uint32_t cksum;
    char filename[KERMIT_MAX_FILENAME+1];
    struct send_settings peer_settings;
    uint8_t rxdata[KERMIT_MAX_RX_BUF];
    uint8_t txdata[KERMIT_MAX_TX_BUF];
    uint16_t txptr;
    int fd;
    bool is_eof;
};

void minikermit_init(struct kermit *k, const struct kermit_ops *ops);
int minikermit_sendfile(struct kermit *k, const char *name);

#endif
