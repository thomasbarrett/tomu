#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>

#include <linux/kvm.h>

#include <guest.h>

void kvm_irq_line(guest_t *guest, int irq, int level) {
	struct kvm_irq_level irq_level;
	irq_level.irq = irq;
	irq_level.level = level ? 1 : 0;

	if (ioctl(guest->vm_fd, KVM_IRQ_LINE, &irq_level) < 0) {
		perror("KVM_IRQ_LINE ioctl");
        exit(errno);
    }
}
