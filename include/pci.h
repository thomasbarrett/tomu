#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stddef.h>

#define PCI_CONFIG_ADDRESS	0x0CF8
#define PCI_CONFIG_DATA		0x0CFC

#define PCI_CONFIG_ADDRESS_ENABLE 0x80000000UL

typedef struct phb {
    struct {
        uint32_t config_address;
    } regs;
} phb_t;

/**
 * phb_in reads `count` bytes from the IO `port` to `buf`.
 */
void phb_in(uint16_t port, uint8_t *buf, size_t count);

/**
 * phb_out writes `count` bytes from `buf` to the IO `port`.
 */
void phb_out(uint16_t port, uint8_t *buf, size_t count);

#endif
