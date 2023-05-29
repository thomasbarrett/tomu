#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/eventfd.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>

#include <queue.h>
#include <serial.h>

#define SERIAL_IO_ADDR 0x3f8

// backport UAPI constants defined in 6.3
// https://github.com/torvalds/linux/commit/3398cc4f2b1592148a2ebabc5a2df3e303d4e77c
#define UART_IIR_FIFO_ENABLED           0xc0 /* FIFOs enabled / port type identification */
#define UART_IIR_FIFO_ENABLED_8250      0x00 /* 8250: no FIFO */
#define UART_IIR_FIFO_ENABLED_16550     0x80 /* 16550: (broken/unusable) FIFO */
#define UART_IIR_FIFO_ENABLED_16550A	0xc0 /* 16550A: FIFO enabled */

serial_t serial_16550a = {
    .regs = {
        .iir = UART_IIR_NO_INT,
        .mcr = UART_MCR_OUT2,
        .lsr = UART_LSR_TEMT | UART_LSR_THRE,
        .msr = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS,
    }
};

void serial_update_irq(serial_t *dev) {
	uint8_t iir = 0;

	// Send an interrupt if rx queue is non-empty and rx interrupt enabled.
	if ((dev->regs.ier & UART_IER_RDI) && (dev->regs.lsr & UART_LSR_DR))
		iir |= UART_IIR_RDI;

	// Send an interrupt if tx queue is empty and tx interrupt enabled.
	if ((dev->regs.ier & UART_IER_THRI) && (dev->regs.lsr & UART_LSR_TEMT))
		iir |= UART_IIR_THRI;

	// Update the iir register and toggle irq state.
    if (!iir) {
		dev->regs.iir = UART_IIR_NO_INT;
		if (dev->irq_state)
            dev->irq_line(4, 0, dev->irq_arg);
	} else {
		dev->regs.iir = iir;
		if (!dev->irq_state)
            dev->irq_line(4, 1, dev->irq_arg);
	}
	dev->irq_state = iir;
}

int serial_write(serial_t *dev, uint8_t *buf, size_t nbytes) {
    pthread_mutex_lock(&dev->mu);

    // Do not accept rx while in loopback mode.
    if (dev->regs.mcr & UART_MCR_LOOP) {
        errno = EAGAIN;
        goto error;
    }

    // Do not accept rx while existing rx in-flight.
    if ((dev->regs.lsr & UART_LSR_DR) || queue_size(&dev->rx_queue) > 0) {
        errno = EAGAIN;
        goto error;
    }

    size_t nwritten = 0;
    while (nwritten < nbytes) {
        if (queue_push(&dev->rx_queue, buf[nwritten]) < 0) {
            break;
        }
        dev->regs.lsr |= UART_LSR_DR;
        nwritten++;
    }

    if (nwritten == 0) {
        errno = EAGAIN;
        goto error;
    }

    serial_update_irq(dev);
    pthread_mutex_unlock(&dev->mu);
    return nwritten;

error:
    pthread_mutex_unlock(&dev->mu);
    return -1;
}

int serial_read(serial_t *dev, uint8_t *buf, size_t nbytes) {
    pthread_mutex_lock(&dev->mu);

    size_t nread = 0;
    while(nread < nbytes) {
        if (queue_pop(&dev->tx_queue, &buf[nread]) == 0) nread++;
        else break;
    }
    if (queue_size(&dev->tx_queue) == 0) {
        if (nread == 0) {
            errno = EAGAIN;
            pthread_mutex_unlock(&dev->mu);
            return -1;
        } else {
            dev->regs.lsr |= UART_LSR_TEMT | UART_LSR_THRE;
        }
    }

    serial_update_irq(dev);
    pthread_mutex_unlock(&dev->mu);
    return nread;
}

