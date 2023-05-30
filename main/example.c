#define _GNU_SOURCE
#include <asm/bootparam.h>


#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <linux/kvm_para.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <assert.h>
#include <signal.h>    /* signal name macros, and the signal() prototype */
#include <sys/epoll.h>

#include <serial.h>
#include <tty.h>

volatile sig_atomic_t done = 0;

void handle_sigterm(int sig_num) {
    done = 1;
}

typedef struct guest {
  int kvm_fd;
  int vm_fd;
  int vcpu_fd;
  void *mem;
} guest_t;

void kvm_irq_line(uint16_t irq, int level, irq_arg_t irq_arg) {
	guest_t *guest = (guest_t*) irq_arg;
	struct kvm_irq_level irq_level;
	irq_level.irq = irq;
	irq_level.level = level ? 1 : 0;

	if (ioctl(guest->vm_fd, KVM_IRQ_LINE, &irq_level) < 0) {
		perror("KVM_IRQ_LINE ioctl");
        exit(errno);
    }
}

#define GUEST_MEMORY_SIZE (1ULL << 30)
#define KERNEL_CMDLINE_ADDR 0x20000

static int guest_error(guest_t *g, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, ", errno: %d\n", errno);
  va_end(args);
  return -1;
}

static int guest_init_regs(guest_t *g) {
  struct kvm_regs regs;
  struct kvm_sregs sregs;
  if (ioctl(g->vcpu_fd, KVM_GET_SREGS, &(sregs)) < 0) {
    return guest_error(g, "failed to get registers");
  }

  sregs.cs.base = 0;
  sregs.cs.limit = ~0;
  sregs.cs.g = 1;

  sregs.ds.base = 0;
  sregs.ds.limit = ~0;
  sregs.ds.g = 1;

  sregs.fs.base = 0;
  sregs.fs.limit = ~0;
  sregs.fs.g = 1;

  sregs.gs.base = 0;
  sregs.gs.limit = ~0;
  sregs.gs.g = 1;

  sregs.es.base = 0;
  sregs.es.limit = ~0;
  sregs.es.g = 1;

  sregs.ss.base = 0;
  sregs.ss.limit = ~0;
  sregs.ss.g = 1;

  sregs.cs.db = 1;
  sregs.ss.db = 1;
  sregs.cr0 |= 1; /* enable protected mode */

  if (ioctl(g->vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
    return guest_error(g, "failed to set special registers");
  }

  if (ioctl(g->vcpu_fd, KVM_GET_REGS, &(regs)) < 0) {
    return guest_error(g, "failed to get registers");
  }

  regs.rflags = 2;
  regs.rip = 0x100000;
  regs.rsi = 0x10000;

  if (ioctl(g->vcpu_fd, KVM_SET_REGS, &(regs)) < 0) {
    return guest_error(g, "failed to set registers");
  }
  return 0;
}

static int guest_init_cpu_id(guest_t *g) {
  struct {
    uint32_t nent;
    uint32_t padding;
    struct kvm_cpuid_entry2 entries[100];
  } kvm_cpuid;
  kvm_cpuid.nent = sizeof(kvm_cpuid.entries) / sizeof(kvm_cpuid.entries[0]);
  ioctl(g->kvm_fd, KVM_GET_SUPPORTED_CPUID, &kvm_cpuid);

  for (unsigned int i = 0; i < kvm_cpuid.nent; i++) {
    struct kvm_cpuid_entry2 *entry = &kvm_cpuid.entries[i];
    if (entry->function == KVM_CPUID_SIGNATURE) {
      entry->eax = KVM_CPUID_FEATURES;
      entry->ebx = 0x4b4d564b; // KVMK
      entry->ecx = 0x564b4d56; // VMKV
      entry->edx = 0x4d;       // M
    }
  }
  ioctl(g->vcpu_fd, KVM_SET_CPUID2, &kvm_cpuid);
  return 0;
}

int guest_init(guest_t *g) {
    if ((g->kvm_fd = open("/dev/kvm", O_RDWR)) < 0) {
        return guest_error(g, "failed to open /dev/kvm");
    }

    if ((g->vm_fd = ioctl(g->kvm_fd, KVM_CREATE_VM, 0)) < 0) {
        return guest_error(g, "failed to create vm");
    }

    if (ioctl(g->vm_fd, KVM_SET_TSS_ADDR, 0xffffd000) < 0) {
        return guest_error(g, "failed to set tss addr");
    }

    __u64 map_addr = 0xffffc000;
    if (ioctl(g->vm_fd, KVM_SET_IDENTITY_MAP_ADDR, &map_addr) < 0) {
        return guest_error(g, "failed to set identity map addr");
    }

    if (ioctl(g->vm_fd, KVM_CREATE_IRQCHIP, 0) < 0) {
        return guest_error(g, "failed to create irq chip");
    }

    struct kvm_pit_config pit = {
        .flags = 0,
    };
    if (ioctl(g->vm_fd, KVM_CREATE_PIT2, &pit) < 0) {
        return guest_error(g, "failed to create i8254 interval timer");
    }

    g->mem = mmap(NULL, GUEST_MEMORY_SIZE, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g->mem == NULL) {
        return guest_error(g, "failed to mmap vm memory");
    }

    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .flags = 0,
        .guest_phys_addr = 0,
        .memory_size = GUEST_MEMORY_SIZE,
        .userspace_addr = (__u64)g->mem,
    };
    if (ioctl(g->vm_fd, KVM_SET_USER_MEMORY_REGION, &region) < 0) {
        return guest_error(g, "failed to set user memory region");
    }

    if ((g->vcpu_fd = ioctl(g->vm_fd, KVM_CREATE_VCPU, 0)) < 0) {
        return guest_error(g, "failed to create vcpu");
    }

    guest_init_regs(g);
    guest_init_cpu_id(g);

    return 0;
}

