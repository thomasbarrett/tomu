#ifndef GUEST_H
#define GUEST_H

typedef struct guest {
  int kvm_fd;
  int vm_fd;
  int vcpu_fd;
  void *mem;
} guest_t;

void kvm_irq_line(guest_t *guest, int irq, int level);

#endif
