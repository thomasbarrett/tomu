#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <pthread.h>

#include <queue.h>
#include <irq.h>

#define FIFO_LEN 16

/**
 * The serial_t device emulates a 16550A UART device. This is a commonly used
 * serial communication device on amd64 machines. Unlike the earlier 8250 UART
 * device, the 16550A stores pending rx and tx bytes in a 16-byte FIFO buffer
 * to minimize data loss due to delayed interrupt handling.
 * 
 * Linux can be configured to write kernel logs to serial console by appending
 * the `console=ttyS0` to the kernel kernel cmdline args.
 * 
 * serial_t devices appear as a ttyS* device in devtmpfs. On amd64, up to 4
 * serial devices are supported. If they exist, they will be named:
 * - /dev/ttyS0
 * - /dev/ttyS1
 * - /dev/ttyS2
 * - /dev/ttyS3
 */
typedef struct serial {
    pthread_mutex_t mu;

    // IRQ abstraction.
    irq_line_func irq_line;
    irq_arg_t irq_arg;
    int irq_state;

    // Writes to THR UART register push to FIFO queue.
    queue_t tx_queue;

    // Reads from RBR UART register pop from FIFO queue.
    queue_t rx_queue;

    // UART registers.
    struct {
        uint8_t dll;
        uint8_t dlm;
        uint8_t iir;
        uint8_t ier;
        uint8_t	fcr;
        uint8_t lcr;
        uint8_t	mcr;
        uint8_t	lsr;
        uint8_t	msr;
        uint8_t scr;
    } regs;
} serial_t;

extern serial_t serial_16550a;

/**
 * serial_read reads up to `count` bytes from the tx queue of the serial
 * device `dev` into `buf`. The number of bytes read may be less than `count`
 * if there is insuffient data on the tx queue.
 * 
 * \param dev the serial device.
 * \param buf the buffer.
 * \param count the buffer size in bytes.
 * \return On success, the number of bytes read is returned. On error, -1
 *         is returned and errno is set to indicate the error.
 *
 * \exception EGAIN - The read would block.
 */
int serial_read(serial_t *dev, uint8_t *buf, size_t count);

/**
 * serial_write writes up to `count` bytes from `buf` to the rx queue of the 
 * serial device `dev`. The number of bytes written may be less than `count`
 * if there is insufficient space in the rx queue.
 * 
 * \param dev the serial device.
 * \param buf the buffer.
 * \param count the buffer size in bytes.
 * \return On success, the number of bytes written is returned. On error, -1
 *         is returned and errno is set to indicate the error.
 * 
 * \exception EGAIN - The write would block.
 */
int serial_write(serial_t *dev, uint8_t *buf, size_t count);

/**
 * serial_write reads `count` bytes from the IO `port` to `buf`.
 */
void serial_in(serial_t *dev, uint16_t port, uint8_t *buf, size_t count);

/**
 * serial_write writes `count` bytes from `buf` to the IO `port`.
 */
void serial_out(serial_t *dev, uint16_t port, uint8_t *buf, size_t count);

#endif /* SERIAL_H */
