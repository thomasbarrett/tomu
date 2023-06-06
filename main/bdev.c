#include <stdio.h>
#include <sys/epoll.h>
#include <errno.h>

#include <bdev.h>

volatile int done = 0;

void write_done(ssize_t res) {
    printf("write finished with res %d\n", res);
}

int main(int argc, char *argv[]) {
    uint8_t chunk[512] __attribute__ ((aligned (512)));
    const size_t MAX_EVENTS = 8;
    struct epoll_event events[MAX_EVENTS];

    bdev_t bdev = {0};
    if (bdev_init(&bdev, "/dev/nvme0n1", 4, 128) < 0) {
        perror("failed to init bdev");
        goto error0;
    }

    bdev_queue_t *queue = bdev_get_queue(&bdev, 1);
    int eventfd = bdev_queue_eventfd(queue);

    int res = bdev_queue_write(queue, &chunk, 512, 0, write_done);
    if (res < 0) {
        perror("bdev_queue_write failed");
        goto error0;
    }

    int epollfd = epoll_create1(0);
    if (epollfd < 0) {
        perror("epoll_create failed");
        goto error0;
    }

    struct epoll_event event;

    event.events = EPOLLIN;
    event.data.fd = eventfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, eventfd, &event) == -1) {
        perror("epoll_ctl failed");
        goto error0;
    }
    
    while (!done) {
        int n = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait failed");
            goto error0;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == eventfd) {
                struct io_event cqe[128];
                int res = io_getevents(queue->ctx, 1, 128, cqe, NULL);
                if (res < 0) {
                    errno = -res;
                    perror("io_getevents failed");
                    goto error0;
                }

                for (int j = 0; j < res; j++) {
                    ((bdev_cb_t) cqe[j].data)(cqe[j].res);
                }
            }
        }
    }

    return 0;
    
error0:
    bdev_deinit(&bdev);
    return 1;
}
