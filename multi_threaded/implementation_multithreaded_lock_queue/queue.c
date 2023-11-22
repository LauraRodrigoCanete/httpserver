#include <stdio.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include "queue.h"
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

struct queue {
    void **buffer;
    int maxSize;
    int in;
    int out;
    int count;
    pthread_mutex_t mutexLock;
    pthread_cond_t empty, full;
};

// void printqueue(queue_t *q){
//     pthread_mutex_lock(&q->mutexLock); // Lock the queue for safe access
//     int i = q->out;
//     int itemsPrinted = 0;

//     while(itemsPrinted < q->count){
//         printf("%ld ", (intptr_t)q->buffer[i]); // Cast to intptr_t to print as integer
//         i = (i + 1) % q->maxSize; // Move to the next element
//         itemsPrinted++;
//     }
//     printf("\n");
//     pthread_mutex_unlock(&q->mutexLock); // Unlock the queue
// }

queue_t *queue_new(int size) {
    queue_t *q = (queue_t *) calloc(1, sizeof(queue_t));
    if (!q) {
        return NULL;
    }
    q->maxSize = size;
    q->buffer = (void **) calloc(size, sizeof(void *));
    if (!q->buffer) {
        free(q);
        return NULL;
    }

    pthread_mutex_init(&q->mutexLock, NULL);
    pthread_cond_init(&q->empty, NULL);
    pthread_cond_init(&q->full, NULL);

    return q;
}

void queue_delete(queue_t **q) {
    pthread_mutex_destroy(&(*q)->mutexLock);
    pthread_cond_destroy(&(*q)->empty);
    pthread_cond_destroy(&(*q)->full);
    free((*q)->buffer);
    free(*q);
    *q = NULL;
}

bool queue_push(queue_t *q, void *elem) {
    pthread_mutex_lock(&q->mutexLock);
    if (q == NULL)
        return false;
    while (q->count == q->maxSize) {
        pthread_cond_wait(&q->full, &q->mutexLock);
    }
    q->buffer[q->in] = elem;
    q->in = (q->in + 1) % q->maxSize;
    q->count++;
    pthread_cond_signal(&q->empty);
    pthread_mutex_unlock(&q->mutexLock);
    return true;
}

bool queue_pop(queue_t *q, void **elem) {
    pthread_mutex_lock(&q->mutexLock);
    if (q == NULL)
        return false;
    while (q->count == 0) {
        pthread_cond_wait(&q->empty, &q->mutexLock);
    }
    *elem = q->buffer[q->out];
    q->out = (q->out + 1) % q->maxSize;
    q->count -= 1;
    pthread_cond_signal(&q->full);
    pthread_mutex_unlock(&q->mutexLock);
    return true;
}
