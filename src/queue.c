#include <queue.h>
#include <stdlib.h>

int queue_init(queue_t *q, size_t capacity) {
    q->front = 0;
    q->rear = 0;
    q->size = 0;
    q->capacity = capacity;
    q->data = calloc(1, capacity);
    if (q->data == NULL) return -1;
    return 0;
}

void queue_deinit(queue_t *q) {
    q->front = 0;
    q->rear = 0;
    q->size = 0;
    q->capacity = 0;
    free(q->data);
    q->data = NULL;
}

void queue_clear(queue_t *q) {
    q->rear = 0;
    q->front = 0;
    q->size = 0;
}

int queue_push(queue_t *q, uint8_t b) {
    if (q->capacity == 0) return -1;
    if (q->size == q->capacity) return -1;
    q->data[q->rear] = b;
    q->size++;
    q->rear = (q->rear + 1) % q->capacity;
    return 0;
}

int queue_pop(queue_t *q, uint8_t *b) {
    if (q->size == 0) return -1;
    *b = q->data[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;
    return 0;
}

size_t queue_size(queue_t *q) {
    return q->size;
}

size_t queue_capacity(queue_t *q) {
    return q->capacity;
}
