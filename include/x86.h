#ifndef X86_H
#define X86_H

#define X86_32BIT_MAX_MEM_SIZE  (1ULL << 32)
#define X86_32BIT_GAP_SIZE	    (1ULL << 30)
#define X86_32BIT_GAP_START	    (X86_32BIT_MAX_MEM_SIZE - X86_32BIT_GAP_SIZE)
#define X86_MMIO_START		    (X86_32BIT_GAP_START)
#define X86_VIRTIO_MMIO_AREA	(X86_MMIO_START + 0x0000000)

#endif
