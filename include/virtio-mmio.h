#ifndef VIRTIO_MMIO_H
#define VIRTIO_MMIO_H
#include <x86.h>
#include <irq.h>

#include <stdint.h>
#include <stddef.h>
#include <linux/virtio_ring.h>

#define VIRTIO_MMIO_MAGIC 0x74726976
#define VIRTIO_VERSION 0x2

#define VIRTIO_DEVICE_NETWORK_CARD 1
#define VIRTIO_DEVICE_BLOCK 2
#define VIRTIO_DEVICE_CONSOLE 3
#define VIRTIO_DEVICE_ENTROPY_SOURCE 4
#define VIRTIO_DEVICE_MEMORY_BALLOON 5

#define VIRTIO_QUEUE_COUNT 1
#define VIRTIO_BLK_QUEUE_DEPTH 128
typedef struct virt_queue {
    struct vring vring;
    uint32_t last_avail_idx;
    uint32_t num_max;
    uint32_t ready;
    uint32_t desc_lo;
    uint32_t desc_hi;
    uint32_t avail_lo;
    uint32_t avail_hi;
    uint32_t used_lo;
    uint32_t used_hi;
} virt_queue_t;

typedef struct virtio_mmio_config {
	uint32_t device_features_sel;
    uint32_t driver_features_sel;
    uint32_t queue_sel;
    uint32_t queue_ready;
    uint32_t status;
    virt_queue_t queues[VIRTIO_QUEUE_COUNT];
    void *mem;

    uint32_t device_id;
    uint32_t vendor_id;
    uint32_t device_features[2];
    uint32_t device_features_len;
    uint32_t driver_features[2];
    uint32_t driver_features_len;
    uint32_t interrupt_status;
    
    irq_line_func irq_line;
    irq_arg_t irq_arg;
    uint32_t irq;
} virtio_mmio_config_t;

#define VIRTIO_MMIO_IO_SIZE	512

void virtio_mmio_config_init(virtio_mmio_config_t *cfg, void *mem, irq_line_func irq_line, irq_arg_t irq_arg);
void virtio_mmio_read(virtio_mmio_config_t *cfg, uintptr_t guest_phys_addr, void *data, size_t size);
void virtio_mmio_write(virtio_mmio_config_t *cfg, uintptr_t guest_phys_addr, void *data, size_t size);
void virtio_mmio_reset(virtio_mmio_config_t *cfg);
#endif
