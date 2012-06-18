#include <debug.h>
#include <stdbool.h>
#include <ts7200.h>

#include <user/mio.h>
#include <user/tio.h>
#include <user/priorities.h>
#include <user/string.h>
#include <user/syscall.h>

/* For AwaitEvent() */
#define INT_UART1       52

/* Where is INT_UART1 actually defined */
#define SOFTINT_BASE    VIC2_BASE
/* Position in the software interrupt register */
#define SOFTINT_POS     (1 << (INT_UART1 - 32))

#define TIO_TX_BUFFER_LEN   64
static volatile struct {
    unsigned int head;
    unsigned int tail;
    char buffer[TIO_TX_BUFFER_LEN];
} g_tio_tx_buffer;

/* Server Message Commands */
#define CMD_NOTIFIER_RX 0
#define CMD_USER_TX     1
#define CMD_USER_RX     2
#define CMD_QUIT        3

static int g_tio_quit;

static void tioNotifier(void) {
    int serverTid = MyParentTid();

    /* set baud: 2400 bps */
    *((volatile unsigned int *) (UART1_BASE + UART_LCRM_OFFSET)) = 0x0;
    *((volatile unsigned int *) (UART1_BASE + UART_LCRL_OFFSET)) = 0xbf;
    /* disable the fifo */
    volatile unsigned int *high =
        (volatile unsigned int *) (UART1_BASE + UART_LCRH_OFFSET);
    *high &= ~FEN_MASK;

    /* initialize the tx buffer */
    g_tio_tx_buffer.head = g_tio_tx_buffer.tail = 0;
    g_tio_tx_buffer.buffer[0] = 0x60;
    g_tio_tx_buffer.buffer[1] = 0x61;
    g_tio_tx_buffer.buffer[2] = 0x60;
    g_tio_tx_buffer.buffer[3] = 0xc0;
    g_tio_tx_buffer.tail = 4;

    /* UART registers */
    volatile unsigned short *data =
        (volatile unsigned short *) (UART1_BASE + UART_DATA_OFFSET);
    volatile unsigned short *flags =
        (volatile unsigned short *) (UART1_BASE + UART_FLAG_OFFSET);

    while(!(*flags & RXFE_MASK)){
        (void) *data;
    }

    bool txfe = false, cts = true, clear_cts = true;

    while (1) {
        if (g_tio_quit) {
            break;
        }

        /* transmit data if possible */
        if (txfe && clear_cts && cts && !(*flags & TXFF_MASK) &&
                !(*flags & TXBUSY_MASK) && (*flags & CTS_MASK) &&
                g_tio_tx_buffer.head != g_tio_tx_buffer.tail) {
            *data = g_tio_tx_buffer.buffer[g_tio_tx_buffer.head];
            g_tio_tx_buffer.head =
                (g_tio_tx_buffer.head + 1) % TIO_TX_BUFFER_LEN;
            txfe = false;
            cts = false;
            clear_cts = false;
        }

        /* Always enable receive and modem status interrupts.
         * Don't need receive timeout because we aren't using FIFO.
         */
        unsigned short int_flags = UARTEN_MASK | RIEN_MASK | MSIEN_MASK;

        if (!txfe || g_tio_tx_buffer.head != g_tio_tx_buffer.tail) {
            /* Only enable transmit interrupt when we have something to send or
             * we're waiting to know when the transmit buffer is empty.
             *
             * Use the same software interrupt trick on transmit as mio.
             */
            int_flags |= TIEN_MASK;
        }

        *((volatile unsigned short *) (UART1_BASE + UART_CTLR_OFFSET)) =
            int_flags;

        AwaitEvent(INT_UART1);

        unsigned short intr =
            *((volatile unsigned short *) (UART1_BASE + UART_INTR_OFFSET));

        if (intr & RIS_MASK) {
            /* receive interrupt */
            struct String str;
            sinit(&str);

            char c = *data;
            ssettag(&str, CMD_NOTIFIER_RX);
            sputc(&str, c);

            Send(
                serverTid,
                (char *) &str, sizeof (struct String),
                (char *) 0, 0
            );
        }

        if (intr & TIS_MASK) {
            /* transmit interrupt */
            txfe = true;
        }

        if (intr & MIS_MASK) {
            /* modem status interrupt */
            if (*flags & CTS_MASK) {
                cts = true;
            } else {
                clear_cts = true;
            }
            *((volatile unsigned short *) (UART1_BASE + UART_INTR_OFFSET)) = 1;
        }

        /* Software interrupt. Just clear it so we can re-evaluate the
         * transmit buffer state. */
        *((volatile unsigned int *)
            (SOFTINT_BASE + VIC_SOFTWARE_INT_CLEAR)) = SOFTINT_POS;
    }

    Exit();
}

