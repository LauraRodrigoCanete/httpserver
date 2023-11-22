#include "rwlock.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 10

rwlock_t *rwlock;

// Reader thread function
void *reader_thread(void *arg) {
    intptr_t id = (intptr_t)arg;

    // Attempt to acquire a reader lock
    reader_lock(rwlock);
    printf("Reader %ld acquired the lock.\n", id);
    //sleep(1); // Simulate some read operations
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
    //sleep(1); // Simulate some write operations
    printf("Writer %ld releasing the lock.\n", id);
    writer_unlock(rwlock);
    
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    rwlock = rwlock_new(READERS, 2);

    if (rwlock == NULL) {
        fprintf(stderr, "Failed to create the rwlock.\n");
        return 1;
    }

    for (intptr_t i = NUM_THREADS / 2; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, writer_thread, (void *)i) != 0) {
            perror("Failed to create the writer thread");
            rwlock_delete(&rwlock);
            return 1;
        }
    }
    
    //Create reader threads
    for (intptr_t i = 0; i < NUM_THREADS / 2; i++) {
        if (pthread_create(&threads[i], NULL, reader_thread, (void *)i) != 0) {
            perror("Failed to create the reader thread");
            rwlock_delete(&rwlock);
            return 1;
        }
    }
    //sleep(1);
    // Create writer threads
    

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
