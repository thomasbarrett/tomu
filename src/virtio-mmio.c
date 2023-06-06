#include <virtio-mmio.h>
#include <irq.h>

#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>

#include <sys/uio.h>

#include <linux/virtio_mmio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_blk.h>
#include <linux/virtio_ring.h>

void virtio_mmio_add_feature(virtio_mmio_config_t *cfg, uint32_t flag) {
        cfg->device_features[flag / 32] |= 1 << (flag % 32);
}

struct virtio_blk_config config = {
    .capacity = 2097152,
};

void virtio_mmio_config_init(virtio_mmio_config_t *cfg, void *mem, irq_line_func irq_line, irq_arg_t irq_arg) {
    memset(cfg, 0, sizeof(virtio_mmio_config_t));
    cfg->mem = mem;
    cfg->irq_line = irq_line;
    cfg->irq_arg = irq_arg;
    cfg->irq = 5;
    cfg->device_id = VIRTIO_DEVICE_BLOCK;
    cfg->device_features_len = 2;
    cfg->driver_features_len = 2;
    for (size_t i = 0; i < VIRTIO_QUEUE_COUNT; i++) {
        cfg->queues[i].num_max = VIRTIO_BLK_QUEUE_DEPTH;
    }
    // if (bdev_init(&cfg->bdev, "/dev/nvme0n1") < 0) {
    //     perror("failed to open block device\r\n");
    //     exit(1);
    // }
    virtio_mmio_add_feature(cfg, VIRTIO_F_VERSION_1);
    //virtio_mmio_add_feature(cfg, VIRTIO_BLK_F_MQ);
    // virtio_mmio_add_feature(cfg, VIRTIO_F_RING_PACKED);
    // virtio_mmio_add_feature(cfg, VIRTIO_BLK_F_SIZE_MAX);
    // virtio_mmio_add_feature(cfg, VIRTIO_BLK_F_SEG_MAX);
    // virtio_mmio_add_feature(cfg, VIRTIO_BLK_F_GEOMETRY);
    // virtio_mmio_add_feature(cfg, VIRTIO_BLK_F_BLK_SIZE);
    // virtio_mmio_add_feature(cfg, VIRTIO_BLK_F_FLUSH);
    // virtio_mmio_add_feature(cfg, VIRTIO_BLK_F_TOPOLOGY);
    // virtio_mmio_add_feature(cfg, VIRTIO_BLK_F_DISCARD);
    // virtio_mmio_add_feature(cfg, VIRTIO_BLK_F_WRITE_ZEROES);
}

void virtio_mmio_read(virtio_mmio_config_t *cfg, uintptr_t guest_phys_addr, void *data, size_t size) {
    if (guest_phys_addr >= X86_VIRTIO_MMIO_AREA + VIRTIO_MMIO_CONFIG && 
        guest_phys_addr < X86_VIRTIO_MMIO_AREA + VIRTIO_MMIO_CONFIG + sizeof(struct virtio_blk_config) &&
        guest_phys_addr + size < X86_VIRTIO_MMIO_AREA + VIRTIO_MMIO_CONFIG + sizeof(struct virtio_blk_config)) {
        memcpy(data, &((uint8_t*)&config)[guest_phys_addr - X86_VIRTIO_MMIO_AREA - VIRTIO_MMIO_CONFIG], size);
    }
    if (size != sizeof(uint32_t)) return;
     if (guest_phys_addr < X86_VIRTIO_MMIO_AREA) return;
    switch (guest_phys_addr - X86_VIRTIO_MMIO_AREA) {
        case VIRTIO_MMIO_MAGIC_VALUE:
            *((uint32_t*) data) = VIRTIO_MMIO_MAGIC;
            break;
        case VIRTIO_MMIO_VERSION:
            *((uint32_t*) data) = VIRTIO_VERSION;
            break;
        case VIRTIO_MMIO_DEVICE_ID:
            *((uint32_t*) data) = cfg->device_id;
            break;
        case VIRTIO_MMIO_VENDOR_ID:
            *((uint32_t*) data) = cfg->vendor_id;
            break;
        case VIRTIO_MMIO_DEVICE_FEATURES:
            if (cfg->device_features_sel < cfg->device_features_len) {
                *((uint32_t*) data) = cfg->device_features[cfg->device_features_sel];
            } else {
                *((uint32_t*) data) = 0x00000000;
            }
            break;
        case VIRTIO_MMIO_QUEUE_NUM_MAX:
            if (cfg->queue_sel < VIRTIO_QUEUE_COUNT) {
                *((uint32_t*) data) = cfg->queues[cfg->queue_sel].num_max;
            } else {
                *((uint32_t*) data) = 0;
            }
            break;
        case VIRTIO_MMIO_QUEUE_READY:
            if (cfg->queue_sel < VIRTIO_QUEUE_COUNT) {
                *((uint32_t*) data) = cfg->queues[cfg->queue_sel].ready;
            } else {
                *((uint32_t*) data) = 0;
            }
            break;
        case VIRTIO_MMIO_INTERRUPT_STATUS:
            *((uint32_t*) data) = cfg->interrupt_status;
            break;
    }
}

