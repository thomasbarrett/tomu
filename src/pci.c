#include <pci.h>

#include <stdio.h>
#include <string.h>

phb_t phb = {
    .regs = {
        .config_address = 0x0000
    }
};

#define min(a, b) (a < b ? a: b)

#define NUM_CONFIG_REGISTERS_PCI    64
#define NUM_CONFIG_REGISTERS_PCIE   1024

typedef struct pci_config {
    uint32_t registers[NUM_CONFIG_REGISTERS_PCIE];
    uint32_t registers_mask[NUM_CONFIG_REGISTERS_PCIE];
} pci_config_t;

pci_config_t phb_config = {0};

#define VENDOR_ID_INTEL             0x8086
#define DEVICE_ID_INTEL_VIRT_PHB    0x0d57

#define DEVICE_CLASS_UNCLASSIFIED 0x00
#define DEVICE_CLASS_MASS_STORAGE 0x01
#define DEVICE_CLASS_NETWORK      0x02
#define DEVICE_CLASS_DISPLAY      0x03
#define DEVICE_CLASS_MULTIMEDIA   0x04
#define DEVICE_CLASS_MEMORY       0x05
#define DEVICE_CLASS_BRIDGE       0x06

#define PCI_BRIDGE_HOST       0x00

void pbh_init(void) {
    phb_config.registers[0] = (DEVICE_ID_INTEL_VIRT_PHB << 16) | VENDOR_ID_INTEL;
    phb_config.registers[2] = (DEVICE_CLASS_BRIDGE << 24) | (PCI_BRIDGE_HOST << 16);
    phb_config.registers[3] = 0x00010000;
}

void phb_in(uint16_t port, uint8_t *buf, size_t count) {
    if (port == PCI_CONFIG_ADDRESS) {
        // The host bridge ignores non-DWORD IO.
        if (count != sizeof(uint32_t)) return;
        *((uint32_t*) buf) = phb.regs.config_address;
        return;
    }

    if (port >= PCI_CONFIG_DATA && port < PCI_CONFIG_DATA + sizeof(uint32_t)) {
        if (phb.regs.config_address | PCI_CONFIG_ADDRESS_ENABLE) {
            uint8_t reg = ((uint8_t) phb.regs.config_address) >> 2;
            uint8_t offset = port - PCI_CONFIG_DATA;
            count = min(count, sizeof(uint32_t) - offset);
            pci_address_t addr = {
                .domain = 0x0000,
                .bus = phb.regs.config_address >> 16,
                .slot = (phb.regs.config_address >> 11) & 0x1F,
                .function = (phb.regs.config_address >> 8) & 0x07,
            };
            if (reg > NUM_CONFIG_REGISTERS_PCI) {
                fprintf(stdout, "invalid register: %d\r\n", reg);
                fflush(stdout);
                return;
            }
            pci_address_t addr1 = {0};
            // fprintf(stdout, "read %04x:%02x:%02x:%01x reg: %d size: %d\r\n", addr.domain, addr.bus, addr.slot, addr.function, phb.regs.config_address & 0xFF, count);
            // fflush(stdout);
            if (memcmp(&addr, &addr1, sizeof(pci_address_t)) == 0) {
                memcpy(buf, ((uint8_t*)&phb_config.registers[reg]) + offset, count);
            } else {
                uint32_t error = 0xffffffff;
                memcpy(buf, &error, count);
            }
        }
    }
}

void phb_out(uint16_t port, uint8_t *buf, size_t count) {
    if (port == PCI_CONFIG_ADDRESS) {
         // The host bridge ignores non-DWORD IO.
        if (count != sizeof(uint32_t)) return;
        phb.regs.config_address = *((uint32_t*) buf);
        return;
    }

    if (port >= PCI_CONFIG_DATA && port < PCI_CONFIG_DATA + sizeof(uint32_t)) {
        if (phb.regs.config_address | PCI_CONFIG_ADDRESS_ENABLE) {
            uint8_t reg = ((uint8_t) phb.regs.config_address) >> 2;
            uint8_t offset = port - PCI_CONFIG_DATA;
            count = min(count, sizeof(uint32_t) - offset);
            pci_address_t addr = {
                .domain = 0x0000,
                .bus = phb.regs.config_address >> 16,
                .slot = (phb.regs.config_address >> 11) & 0x1F,
                .function = (phb.regs.config_address >> 8) & 0x07,
            };
            if (reg > NUM_CONFIG_REGISTERS_PCI) {
                fprintf(stdout, "invalid register: %d\r\n", reg);
                fflush(stdout);
                return;
            }
            pci_address_t addr1 = {0};
            // fprintf(stdout, "write %04x:%02x:%02x:%01x reg: %d size: %d\r\n", addr.domain, addr.bus, addr.slot, addr.function, phb.regs.config_address & 0xFF, count);
            // fflush(stdout);
            if (memcmp(&addr, &addr1, sizeof(pci_address_t)) == 0) {
                memcpy(((uint8_t*)&phb_config.registers[reg]) + offset, buf, count);
            }
        }
    }
}