int guest_load(guest_t *g, const char *image_path, const char *initrd_path) {
    size_t datasz;
    void *data;
    int fd = open(image_path, O_RDONLY);
    if (fd < 0) {
    return 1;
    }
    struct stat st;
    fstat(fd, &st);
    data = mmap(0, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    datasz = st.st_size;
    close(fd);

    fd = open(initrd_path, O_RDONLY);
    if (fd < 0) {
    return 1;
    }

    fstat(fd, &st);
    void *initramfs = mmap(0, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    size_t initramfs_size = st.st_size;
    close(fd);

    struct boot_params *boot = (struct boot_params *) (((uint8_t *)g->mem) + 0x10000);
    void *kernel = (void *)(((uint8_t *)g->mem) + 0x100000);

    memset(boot, 0, sizeof(struct boot_params));
    memmove(boot, data, sizeof(struct boot_params));
    size_t setup_sectors = boot->hdr.setup_sects;
    size_t setupsz = (setup_sectors + 1) * 512;
    boot->hdr.vid_mode = 0xFFFF; // vga=normal
    boot->hdr.type_of_loader = 0xFF;
    boot->hdr.loadflags |= CAN_USE_HEAP | 0x01 | KEEP_SEGMENTS;
    boot->hdr.heap_end_ptr = 0xFE00;
    boot->hdr.ext_loader_ver = 0x0;
    boot->hdr.cmd_line_ptr = KERNEL_CMDLINE_ADDR;
    memcpy(kernel, (char *)data + setupsz, datasz - setupsz);
    munmap(data, datasz);

    /* load kernel command-line arguments */
    void *cmdline = (void *)(((uint8_t *) g->mem) + KERNEL_CMDLINE_ADDR);
    memset(cmdline, 0, boot->hdr.cmdline_size);
    memcpy(cmdline, "console=ttyS0,9600", 19);

    /* load initramfs to the highest 4k page-aligned address range */
    size_t initramfs_addr = ((GUEST_MEMORY_SIZE - 1) - initramfs_size) & ~(0x0FFFULL);
    memcpy((uint8_t*) g->mem + initramfs_addr, initramfs, initramfs_size);
    boot->hdr.ramdisk_image = initramfs_addr;
    boot->hdr.ramdisk_size = initramfs_size;
    munmap(initramfs, initramfs_size);

    return 0;
}

void guest_deinit(guest_t *g) {
    close(g->kvm_fd);
    close(g->vm_fd);
    close(g->vcpu_fd);
    munmap(g->mem, GUEST_MEMORY_SIZE);
}

int guest_run(guest_t *g) {
    int run_size = ioctl(g->kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    struct kvm_run *run = mmap(0, run_size, PROT_READ | PROT_WRITE, MAP_SHARED, g->vcpu_fd, 0);
    while (!done) {
        if (ioctl(g->vcpu_fd, KVM_RUN, 0) < 0) {
            return guest_error(g, "kvm_run failed");
        }
        
        switch (run->exit_reason) {
        case KVM_EXIT_IO:
            {
                uint8_t *data = (uint8_t *) run + run->io.data_offset;
                size_t len = (size_t) run->io.count * run->io.size;
                if (run->io.direction == KVM_EXIT_IO_OUT) {
                    serial_out(&serial_16550a, run->io.port, data, len);
                } else if (run->io.direction == KVM_EXIT_IO_IN) {
                    serial_in(&serial_16550a, run->io.port, data, len);
                }
                break;
            }
        case KVM_EXIT_SHUTDOWN:
            printf("guest exited: shutdown\n");
            return 0;
        default:
            printf("guest exited: reason: %d\n", run->exit_reason);
            return -1;
        }
    }

    return 0;
}


void* thread1_func(void* arg) {
    serial_t *serial = (serial_t*) arg;

    tty_state_t tty_state;
    if (tty_make_raw(STDIN_FILENO, &tty_state) < 0) {
        perror("failed to enable tty raw mode");
        exit(1);
    }

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("failed to set O_NONBLOCK flag on stdin");
    }

    int eventfd = serial_open(serial);
    if (eventfd < 0) {
        perror("failed to open serial console");
        exit(1);
    }

    int epollfd = epoll_create1(0);
    if (epollfd < 0) {
        perror("epoll_create1");
        exit(1);
    }

    struct epoll_event event;

    event.events = EPOLLIN;
    event.data.fd = eventfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, eventfd, &event) == -1) {
        perror("epoll_ctl: eventfd");
        exit(EXIT_FAILURE);
    }

    event.events = EPOLLIN;
    event.data.fd = STDIN_FILENO;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &event) == -1) {
        perror("epoll_ctl: stdin");
        exit(EXIT_FAILURE);
    }
  
    const size_t MAX_EVENTS = 8;
    struct epoll_event events[MAX_EVENTS];
    while (!done) {
        int n = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait");
            exit(1);
        }
        
        for (int i = 0; i < n; i++) {

            if (events[i].data.fd == eventfd) {
                uint8_t buf[SERIAL_FIFO_LEN];
                int res = serial_read(serial, buf, SERIAL_FIFO_LEN);
                if (res == 0) return NULL;
                if (res > 0) {
                    write(STDOUT_FILENO, buf, res);
                }
                if (res < 0) {
                    if (errno != EAGAIN) {
                        perror("failed to read stdin");
                        exit(1);
                    }
                }
            }

            if (events[i].data.fd == STDIN_FILENO) {
                uint8_t buf[SERIAL_FIFO_LEN];
                int res = read(STDIN_FILENO, buf, SERIAL_FIFO_LEN);
                if (res == 0) return NULL;
                if (res > 0) {
                    serial_write(serial, buf, res);
                }
                if (res < 0) {
                    if (errno != EAGAIN) {
                        perror("failed to read stdin");
                        exit(1);
                    }
                }
            }
        }
    }

    tty_restore(STDIN_FILENO, tty_state);
    close(epollfd);
    serial_close(serial, eventfd);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    guest_t guest;
    
    signal(SIGTERM, handle_sigterm);

    if (guest_init(&guest) < 0) {
        perror("failed to initialize guest");
        goto error0;
    }

    if (serial_init(&serial_16550a, kvm_irq_line, &guest) < 0) {
        perror("failed to initialize serial device");
        goto error1;
    }

    if (guest_load(&guest, argv[1], argv[2]) < 0) {
        perror("failed to load guest");
        goto error2;
    }

    pthread_t thread1;
    if (pthread_create(&thread1, NULL, thread1_func, &serial_16550a) != 0) {
        perror("failed to create thread");
        goto error2;
    }

    guest_run(&guest);

    pthread_join(thread1, NULL);
    serial_deinit(&serial_16550a);
    guest_deinit(&guest);

    return 0;

error2:
    serial_deinit(&serial_16550a);
error1:
    guest_deinit(&guest);
error0:
    return 1;
}
