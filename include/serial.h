#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <queue.h>
#include <guest.h>

#include <pthread.h>

#define FIFO_LEN 16

typedef struct serial {
    guest_t *guest;
    pthread_mutex_t mu;
    uint8_t irq_state;

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

int serial_write(serial_t *dev, uint8_t *buf, size_t nbytes);
int serial_read(serial_t *dev, uint8_t *buf, size_t nbytes);

void serial_out(serial_t *dev, uint16_t port, uint8_t *data, size_t len);
void serial_in(serial_t *dev, uint16_t port, uint8_t *data, size_t len);

#endif /* QUEUE_H */