void virtio_mmio_write(virtio_mmio_config_t *cfg, uintptr_t guest_phys_addr, void *data, size_t size) {
    if (size != sizeof(uint32_t)) return;
    if (guest_phys_addr < X86_VIRTIO_MMIO_AREA) return;
    switch (guest_phys_addr - X86_VIRTIO_MMIO_AREA) {
        case VIRTIO_MMIO_DEVICE_FEATURES_SEL:
            cfg->device_features_sel = *((uint32_t*) data);
            break;
        case VIRTIO_MMIO_DRIVER_FEATURES:
            if (cfg->driver_features_sel < cfg->driver_features_len) {
                cfg->driver_features[cfg->driver_features_sel] = *((uint32_t*) data);
            }
            break;
        case VIRTIO_MMIO_DRIVER_FEATURES_SEL:
            cfg->driver_features_sel = *((uint32_t*) data);
            break;
        case VIRTIO_MMIO_QUEUE_SEL:
            cfg->queue_sel = *((uint32_t*) data);
            break;
        case VIRTIO_MMIO_QUEUE_NUM:
            if (cfg->queue_sel < VIRTIO_QUEUE_COUNT) {
                cfg->queues[cfg->queue_sel].vring.num = *((uint32_t*) data);
            }
            break;
        case VIRTIO_MMIO_QUEUE_READY:
            if (cfg->queue_sel < VIRTIO_QUEUE_COUNT) {
                cfg->queues[cfg->queue_sel].ready = *((uint32_t*) data);
            }
            break;
        case VIRTIO_MMIO_QUEUE_NOTIFY:
        {
            //uint32_t queue_id = *(uint32_t*)data;
            virt_queue_t *queue = &cfg->queues[cfg->queue_sel];
            uintptr_t desc_table = ((uintptr_t) queue->desc_hi << 32 ) | queue->desc_lo;
            uintptr_t avail_ring = ((uintptr_t) queue->avail_hi << 32 ) | queue->avail_lo;
            uintptr_t used_ring = ((uintptr_t) queue->used_hi << 32 ) | queue->used_lo;
            queue->vring.desc = (struct vring_desc*) ((uint8_t*) cfg->mem + desc_table);
            queue->vring.avail = (struct vring_avail*) ((uint8_t*) cfg->mem + avail_ring);
            queue->vring.used = (struct vring_used*) ((uint8_t*) cfg->mem + used_ring);
            
            // virt-queue synchronization between the guest and host is performed in a
            // lock-free manner using memory fences. See sections 2.6.13 and 2.6.14 of
            // the virtio 1.1 specification for more details.
            //
            //           Guest            |           Host
            //                            |
            // avail.ring[i] = next       | while(avail.idx != j) {
            // fence(acquire)             |     fence(release)
            // avail.idx = ++i            |     next = avail.ring[j++]
            //                            |     ...
            //                            | }
            while(queue->vring.avail->idx != queue->last_avail_idx) {
                atomic_thread_fence(memory_order_acquire);
                uint32_t buffer_id = queue->vring.avail->ring[queue->last_avail_idx++ % queue->vring.num];

                struct iovec iov[VIRTIO_BLK_QUEUE_DEPTH];
                size_t n = 0;

                uint32_t iter = buffer_id;
                while (queue->vring.desc[iter].flags & VRING_DESC_F_NEXT) {
                    if (n == VIRTIO_BLK_QUEUE_DEPTH) {
                        // printf("invalid next\r\n");
                        // fflush(stdout);
                        exit(1);
                    }

                    iov[n++] = (struct iovec) {
                        .iov_base =  (uint8_t*) cfg->mem + queue->vring.desc[iter].addr,
                        .iov_len = queue->vring.desc[iter].len
                    };
                    
                    iter = queue->vring.desc[iter].next;
                    if (iter >= VIRTIO_BLK_QUEUE_DEPTH) {
                        printf("invalid next\r\n");
                        fflush(stdout);
                        exit(1);
                    }
                }
                iov[n++] = (struct iovec) {
                    .iov_base =  (uint8_t*) cfg->mem + queue->vring.desc[iter].addr,
                    .iov_len = queue->vring.desc[iter].len
                };

                struct virtio_blk_outhdr* hdr = (struct virtio_blk_outhdr*) iov[0].iov_base;
                switch (hdr->type) {
                case VIRTIO_BLK_T_IN:
                   // bdev_read(&cfg->bdev, iov[1].iov_base, iov[1].iov_len, hdr->sector * 512);
                  //  printf("read [%04x] of size [%04x]\r\n", hdr->sector, iov[1].iov_len);
                   // fflush(stdout);
                    break;
                case VIRTIO_BLK_T_OUT:
                  //  bdev_write(&cfg->bdev, iov[1].iov_base, iov[1].iov_len, hdr->sector * 512);
                   // printf("write [%04x] of size [%04x]\r\n", hdr->sector, iov[1].iov_len);
                   // fflush(stdout);
                    break;
                default:
                  //  printf("unknown blk io type %d\r\n", hdr->type);
                  //  fflush(stdout);
                  break;
                }
                //printf("completing io %d %d %d\r\n", buffer_id, n, iov[n - 1].iov_len);
                fflush(stdout);
                *((uint8_t*) iov[n - 1].iov_base) = VIRTIO_BLK_S_OK;
                queue->vring.used->ring[queue->vring.used->idx] = (vring_used_elem_t) {
                    .id = buffer_id,
                    .len = n,
                };
                atomic_thread_fence(memory_order_release);
                queue->vring.used->idx++;

                cfg->interrupt_status |= VIRTIO_MMIO_INT_VRING;
                cfg->irq_line(cfg->irq, 1, cfg->irq_arg);
	            cfg->irq_line(cfg->irq, 0, cfg->irq_arg);
            }
            
            // TODO: Writing a value to this register notifies the
            // device that there are new buffers to process in a queue.
            // When VIRTIO_F_NOTIFICATION_DATA has not been negotiated,
            // the value written is the queue index.
            break;
        }
        case VIRTIO_MMIO_INTERRUPT_ACK:
            cfg->interrupt_status &= ~*((uint32_t*) data);
            break;
        case VIRTIO_MMIO_STATUS:
            {
                uint32_t value = *((uint32_t*) data);
                if (value) {
                    cfg->status = value;
                } else {
                    virtio_mmio_reset(cfg);
                }
            }
            break;
        case VIRTIO_MMIO_QUEUE_DESC_LOW:
            if (cfg->queue_sel < VIRTIO_QUEUE_COUNT) {
                cfg->queues[cfg->queue_sel].desc_lo = *((uint32_t*) data);
            }
            break;
        case VIRTIO_MMIO_QUEUE_DESC_HIGH:
            if (cfg->queue_sel < VIRTIO_QUEUE_COUNT) {
                cfg->queues[cfg->queue_sel].desc_hi = *((uint32_t*) data);
            }
            break;
        case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
            if (cfg->queue_sel < VIRTIO_QUEUE_COUNT) {
                cfg->queues[cfg->queue_sel].avail_lo = *((uint32_t*) data);
            }
            break;
        case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
            if (cfg->queue_sel < VIRTIO_QUEUE_COUNT) {
                cfg->queues[cfg->queue_sel].avail_hi = *((uint32_t*) data);
            }
            break;
        case VIRTIO_MMIO_QUEUE_USED_LOW:
            if (cfg->queue_sel < VIRTIO_QUEUE_COUNT) {
                cfg->queues[cfg->queue_sel].used_lo = *((uint32_t*) data);
            }
            break;
        case VIRTIO_MMIO_QUEUE_USED_HIGH:
            if (cfg->queue_sel < VIRTIO_QUEUE_COUNT) {
                cfg->queues[cfg->queue_sel].used_hi = *((uint32_t*) data);
            }
            break;
    }
}

void virtio_mmio_reset(virtio_mmio_config_t *cfg) {
    printf("virtio-mmio device reset\r\n");
    fflush(stdout);
    // TODO: Reset appropriately
}
