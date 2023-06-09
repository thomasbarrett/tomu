#include <stdio.h>
#include <sys/epoll.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include <bdev.h>

#define QUEUE_COUNT 4
#define QUEUE_DEPTH 128

volatile int done = 0;

void handle_sigterm(int sig_num) {
    done = 1;
}

void write_done(ssize_t res) {
    printf("write finished with res %d\n", res);
}

void* thread_func(void* arg) {
    bdev_queue_t *queue = arg;
    
    uint8_t chunk[512] __attribute__ ((aligned (512)));
    const size_t MAX_EVENTS = 8;
    struct epoll_event events[MAX_EVENTS];

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
        int n = epoll_wait(epollfd, events, MAX_EVENTS, 100);
        if (n == -1) {
            perror("epoll_wait failed");
            goto error0;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == eventfd) {
                bdev_queue_poll(queue);
            }
        }
    }

    return (void*) ((uintptr_t) 0);

error0:
    return (void*) ((uintptr_t) 1);
}

int main(int argc, char *argv[]) {
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);

    bdev_t bdev = {0};
    if (bdev_init(&bdev, "/dev/nvme0n1", QUEUE_COUNT, 128) < 0) {
        perror("failed to init bdev");
        goto error0;
    }

    pthread_t threads[QUEUE_COUNT];
    for (size_t i = 0; i < QUEUE_COUNT; i++) {
        bdev_queue_t *queue = bdev_get_queue(&bdev, i);
        if (pthread_create(&threads[i], NULL, thread_func, queue) != 0) {
            perror("failed to create thread");
            exit(1);
        }
    }
    
    for (size_t i = 0; i < QUEUE_COUNT; i++) {
        void *thread_res;
        int res = pthread_join(threads[i], &thread_res);
        if (res != 0) {
            perror("failed to join thread");
            exit(1);
        }

        if ((uintptr_t) thread_res != 0) {
            perror("thread exited with error");
            exit(1);
        }
    }

error1:
    bdev_deinit(&bdev);

error0:
    return 1;
}