static void tioServer(void) {
    Create(TIO_NOTIFIER_PRIORITY, tioNotifier);

    int rx_tid = -1;
    struct String rx_buffer;
    sinit(&rx_buffer);

    while (true) {
        int tid;
        struct String req;
        sinit(&req);

        if (g_tio_quit) {
            break;
        }

        Receive(&tid, (char *) &req, sizeof(struct String));

        switch (stag(&req)) {
            default:
                /* cannot assert(0) for fear of heisenbug */
                break;

            case CMD_NOTIFIER_RX:
                sconcat(&rx_buffer, &req);
                Reply(tid, (char *) 0, 0);

                break;

            case CMD_USER_TX: {
                char *buf = sbuffer(&req);

                for (unsigned int i = 0; i < slen(&req); ++i) {
                    g_tio_tx_buffer.buffer[g_tio_tx_buffer.tail] = buf[i];
                    g_tio_tx_buffer.tail =
                        (g_tio_tx_buffer.tail + 1) % TIO_TX_BUFFER_LEN;

                    /* make sure we never wrap around */
                    assert(g_tio_tx_buffer.tail != g_tio_tx_buffer.head);
                }

                Reply(tid, (char *) 0, 0);

                /* raise a software interrupt to inform the notifier */
                *((volatile unsigned int *)
                    (SOFTINT_BASE + VIC_SOFTWARE_INT)) = SOFTINT_POS;

                break;
            }

            case CMD_USER_RX:
                assert(rx_tid == -1);
                rx_tid = tid;
                break;

            case CMD_QUIT:
                g_tio_quit = true;
                Reply(tid, (char *) 0, 0);

                /* raise a software interrupt to quit the notifier */
                *((volatile unsigned int *)
                    (SOFTINT_BASE + VIC_SOFTWARE_INT)) = SOFTINT_POS;

                break;
        }

        if (slen(&rx_buffer) > 0 && rx_tid >= 0) {
            Reply(rx_tid, (char *) &rx_buffer, sizeof(struct String));

            sinit(&rx_buffer);
            rx_tid = -1;
        }
    }

    Exit();
}

static int g_tio_server_tid;
void tioInit(void) {
    g_tio_quit = false;
    g_tio_server_tid = Create(TIO_SERVER_PRIORITY, tioServer);
    assert(g_tio_server_tid >= 0);
}

void tioPrint(struct String *s) {
    ssettag(s, CMD_USER_TX);
    Send(
        g_tio_server_tid,
        (char *) s, sizeof(struct String),
        (char *) 0, 0
    );
}

void tioRead(struct String *s) {
    struct String cmd;
    sinit(&cmd);
    ssettag(&cmd, CMD_USER_RX);

    Send(
        g_tio_server_tid,
        (char *) &cmd, sizeof(struct String),
        (char *) s, sizeof(struct String)
    );
}

void tioQuit(void) {
    struct String cmd;
    sinit(&cmd);
    ssettag(&cmd, CMD_QUIT);

    Send(
        g_tio_server_tid,
        (char *) &cmd, sizeof(struct String),
        (char *) 0, 0
    );
}