void serial_out(serial_t *dev, uint16_t port, uint8_t *data, size_t len) {
    pthread_mutex_lock(&dev->mu);
    switch (port) {
    case SERIAL_IO_ADDR + UART_TX:
        if (dev->regs.lcr & UART_LCR_DLAB) {
            dev->regs.dll = *data;
            break;
        }

        // While in loopback mode, write the tx to rx queue.
        if (dev->regs.mcr & UART_MCR_LOOP) {
            if (queue_push(&dev->rx_queue, *data) == 0) {
                dev->regs.lsr |= UART_LSR_DR;
            }
            break;
        }
        
        if (queue_push(&dev->tx_queue, *data) == 0) {
            dev->regs.lsr &= ~UART_LSR_TEMT;
            if (write(dev->eventfd, (void*) &(uint64_t){1}, sizeof(uint64_t)) < 0) {
                // The number of unhandled events has exceeded 0xffffffffffffffff.
                // We never expect this to happen under ordinary conditions, but it
                // may indicate that the event loop is in deadlock.
                // TODO: Add warning log if the eventfd has not been read.
            }
        }

        if (queue_size(&dev->tx_queue) >= SERIAL_FIFO_LEN / 2)
            dev->regs.lsr &= ~UART_LSR_THRE;

        break;
    case SERIAL_IO_ADDR + UART_IER:
        if (dev->regs.lcr & UART_LCR_DLAB)
            dev->regs.dlm = *data;
        else
            dev->regs.ier = *data & 0x0f;
        break;
    case SERIAL_IO_ADDR + UART_FCR:
        dev->regs.fcr = *data;
        break;
    case SERIAL_IO_ADDR + UART_LCR:
        dev->regs.lcr = *data;
        
        // clear rx queue
        if (dev->regs.lcr & UART_FCR_CLEAR_RCVR) {
            dev->regs.lcr &= ~UART_FCR_CLEAR_RCVR;
            queue_clear(&dev->rx_queue);
            dev->regs.lsr &= ~UART_LSR_DR;
        }
        
        // clear tx queue
        if (dev->regs.lcr & UART_FCR_CLEAR_XMIT) {
            dev->regs.lcr &= ~UART_FCR_CLEAR_XMIT;
            queue_clear(&dev->tx_queue);
            dev->regs.lsr |= UART_LSR_TEMT | UART_LSR_THRE;
        }

        break;
    case SERIAL_IO_ADDR + UART_MCR:
        dev->regs.mcr = *data;
        break;
    case SERIAL_IO_ADDR + UART_LSR:
        // lsr is read-only.
        break;
    case SERIAL_IO_ADDR + UART_MSR:
        // msr is read-only.
        break;
    case SERIAL_IO_ADDR + UART_SCR:
        dev->regs.scr = *data;
        break;
    default:
        break;
    }
    serial_update_irq(dev);
    pthread_mutex_unlock(&dev->mu);
}

void serial_in(serial_t *dev, uint16_t port, uint8_t *data, size_t len) {
    pthread_mutex_lock(&dev->mu);
    switch (port) {
        case SERIAL_IO_ADDR + UART_RX:
        if (dev->regs.lcr & UART_LCR_DLAB) {
            *data = dev->regs.dll;
            break;
        }

        if (queue_size(&dev->rx_queue) == 0)
            break;

        /* Break issued ? */
        if (dev->regs.lsr & UART_LSR_BI) {
            dev->regs.lsr &= ~UART_LSR_BI;
            *data = 0x00;
            break;
        }
        
        queue_pop(&dev->rx_queue, data);
        if (queue_size(&dev->rx_queue) == 0) {
            dev->regs.lsr &= ~UART_LSR_DR;
        }                      

        break;
    case SERIAL_IO_ADDR + UART_IER:
        if (dev->regs.lcr & UART_LCR_DLAB)
            *data = dev->regs.dlm;
        else
            *data = dev->regs.ier;
        break;
    case SERIAL_IO_ADDR + UART_IIR:
        *data = dev->regs.iir | UART_IIR_FIFO_ENABLED_16550A;
        break;
    case SERIAL_IO_ADDR + UART_LCR:
        *data = dev->regs.lcr;
        break;
    case SERIAL_IO_ADDR + UART_MCR:
        *data = dev->regs.mcr;
        break;
    case SERIAL_IO_ADDR + UART_LSR:
        *data = dev->regs.lsr;
        break;
    case SERIAL_IO_ADDR + UART_MSR:
        *data = dev->regs.msr;
        break;
    case SERIAL_IO_ADDR + UART_SCR:
        *data = dev->regs.scr;
        break;
    default:
        break;
    }
    serial_update_irq(dev);
    pthread_mutex_unlock(&dev->mu);
}

int serial_init(serial_t *dev, irq_line_func irq_line, irq_arg_t irq_arg) {
    if (queue_init(&dev->rx_queue, SERIAL_FIFO_LEN) < 0) return -1;
    if (queue_init(&dev->tx_queue, SERIAL_FIFO_LEN) < 0) return -1;
    dev->irq_arg = irq_arg;
    dev->irq_line = irq_line;
    dev->eventfd = -1;
    return 0;
}

void serial_deinit(serial_t *dev) {
    queue_deinit(&dev->rx_queue);
    queue_deinit(&dev->tx_queue);
}

int serial_open(serial_t *dev) {
    pthread_mutex_lock(&dev->mu);
    if (dev->eventfd != -1) {
        errno = EBUSY;
        pthread_mutex_unlock(&dev->mu);
        return -1;
    }

    int res = eventfd(0, EFD_NONBLOCK);
    if (res >= 0) {
        dev->eventfd = res;
    }

    return res;
    pthread_mutex_unlock(&dev->mu);
}

int serial_close(serial_t *dev, int fd) {
    pthread_mutex_lock(&dev->mu);
    if (dev->eventfd != fd) {
        errno = EINVAL;
        pthread_mutex_unlock(&dev->mu);
        return -1;
    }

    int res = close(fd);
    dev->eventfd = -1;
    return res;
    pthread_mutex_unlock(&dev->mu);
}
