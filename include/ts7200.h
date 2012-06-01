/*
 * ts7200.h - definitions describing the ts7200 peripheral registers
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

// T1 and T2 are 16 bits wide, T3 is 32 bits wide.
#define	TIMER1_BASE	0x80810000
#define	TIMER2_BASE	0x80810020
#define	TIMER3_BASE	0x80810080

// The initial value of the timer, set to 0 on reset.
// Do not write after enabling timer.
#define	LDR_OFFSET	0x00000000	// 16/32 bits, RW
// Read only, current timer value.
#define	VAL_OFFSET	0x00000004	// 16/32 bits, RO
// All zero on reset.
#define CRTL_OFFSET	0x00000008	// 3 bits, RW
        // Enable the timer, starts counting down, must set load register everytime.
	#define	ENABLE_MASK	0x00000080
        // 1 for periodic, 0 for free running.
	#define	MODE_MASK	0x00000040
        // 1 for 508 kHz Clock, 2 for 2kHz Clock
	#define	CLKSEL_MASK	0x00000008
// Write only, clears interrupt.
#define CLR_OFFSET	0x0000000c	// no data, WO


#define LED_ADDRESS	0x80840020
	#define LED_NONE	0x0
	#define LED_GREEN	0x1
	#define LED_RED		0x2
	#define LED_BOTH	0x3

#define COM1	0
#define COM2	1

#define IRDA_BASE	0x808b0000
#define UART1_BASE	0x808c0000
#define UART2_BASE	0x808d0000

// All the below registers for UART1
// First nine registers (up to Ox28) for UART 2

/*
 * Documented name: UART1Data (Cirrus page 539)
 * Read: gets the top most byte from the fifo. Status bits are on the UART1RXSts
 * Write: enqueues a byte onto the fifo.
 * Disabled fifos just mean that the effective size is one byte.
 */
#define UART_DATA_OFFSET	0x0	// low 8 bits
	#define DATA_MASK	0x000000ff
/*
 * Documented name: UART1RXSts (Cirrus page 540)
 * Read: Status bits are on the 4 lowest bit.
 * Write: clears the status bits.
 * In fifo mode, the errors are associated with the top word in the fifo.
 */
#define UART_RSR_OFFSET		0x4	// low 4 bits
	#define FE_MASK		1<<0	// Framing Error: no stop bit
	#define PE_MASK		1<<1	// Parity Error
	#define BE_MASK		1<<2	// Break Error
	#define OE_MASK		1<<3	// Overrun Error: Too much data, not handled fast enough
/*
 * Documented name: UART1LinCtrlHigh (Cirrus page 541)
 * Read: Reads status flags.
 * Write: Sets flags and flushes current contents of LCRM and LCRL at once.
 */
#define UART_LCRH_OFFSET	0x8	// low 7 bits
	#define BRK_MASK	1<<0	// Set to send a break after current character
	#define PEN_MASK	1<<1	// parity enable flag
	#define EPS_MASK	1<<2	// on: even parity, off: odd parity
	#define STP2_MASK	1<<3	// on: 2 stop bits, off: 1 stop bit
	#define FEN_MASK	1<<4	// Sets whether fifos are in use or not, (see note on UART1Data)
	#define WLEN_MASK	0x3<<5	// 2 bits for word length (5 + bits)
/*
 * Documented name: UART1LinCtrlMid (Cirrus page 542)
 * Read: Reads baud rate divisor MSB.
 * Write: Sets baud rate divisor MSB. Needs flush to actually do anything (see above).
 * The effective baud rate divisor is BAUDDIV = (F_UARTCLK/(16 * Baud rate)) - 1,
 * alternatively the baud rate is F_UARTCLK/(16 * (BAUDDIV + 1) )
 * Where F_UARTCLK is the UART Reference clock frequency.
 */
#define UART_LCRM_OFFSET	0xc		// low 8 bits
	#define BRDH_MASK	0x000000ff	// MSB of baud rate divisor
/*
 * Documented name: UART1LinCtrlLow (Cirrus page 543)
 * Read: Reads baud rate divisor LSB.
 * Write: Sets baud rate divisor LSB. Needs flush to actually do anything (see above).
 */
