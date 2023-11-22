#include "rwlock.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 6

rwlock_t *rwlock;

// Reader thread function
void *reader_thread(void *arg) {
    intptr_t id = (intptr_t)arg;

    // Attempt to acquire a reader lock
    reader_lock(rwlock);
    printf("Reader %ld acquired the lock.\n", id);
    sleep(4); // Simulate some read operations
    printf("Reader %ld releasing the lock.\n", id);
    reader_unlock(rwlock);

    return NULL;
}

// Writer thread function
void *writer_thread(void *arg) {
    intptr_t id = (intptr_t)arg;

    // Attempt to acquire a writer lock
    writer_lock(rwlock);
    printf("Writer %ld acquired the lock.\n", id);
    sleep(3); // Simulate some write operations
    printf("Writer %ld releasing the lock.\n", id);
    writer_unlock(rwlock);
    
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    rwlock = rwlock_new(N_WAY, 2);

    if (rwlock == NULL) {
        fprintf(stderr, "Failed to create the rwlock.\n");
        return 1;
    }
    pthread_create(&threads[0], NULL, writer_thread, (void *)0);
    sleep(1);
    pthread_create(&threads[1], NULL, reader_thread, (void *)1);
    pthread_create(&threads[2], NULL, reader_thread, (void *)2);
    pthread_create(&threads[3], NULL, reader_thread, (void *)3);
    pthread_create(&threads[4], NULL, writer_thread, (void *)4);
    pthread_create(&threads[5], NULL, writer_thread, (void *)5);
    //the first writer releases and two of the readers go
    //one of the readers erleases
    //the other releases
    //the lock should be given to one of the writers
    //the writer releases
    //the lock is given to the remaining reader
    //the reader releases
    //the lock is given to last writer

    // Create reader threads
    // for (intptr_t i = 0; i < NUM_THREADS / 2; i++) {
    //     if (pthread_create(&threads[i], NULL, reader_thread, (void *)i) != 0) {
    //         perror("Failed to create the reader thread");
    //         rwlock_delete(&rwlock);
    //         return 1;
    //     }
    // }

    // // Create writer threads
    // for (intptr_t i = NUM_THREADS / 2; i < NUM_THREADS; i++) {
    //     if (pthread_create(&threads[i], NULL, writer_thread, (void *)i) != 0) {
    //         perror("Failed to create the writer thread");
    //         rwlock_delete(&rwlock);
    //         return 1;
    //     }
    // }

    // Join all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Failed to join the thread");
            rwlock_delete(&rwlock);
            return 1;
        }
    }

    rwlock_delete(&rwlock);
    printf("All threads have finished.\n");
    return 0;
}
