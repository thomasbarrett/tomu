#include <pci.h>

#include <stdio.h>

phb_t phb = {
    .regs = {
        .config_address = 0x0000
    }
};

void phb_in(uint16_t port, uint8_t *buf, size_t count) {\
    switch (port) {
        case PCI_CONFIG_DATA:
            if (phb.regs.config_address | PCI_CONFIG_ADDRESS_ENABLE) {
                uint8_t offset = phb.regs.config_address & 0xFC;
                uint8_t function = (phb.regs.config_address >> 8) & 0x07;
                uint8_t slot = (phb.regs.config_address >> 11) & 0x1F;
                uint8_t bus = phb.regs.config_address >> 16;
                // printf("read [%02x] %02x:%02x.%01x\r\n", offset, bus, slot, function);
                // fflush(stdout);
            }
            break;
        case PCI_CONFIG_ADDRESS:
            // The host bridge ignores non-DWORD IO.
            if (count != sizeof(uint32_t)) return;
            *((uint32_t*) buf) = phb.regs.config_address;
            break;
    }
}

void phb_out(uint16_t port, uint8_t *buf, size_t count) {
    switch (port) {
        case PCI_CONFIG_DATA:
            if (phb.regs.config_address | PCI_CONFIG_ADDRESS_ENABLE) {
                uint8_t offset = phb.regs.config_address & 0xFC;
                uint8_t function = (phb.regs.config_address >> 8) & 0x07;
                uint8_t slot = (phb.regs.config_address >> 11) & 0x1F;
                uint8_t bus = phb.regs.config_address >> 16;
                // printf("write [%02x] %02x:%02x.%01x\r\n", offset, bus, slot, function);
                // fflush(stdout);
            }
            break;
        case PCI_CONFIG_ADDRESS:
            // The host bridge ignores non-DWORD IO.
            if (count != sizeof(uint32_t)) return;
            phb.regs.config_address = *((uint32_t*) buf);
            break;
    }
}
