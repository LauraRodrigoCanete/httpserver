#include "queue.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c = PTHREAD_COND_INITIALIZER;
int ops = 0;
queue_t *q = NULL;

void pop_check(queue_t *q, void *elem) {
  bool rtn = queue_pop(q, elem);
  if (!rtn) {
    fprintf(stderr, "queue_pop failed!\n");
    exit (1);
  }
}

void push_check(queue_t *q, void *elem) {
  bool rtn = queue_push(q, elem);
  if (!rtn) {
    fprintf(stderr, "queue_push failed!\n");
    exit (1);
  }
}

void *thread1() {
  intptr_t i = 1;
  while(i< 200000){
    printf("1");
    push_check(q, (void *)i);
    i = i+2;
  }
  return 0;
}


void *thread2() {
  intptr_t i = 0; //OJOOO
  while(i< 200000){
    printf("2");
    push_check(q, (void *)i);
    i = i+2;
  }
  return 0;
}

int main() {
  q = queue_new(210000);
  if (q == NULL) {
    return 1;
  }
  pthread_t t1, t2;
  uintptr_t rc;
  bool rtn;

  pthread_create(&t1, NULL, thread1, NULL);
  pthread_create(&t2, NULL, thread2, NULL);

  pthread_join(t1, (void **)&rc);
  if (rc) {
    fprintf(stderr, "thread_join failed\n!");
    return 1;
  }

  pthread_join(t2, (void **)&rc);
  if (rc) {
    fprintf(stderr, "thread_join failed\n!");
    return 1;
  }
  //printqueue(q);
  uintptr_t next_odd = 1, next_even = 0;
  uintptr_t failed = 0;
  //printf("checking\n");
  
  for (int i = 0; i < 200000; ++i) {
    rtn = queue_pop(q, (void **)&rc);
    //printf("%lu\n", rc);

    if (!(rc % 2) && rc != next_even) {
      // we got an even number, but it wasn't the one we expected!
      fprintf(stderr, "(pos %d): I didn't expect %lu\n", i, rc);
      failed = 1;
    }
    else if (rc % 2 && rc != next_odd) {
      // we got an even number, but it wasn't the one we expected!
      fprintf(stderr, "*pos %d): I didn't expect %lu\n", i, rc);
      failed = 1;
    }
    else if ( !(rc %2)) {
      next_even += 2;
    }
    else if (rc %2) {
      next_odd += 2;
    }
  }
  //printf("we ended");
  return failed;
}
