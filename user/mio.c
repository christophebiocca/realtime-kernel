#include <debug.h>
#include <ts7200.h>

#include <user/mio.h>
#include <user/priorities.h>
#include <user/string.h>
#include <user/syscall.h>

/* For AwaitEvent() */
#define INT_UART2       54

/* Where is INT_UART2 actually defined */
#define SOFTINT_BASE    VIC2_BASE
/* Position in the software interrupt register */
#define SOFTINT_POS     (1 << (INT_UART2 - 32))

#define MIO_TX_BUFFER_LEN   512
static volatile struct {
    unsigned int head;
    unsigned int tail;
    char buffer[MIO_TX_BUFFER_LEN];
} g_mio_tx_buffer;

/* Server Message Commands */
#define CMD_NOTIFIER_RX 0
#define CMD_USER_TX     1
#define CMD_USER_RX     2

static void mioNotifier(void) {
    int serverTid = MyParentTid();

    /* set baud: 115.2 kbps */
    *((volatile unsigned int *) (UART2_BASE + UART_LCRM_OFFSET)) = 0x0;
    *((volatile unsigned int *) (UART2_BASE + UART_LCRL_OFFSET)) = 0x3;
    /* enable the fifo */
    volatile unsigned int *high =
        (volatile unsigned int *) (UART2_BASE + UART_LCRH_OFFSET);
    *high |= FEN_MASK;

    /* initialize the tx buffer */
    g_mio_tx_buffer.head = g_mio_tx_buffer.tail = 0;

    while (1) {
        /* always enable receive interrupts. FIXME: what about modem status? */
        unsigned short int_flags = UARTEN_MASK | RIEN_MASK;
        /* DANGER WILL ROBINSON: THIS CAN BREAK HORRIBLY!
         *
         * To prevent useless interrupts when we have nothing to send, we only
         * enable the transmit interrupt when necessary. However, if transmit
         * interrupts are disabled and we queue on some output, the output will
         * not be flushed until the next UART interrupt (possibly the user
         * typing something in).
         *
         * Since buffering output till the user types something is clearly
         * broken, we get around this by having the server raise the UART
         * interrupt in software (after loading data into g_mio_tx_buffer). This
         * will eventually bring us to the top of our loop, where we'll
         * re-evaluate our state and enable the transmit interrupt.
         */
        if (g_mio_tx_buffer.head != g_mio_tx_buffer.tail) {
            /* only enable transmit interrupt if we have stuff to send */
            int_flags |= TIEN_MASK;
        }
        *((volatile unsigned short *) (UART2_BASE + UART_CTLR_OFFSET)) =
            int_flags;

        /* Wait for something to happen */
        AwaitEvent(INT_UART2);

        unsigned short intr =
            *((volatile unsigned short *) (UART2_BASE + UART_INTR_OFFSET));
        volatile unsigned short *data =
            (volatile unsigned short *) (UART2_BASE + UART_DATA_OFFSET);
        volatile unsigned short *flags =
            (volatile unsigned short *) (UART2_BASE + UART_FLAG_OFFSET);

        if (intr & RIS_MASK) {
            /* receive interrupt */
            struct String str;
            sinit(&str);
            str.tag = CMD_NOTIFIER_RX;

            while (!(*flags & RXFE_MASK)) {
                sputc(&str, *data);
            }

            Send(
                serverTid,
                (char *) &str, sizeof (struct String),
                (char *) 0, 0
            );
        }

        /* FIXME: we don't care about CTS for monitor? */
        if (intr & TIS_MASK) {
            /* transmit interrupt */
            while (g_mio_tx_buffer.head != g_mio_tx_buffer.tail &&
                    !(*flags & TXFF_MASK)) {
                *data = g_mio_tx_buffer.buffer[g_mio_tx_buffer.head];
                g_mio_tx_buffer.head =
                    (g_mio_tx_buffer.head + 1) % MIO_TX_BUFFER_LEN;
            }
        }

        if (*((volatile unsigned int *)
                SOFTINT_BASE + VIC_SOFTWARE_INT) & SOFTINT_POS) {
            /* Software interrupt. Just clear it so we can re-evaluate the
             * transmit buffer state. */
            *((volatile unsigned int *)
                (SOFTINT_BASE + VIC_SOFTWARE_INT_CLEAR)) = SOFTINT_POS;
        }
    }

    Exit();
}

static void mioServer(void) {
    Create(MIO_NOTIFIER_PRIORITY, mioNotifier);

    int rx_tid = -1;
    struct String rx_buffer;
    sinit(&rx_buffer);

    while (1) {
        int tid;
        struct String str;
        sinit(&str);

        Receive(&tid, (char *) &str, sizeof(struct String));

        switch (str.tag) {
            case CMD_NOTIFIER_RX:
                sconcat(&rx_buffer, &str);
                break;

            case CMD_USER_TX: {
                char *buf = sbuffer(&str);

                for (unsigned int i = 0; i < slen(&str); ++i) {
                    g_mio_tx_buffer.buffer[g_mio_tx_buffer.tail] = buf[i];
                    g_mio_tx_buffer.tail =
                        (g_mio_tx_buffer.tail + 1) % MIO_TX_BUFFER_LEN;

                    /* make sure we never wrap around */
                    assert(g_mio_tx_buffer.tail != g_mio_tx_buffer.head);
                }

                /* raise a software interrupt to inform the notifier */
                *((volatile unsigned int *)
                    (SOFTINT_BASE + VIC_SOFTWARE_INT)) = SOFTINT_POS;

                break;
            }

            case CMD_USER_RX:
                /* make sure no one else is already waiting for input */
                assert(rx_tid == -1);
                rx_tid = tid;

                break;

            default:
                assert(0);
        }

        if (slen(&rx_buffer) != 0 && rx_tid >= 0) {
            Reply(rx_tid, (char *) &rx_buffer, sizeof(struct String));

            sinit(&rx_buffer);
            rx_tid = -1;
        }
    }

    Exit();
}

static int g_mio_server_tid;
void mioInit(void) {
    g_mio_server_tid = Create(MIO_SERVER_PRIORITY, mioServer);
    assert(g_mio_server_tid >= 0);
}

void mioPrint(struct String *s) {
    ssettag(s, CMD_USER_TX);
    Send(
        g_mio_server_tid,
        (char *) s, sizeof(struct String),
        (char *) 0, 0
    );
}

void mioRead(struct String *s) {
    struct String cmd;
    sinit(&cmd);
    ssettag(&cmd, CMD_USER_RX);

    Send(
        g_mio_server_tid,
        (char *) &cmd, sizeof(struct String),
        (char *) s, sizeof(struct String)
    );
}
