#include <debug.h>
#include <ts7200.h>

#include <user/mio.h>
#include <user/string.h>

#define INT_UART2   54

#define MIO_TX_BUFFER_LEN   512
static struct {
    unsigned int head;
    unsigned int tail;
    char buffer[MIO_TX_BUFFER_LEN];
} g_mio_tx_buffer;

static void mioNotifier(void) {
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
        unsigned int flags = UARTEN_MASK | RIEN_MASK;
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
         *
         * There does exist a race condition: what if the notifier checks
         * g_mio_tx_buffer and detects no data to transmit. However, before it
         * can AwaitEvent(), the server queues on some data and raises the
         * interrupt in software. Since nobody is awaiting that interrupt, it
         * gets dropped silently, and by the time the notifier AwaitEvent()'s,
         * we've missed the boat.
         *
         * We solve this by exploiting our knowledge of the scheduler. Since the
         * kernel never preempts a task and there is no system call in between
         * the check for data to transmit and the AwaitEvent(), there is no
         * possible way the server can be scheduled. Furthermore, the notifier
         * runs at a higher priority than the server, and thus if both of them
         * are capable of running, the notifier will always be picked first.
         */
        if (g_mio_tx_buffer.head != g_mio_tx_buffer.tail) {
            /* only enable transmit interrupt if we have stuff to send */
            flags |= TIEN_MASK;
        }
        *((volatile unsigned short *) (UART2_BASE + UART_CTLR_OFFSET)) = flags;

        /* Wait for something to happen */
        AwaitEvent(INT_UART2);

        unsigned short status =
            *((volatile unsigned short *) (UART2_BASE + UART_INTR_OFFSET));

        if (status & RIS_MASK) {
            /* receive interrupt */
        }

        if (status & TIS_MASK) {
            /* transmit interrupt */
        }

        if (!((status & RIS_MASK) | (status & TIS_MASK))) {
            /* software interrupt */
        }
    }

    Exit();
}

static void mioServer(void) {
    Create(MIO_NOTIFIER_PRIORITY, mioNotifier);
}

static int g_mio_server_tid;
void mioInit(void) {
    g_mio_server_tid = Create(MIO_SERVER_PRIORITY, mioServer);
    assert(g_mio_server_tid >= 0);
}

void mio_print(struct String *s) {
}
