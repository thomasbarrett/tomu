#ifndef BDEV_H
#define BDEV_H

#include <stddef.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/types.h>

#include <libaio.h>

typedef struct bdev {
    int fd;
    size_t queue_depth;
    struct bdev_queue *queues;
    size_t queue_count;
} bdev_t;

typedef struct bdev_queue {
    bdev_t *bdev;
    int eventfd;
    io_context_t ctx;
} bdev_queue_t;

int bdev_init(bdev_t *bdev, const char *path, size_t queue_count, size_t queue_depth);

void bdev_deinit(bdev_t *bdev);

int bdev_queue_init(bdev_queue_t *queue, bdev_t *bdev);

void bdev_queue_deinit(bdev_queue_t *queue);

bdev_queue_t* bdev_get_queue(bdev_t *bdev, size_t i);

int bdev_queue_eventfd(bdev_queue_t *queue);

typedef void (*bdev_cb_t)(ssize_t res);

int bdev_queue_read(bdev_queue_t *queue, void *buf, size_t count, off_t offset, bdev_cb_t cb);

int bdev_queue_write(bdev_queue_t *queue, void *buf, size_t count, off_t offset, bdev_cb_t cb);

#endif