#define UART_LCRL_OFFSET	0x10		// low 8 bits
	#define BRDL_MASK	0x000000ff	// LSB of baud rate divisor
/*
 * Documented name: UART1Ctrl (Cirrus page 544)
 * Read: Reads control bits.
 * Write: Sets control bits.
 * (TODO) Some of these look very important, test and check.
 */
#define UART_CTLR_OFFSET	0x14	// low 8 bits
	#define UARTEN_MASK	1<<0	// UART enable bits (important?)
	#define MSIEN_MASK	1<<3	// modem status interrupt
	#define RIEN_MASK	1<<4	// receive interrupt
	#define TIEN_MASK	1<<5	// transmit interrupt
	#define RTIEN_MASK	1<<6	// receive timeout interrupt
	#define LBEN_MASK	1<<7	// loopback: send sent stuff back to read.
/*
 * Documented name: UART1Flag (Cirrus page 544)
 * ReadOnly: Reads flags
 */
#define UART_FLAG_OFFSET	0x18	// low 8 bits
	#define CTS_MASK	1<<0	// High if cleared to send
	#define DSR_MASK	1<<1	// Data set ready status
	#define DCD_MASK	1<<2	// Data carrier Detect status
	#define TXBUSY_MASK	1<<3	// Busy, not everything has been sent.
	#define RXFE_MASK	1<<4	// Receive buffer empty
	#define TXFF_MASK	1<<5	// Transmit buffer full
	#define RXFF_MASK	1<<6	// Receive buffer full
	#define TXFE_MASK	1<<7	// Transmit buffer empty
/*
 * Documented name: UART1IntIDIntClr (Cirrus page 546)
 * Read: Reads interrupt id bits
 * Write: (any value) Clears the Modem status interrupt.
 */
#define UART_INTR_OFFSET	0x1c
	#define MIS_MASK	1<<0	// Modem interrupt
	#define RIS_MASK	1<<1	// Receive interrupt (Receive buffer not empty)
	#define TIS_MASK	1<<2	// Transmit interrupt (Send buffer not empty)
	#define RTIS_MASK	1<<3	// Receive timeout (TODO:Needs clarification)

/* TODO: Document this */
#define UART_DMAR_OFFSET	0x28

// Specific to UART1

/* TODO: Figure out if any of these are actually relevant */
#define UART_MDMCTL_OFFSET	0x100
#define UART_MDMSTS_OFFSET	0x104
#define UART_HDLCCTL_OFFSET	0x20c
#define UART_HDLCAMV_OFFSET	0x210
#define UART_HDLCAM_OFFSET	0x214
#define UART_HDLCRIB_OFFSET	0x218
#define UART_HDLCSTS_OFFSET	0x21c


// Interrupt related //

#define VIC1_BASE 0x800B0000
#define VIC2_BASE 0x800C0000

// Status of interrupts after masking with VIC_INT_ENABLE and VIC_INT_SELECT. Read-only
#define VIC_IRQ_STATUS_OFFSET   0x000

// Status of interrupts after masking with VIC_INT_ENABLE and VIC_INT_SELECT. Read-only
#define VIC_FIQ_STATUS_OFFSET   0x004

// Raw interrupts asserted before masking. Read-only.
#define VIC_RAW_STATUS  `       0x008

// Set 0 on a bit for IRQ, 1 for FIQ. Read-write.
#define VIC_INT_SELECT          0x00C

// Set 1 on a bit to enable the related interrupt.
#define VIC_INT_ENABLE          0x010

// Writing 1 here clears the corresponding bit in VIC_INT_ENABLE
#define VIC_INT_ENABLE_CLEAR    0x014

// Writing 1 here triggers the line for the corresponding interrupt.
#define VIC_SOFTWARE_INT        0x018

// Writing 1 here clears the line for the above interrupts.
#define VIC_SOFTWARE_INT_CLEAR  0x01C

// Blocks access to these registers in user mode.
#define VIC_PROTECTION          0x020

enum {
    INT_TC1UI = 1 << 4,
    INT_TC2UI = 1 << 5,
};

/* TODO: When/if we start vectored interrupts, we'll need to deal with more registers, see EP93xx docs, page 170 (6-8) */
