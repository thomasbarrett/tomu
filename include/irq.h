#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

typedef void* irq_arg_t;

typedef void (*irq_line_func) (uint16_t irq, int level, irq_arg_t arg);

#endif
