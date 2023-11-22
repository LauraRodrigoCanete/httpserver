#include <stdio.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include "rwlock.h"
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

struct rwlock {
    pthread_mutex_t lock;
    pthread_cond_t readers_cond;
    pthread_cond_t writers_cond;
    int active_readers;
    int active_writers;
    int waiting_readers;
    int waiting_writers;
    PRIORITY priority;
    int n_way_count;
    int n_way_limit;
};

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    rwlock_t *rw = (rwlock_t *) calloc(1, sizeof(rwlock_t));
    rw->priority = p;
    rw->n_way_limit = n;
    pthread_mutex_init(&(rw->lock), NULL);
    pthread_cond_init(&(rw->readers_cond), NULL);
    pthread_cond_init(&(rw->writers_cond), NULL);
    return rw;
}

void rwlock_delete(rwlock_t **rw) {
    pthread_mutex_destroy(&((*rw)->lock));
    pthread_cond_destroy(&((*rw)->readers_cond));
    pthread_cond_destroy(&((*rw)->writers_cond));
    free(*rw);
    *rw = NULL;
}

void reader_lock(rwlock_t *rw) {
    pthread_mutex_lock(&(rw->lock));
    rw->waiting_readers++;
    while (rw->active_writers > 0) {
        pthread_cond_wait(&(rw->readers_cond), &(rw->lock));
    }
    if (rw->priority == WRITERS) {
        while (rw->active_writers > 0 || rw->waiting_writers > 0) {
            pthread_cond_wait(&(rw->readers_cond), &(rw->lock));
        }
    } else if (rw->priority == N_WAY) {
        while (rw->active_writers > 0
               || (rw->n_way_count >= rw->n_way_limit && rw->waiting_writers > 0)) {
            pthread_cond_wait(&(rw->readers_cond), &(rw->lock));
        }
        rw->n_way_count++;
    }
    rw->active_readers++;
    rw->waiting_readers--;
    pthread_mutex_unlock(&(rw->lock));
}

void reader_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&(rw->lock));
    rw->active_readers--;
    if (rw->active_readers == 0) {
        if (rw->priority == READERS) {
            if (rw->waiting_readers > 0) {
                pthread_cond_broadcast(&(rw->readers_cond));
            } else {
                pthread_cond_signal(&(rw->writers_cond));
            }
        } else if (rw->priority == WRITERS) {
            if (rw->waiting_writers > 0) {
                pthread_cond_signal(&(rw->writers_cond));
            } else {
                pthread_cond_signal(&(rw->readers_cond));
            }
        } else {
            if (rw->n_way_count < rw->n_way_limit) {
                if (rw->waiting_readers > 0) {
                    pthread_cond_signal(&(rw->readers_cond));
                } else {
                    pthread_cond_signal(&(rw->writers_cond));
                }
            } else {
                if (rw->waiting_writers == 0) {
                    if (rw->waiting_readers > 0) {
                        pthread_cond_signal(&(rw->readers_cond));
                    }
                } else {
                    pthread_cond_signal(&(rw->writers_cond));
                }
            }
        }
    }
    pthread_mutex_unlock(&(rw->lock));
}

void writer_lock(rwlock_t *rw) {
    pthread_mutex_lock(&(rw->lock));
    rw->waiting_writers++;
    while (rw->active_readers > 0 || rw->active_writers > 0) {
        pthread_cond_wait(&(rw->writers_cond), &(rw->lock));
    }
    if (rw->priority == READERS) {
        while (rw->active_readers > 0 || rw->active_writers > 0 || rw->waiting_readers > 0) {
            pthread_cond_wait(&(rw->writers_cond), &(rw->lock));
        }
    } else if (rw->priority == N_WAY) {
        while (rw->active_readers > 0 || rw->active_writers > 0
               || (rw->n_way_count < rw->n_way_limit && rw->waiting_readers > 0)) {
            pthread_cond_wait(&(rw->writers_cond), &(rw->lock));
        }
    }
    rw->active_writers++;
    rw->waiting_writers--;
    pthread_mutex_unlock(&(rw->lock));
}

void writer_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&(rw->lock));
    rw->active_writers--;
    if (rw->priority == READERS) {
        if (rw->waiting_readers > 0) {
            pthread_cond_broadcast(&(rw->readers_cond));
        } else {
            pthread_cond_signal(&(rw->writers_cond));
        }
    } else if (rw->priority == WRITERS) {
        if (rw->waiting_writers > 0) {
            pthread_cond_signal(&(rw->writers_cond));
        } else {
            pthread_cond_signal(&(rw->readers_cond));
        }
    } else {
        rw->n_way_count = 0; //pensar si va aqui
        if (rw->waiting_readers > 0) {
            pthread_cond_signal(&(rw->readers_cond));
        } else {
            pthread_cond_signal(&(rw->writers_cond));
        }
    }
    pthread_mutex_unlock(&(rw->lock));
}
