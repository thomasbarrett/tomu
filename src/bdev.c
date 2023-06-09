#define _GNU_SOURCE

#include <bdev.h>

#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/eventfd.h>

#include <libaio.h>

int bdev_init(bdev_t *bdev, const char *path, size_t queue_count, size_t queue_depth) {
    int res = open(path, O_DIRECT | O_RDWR);
    if (res < 0) {
        goto error0;
    }
    bdev->fd = res;
    bdev->queue_depth = queue_depth;
    bdev->queues = (bdev_queue_t*) calloc(queue_count, sizeof(bdev_queue_t));
    if (bdev->queues == NULL) {
        goto error1;
    }

    for (bdev->queue_count = 0; bdev->queue_count < queue_count; bdev->queue_count++) {
        if (bdev_queue_init(&bdev->queues[bdev->queue_count], bdev) < 0) {
            goto error2;
        }
    }

    return 0;

error2:
    for (size_t i = 0; i < bdev->queue_count; i++) {
        bdev_queue_deinit(&bdev->queues[i]);
    }
    free(bdev->queues);
error1:
    close(bdev->fd);
error0:
    return -1;
}

void bdev_queue_deinit(bdev_queue_t *queue) {
    close(queue->eventfd);
    io_destroy(queue->ctx);
}

int bdev_queue_init(bdev_queue_t *queue, bdev_t *bdev) {
    int res = io_setup(bdev->queue_depth, &queue->ctx);
    if (res < 0) {
        errno = -res;
        goto error0;
    }

    queue->eventfd = eventfd(0, EFD_NONBLOCK);
    if (queue->eventfd < 0) {
        goto error1;
    }

    queue->bdev = bdev;

    return 0;

error1:
    io_destroy(queue->ctx);
error0:
    return -1;
}

void bdev_deinit(bdev_t *bdev) {
    close(bdev->fd);
    bdev->fd = -1;
}

bdev_queue_t* bdev_get_queue(bdev_t *bdev, size_t i) {
    return &bdev->queues[i];
}

int bdev_queue_eventfd(bdev_queue_t *queue) {
    return queue->eventfd;
}

int bdev_queue_read(bdev_queue_t *queue, void *buf, size_t count, off_t offset, bdev_cb_t cb) {
    struct iocb io = {0};
    io_prep_pread(&io, queue->bdev->fd, buf, count, offset);
    io_set_eventfd(&io, queue->eventfd);
    io_set_callback(&io, cb);

    struct iocb *ios[1] = {&io};
    int res = io_submit(queue->ctx, 1, ios);
    if (res < 0) {
        errno = -res;
        return -1;
    }

    return 0;
}

int bdev_queue_write(bdev_queue_t *queue, void *buf, size_t count, off_t offset, bdev_cb_t cb) {
    struct iocb io = {0};
    io_prep_pwrite(&io, queue->bdev->fd, buf, count, offset);
    io_set_eventfd(&io, queue->eventfd);
    io_set_callback(&io, cb);

    struct iocb *ios[1] = {&io};
    int res = io_submit(queue->ctx, 1, ios);
    if (res < 0) {
        errno = -res;
        return -1;
    }

    return 0;
}

int bdev_queue_poll(bdev_queue_t *queue) {
    struct io_event cqe[128];
    struct timespec timeout = (struct timespec) {
        .tv_sec = 0,
        .tv_nsec = 100000000
    };
    int res = io_getevents(queue->ctx, 1, 128, cqe, &timeout);
    if (res < 0) {
        errno = -res;
        return -1;
    }

    for (int j = 0; j < res; j++) {
        ((bdev_cb_t) cqe[j].data)(cqe[j].res);
    }

    return 0;
}